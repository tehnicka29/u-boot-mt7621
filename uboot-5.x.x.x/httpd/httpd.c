#include "uip.h"
#include "httpd.h"
#include "fs.h"
#include "fsdata.h"

#define STATE_NONE				0		// empty state (waiting for request...)
#define STATE_FILE_REQUEST		1		// remote host sent GET request
#define STATE_UPLOAD_REQUEST	2		// remote host sent POST request

// ASCII characters
#define ISO_G					0x47	// GET
#define ISO_E					0x45
#define ISO_T					0x54
#define ISO_P					0x50	// POST
#define ISO_O					0x4f
#define ISO_S					0x53
#define ISO_T					0x54
#define ISO_slash				0x2f	// control and other characters
#define ISO_space				0x20
#define ISO_nl					0x0a
#define ISO_cr					0x0d
#define ISO_tab					0x09

// we use this so that we can do without the ctype library
#define is_digit(c)				((c) >= '0' && (c) <= '9')

// debug
//#define DEBUG_UIP

// html files
extern const struct fsdata_file file_index_html;
extern const struct fsdata_file file_404_html;
extern const struct fsdata_file file_flashing_html;
extern const struct fsdata_file file_fail_html;

extern int webfailsafe_ready_for_upgrade;
extern int webfailsafe_upgrade_type;
extern ulong NetBootFileXferSize;

// http app state
struct httpd_state *hs;

static unsigned char *webfailsafe_data_pointer;
unsigned char *webfailsafe_data_pointer_base;

static int webfailsafe_post_done = 0;
static int webfailsafe_upload_failed = 0;
static int data_start_found = 0;

static unsigned char post_packet_counter = 0;

// 0x0D -> CR 0x0A -> LF
static char eol[3] = { 0x0d, 0x0a, 0x00 };
static char eol2[5] = { 0x0d, 0x0a, 0x0d, 0x0a, 0x00 };

static char *boundary_value;

// str to int
static int atoi(const char *s){
	int i = 0;

	while(is_digit(*s)){
		i = i * 10 + *(s++) - '0';
	}

	return(i);
}

// print downloading progress
static void httpd_download_progress(void){
	if(post_packet_counter == 39){
		puts("\n         ");
		post_packet_counter = 0;
	}

	puts("#");
	post_packet_counter++;
}

// http server init
void httpd_init(void){
	fs_init();
	uip_listen(HTONS(80));
}

// reset app state
static void httpd_state_reset(void){
	hs->state = STATE_NONE;
	hs->count = 0;
	hs->dataptr = 0;
	hs->upload = 0;
	hs->upload_total = 0;

	data_start_found = 0;
	post_packet_counter = 0;

	if(boundary_value){
		free(boundary_value);
		boundary_value = NULL;
	}
}

// find and get first chunk of data
static int httpd_findandstore_firstchunk(void){

	DECLARE_GLOBAL_DATA_PTR;

	char *start = NULL;
	char *end = NULL;

	if(!boundary_value){
		return(0);
	}

	// chek if we have data in packet
	start = (char *)strstr((char *)uip_appdata, (char *)boundary_value);

	if(start){

		// ok, we have data in this packet!
		// find upgrade type

		end = (char *)strstr((char *)start, "name=\"firmware\"");
		if (end) {

			printf("Upgrade type: firmware\n");
			webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE;

		} else {

			end = (char *)strstr((char *)start, "name=\"uboot\"");
			if (end) {
#if defined(WEBFAILSAFE_DISABLE_UBOOT_UPGRADE)
				printf("*** ERROR: U-Boot upgrade is not allowed on this board!\n");
				webfailsafe_upload_failed = 1;
#else
				webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_UBOOT;
				printf("Upgrade type: U-Boot\n");
#endif /* if defined(WEBFAILSAFE_DISABLE_UBOOT_UPGRADE) */
			} else {

				end = (char *)strstr((char *)start, "name=\"radio\"");
				if (end) {
#if defined(WEBFAILSAFE_DISABLE_RADIO_UPGRADE)
					printf("*** ERROR: Radio upgrade is not allowed on this board!\n");
					webfailsafe_upload_failed = 1;
#else
					printf("Upgrade type: RADIO\n");
					webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_RADIO;
#endif
				} else {
					end = (char *)strstr( (char *)start, "name=\"config\"" );
					if ( end ) {
#if defined(WEBFAILSAFE_DISABLE_CONFIG_UPGRADE)
						printf( "*** ERROR: Config upgrade is not allowed on this board!\n" );
						webfailsafe_upload_failed = 1;
#else
						printf( "Upgrade type: CONFIG\n" );
						webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_CONFIG;
#endif
					} else {
						end = (char *)strstr( (char *)start, "name=\"tplink\"" );
						if ( end ) {
#if defined(WEBFAILSAFE_DISABLE_TPLINK_UPGRADE)
							printf( "*** ERROR: Tplink upgrade is not allowed on this board!\n" );
							webfailsafe_upload_failed = 1;
#else
							printf( "Upgrade type: TPLINK\n" );
							webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_TPLINK;
#endif
						} else {
							end = (char *)strstr( (char *)start, "name=\"initramfs\"" );
							if ( end ) {
#if defined(WEBFAILSAFE_DISABLE_INITRAMFS_UPGRADE)
								printf( "*** ERROR: Initramfs upgrade is not allowed on this board!\n" );
								webfailsafe_upload_failed = 1;
#else
								printf( "Upgrade type: INITRAMFS\n" );
								webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_INITRAMFS;
#endif
							} else {
								printf("*** ERROR: input name not found!\n");
								return(0);
							}
						}
					}
				}

			}
		}

		end = NULL;

		// find start position of the data!
		end = (char *)strstr((char *)start, eol2);

		if(end){

			if ((end - (char *)uip_appdata) < uip_len) {

				// move pointer over CR LF CR LF
				end += 4;

				// how much data we expect?
				// last part (magic value 6): [CR][LF](boundary length)[-][-][CR][LF]
				hs->upload_total = hs->upload_total - (int)(end - start) - strlen(boundary_value) - 6;

				printf("Upload file size: %d bytes\n", hs->upload_total);

				// We need to check if file which we are going to download
				// has correct size (for every type of upgrade)

				// U-Boot
				if (webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_UBOOT) {
					if (hs->upload_total > UBOOT_FILE_SIZE_MAX) {
						printf("*** ERROR: u-boot file is too big, should be <= %d bytes.\n", UBOOT_FILE_SIZE_MAX);
						webfailsafe_upload_failed = 1;
					}
					else if (hs->upload_total < UBOOT_FILE_SIZE_MIN) {
						printf("*** ERROR: u-boot file is too small, should be >= %d bytes.\n", UBOOT_FILE_SIZE_MIN);
						webfailsafe_upload_failed = 1;
					}
				// Radio
				} else if (webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_RADIO) {
					if (hs->upload_total != CFG_RADIO_SIZE) {
						printf("*** ERROR: wrong RADIO file size, should be %d bytes.\n", CFG_RADIO_SIZE);
						webfailsafe_upload_failed = 1;
					}
				} else if ( webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_CONFIG ) {
					if ( hs->upload_total != CFG_CONFIG_SIZE ) {
						printf( "*** ERROR: wrong Config file size, should be %d bytes.\n", CFG_CONFIG_SIZE );
						webfailsafe_upload_failed = 1;
					}
				} else if ( webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_TPLINK ) {
					if ( hs->upload_total != CFG_TPLINK_SIZE ) {
						printf( "*** ERROR: wrong TPLink file size, should be %d bytes.\n", CFG_TPLINK_SIZE );
						webfailsafe_upload_failed = 1;
					}
				} else if (webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE || webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_INITRAMFS) {
					const char *fw_type = webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE ? "firmware" : "initramfs";
					if ( hs->upload_total > (gd->bd->bi_flashsize - WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES) ) {
						printf("*** ERROR: %s file is too big - %d, should be <= %d bytes.\n", fw_type, hs->upload_total, (gd->bd->bi_flashsize - WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES));
						webfailsafe_upload_failed = 1;
					}
					else if (hs->upload_total < LINUX_FILE_SIZE_MIN) {
						printf( "*** ERROR: %s file is too small - %d, should be >= %d bytes.\n", fw_type, hs->upload_total, LINUX_FILE_SIZE_MIN);
						webfailsafe_upload_failed = 1;
					}
				} else if (webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_ROOTFS_RESET) {
					if (rootfs_func(1) < 0) {
						printf("*** ERROR: rootfs reset is not supported by current firmware running on the device\n");
						webfailsafe_upload_failed = 1;
					}
				}

				printf("Loading: ");

				// how much data we are storing now?
				hs->upload = (unsigned int)(uip_len - (end - (char *)uip_appdata));
				webfailsafe_data_pointer = (void*)WEBFAILSAFE_UPLOAD_RAM_ADDRESS;

				switch ( webfailsafe_upgrade_type ) {
					case WEBFAILSAFE_UPGRADE_TYPE_UBOOT:
						memset( (void *)webfailsafe_data_pointer, 0xFF, CFG_BOOTLOADER_SIZE );
						break;
					case WEBFAILSAFE_UPGRADE_TYPE_INITRAMFS:
					case WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE:
					case WEBFAILSAFE_UPGRADE_TYPE_RADIO:
					case WEBFAILSAFE_UPGRADE_TYPE_CONFIG:
					case WEBFAILSAFE_UPGRADE_TYPE_TPLINK:
					default:
						break;
					case WEBFAILSAFE_UPGRADE_TYPE_ROOTFS_RESET:
						webfailsafe_data_pointer = (void*)WEBFAILSAFE_UPLOAD_RAM_ADDRESS;
						break;
				}

				if ( webfailsafe_upload_failed ) {
					webfailsafe_data_pointer_base = NULL;
				} else {
					webfailsafe_data_pointer_base = webfailsafe_data_pointer;
				}

				if (webfailsafe_data_pointer) {
					printf("Data will be downloaded at 0x%X in RAM\n", (unsigned long)webfailsafe_data_pointer);
					memcpy((void *)webfailsafe_data_pointer, (void *)end, hs->upload);
					webfailsafe_data_pointer += hs->upload;
				}

				httpd_download_progress();

				return(1);

			}

		} else {
			printf("*** ERROR: couldn't find start of data!\n");
		}

	}

	return(0);
}

#define MIN_RESERVED_VECTORS 2

int verify_uboot_image( const u32 *data, unsigned int length ) {
	int i;

	if (length > UBOOT_FILE_SIZE_MAX) {
		printf("%s(): image size %d is too big!\n", __func__, length);
		return -1;
	}

	if (length < UBOOT_FILE_SIZE_MIN) {
		printf("%s(): image size %d is too small!\n", __func__, length);
		return -1;
	}

	/* check that we have valid interrupt vector table */
	for (i = 0; i < MIN_RESERVED_VECTORS; i++) {
		u16 opcode;
		s16 offset;
		if (data[i] == 0x00000000) {
			continue; /* nop */
		}
		opcode = (u16)((data[i] >> 16) & 0xFFFF);
		offset = (s16)((data[i] >> 0) & 0xFFFF);
		if ( opcode == 0x1000 || opcode == 0x241A ) {
			/* branch/li instructions */
			if (offset > MIN_RESERVED_VECTORS - i) {
				continue;
			}
			debug("%s() [%03d] : branch to bad offset %i\n", __func__, i * 4, (offset - ((MIN_RESERVED_VECTORS-1) - i)) * 4);
			return -1; /* bad offset */
		}
		debug("%s() [%03d] : bad instruction -  0x%08x\n", __func__, i, data[i]);
		return -1;
	}

	return 0;
}

static int validate_uploaded_data( unsigned char *data, unsigned int length ) {
	if ( webfailsafe_upload_failed ) {
		return -1;
	}
	if (webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_ROOTFS_RESET) {
		return 0;
	}
	if (data == NULL) {
		return -1;
	}
	if ( webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_UBOOT ) {
		return verify_uboot_image( (const u32 *)data, length );
	}
	if ( webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_INITRAMFS || webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE) {
		int len = verify_kernel_image((ulong)data, NULL, NULL, NULL, 1);
		if (len > 0) {
			unsigned int data_size = length - len;
			if (webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_INITRAMFS) {
				/* check initramfs */
				if (data_size == sizeof(image_header_t)) {
					return 0;
				}
			}
			else {
				/* check firmware */
				if (data_size > sizeof(image_header_t)) {
					return 0;
				}
			}
		}
		return -1;
	}
#if 0
	if ( webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_CONFIG ) {
		/* TODO: validate for valid MAC & WPS PIN */
	}
	if (webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_TPLINK) {
		/* TODO: validate for valid partition table */
	}
#endif
	return 0;
}

static int httpd_inline_commands( const char *start )
{
	if ( strstr((char*)uip_appdata + 4, "application/x-www-form-urlencoded" ) ) {
		start = (const char *)strstr( start, "rootfs_reset=" );
		if ( start ) {
			if (rootfs_func(1 /* check */) >= 0) {
				webfailsafe_upgrade_type = WEBFAILSAFE_UPGRADE_TYPE_ROOTFS_RESET;
				webfailsafe_upload_failed = 0;
			} else {
				webfailsafe_upgrade_type = -1;
				webfailsafe_upload_failed = 1;
			}
			return 0;
		}
	}
	return -1;
}

static void httpd_post_done( struct httpd_state * hs, ulong upload_total, int failed )
{
	struct fs_file fsfile;

	// end of post upload
	webfailsafe_post_done = 1;
	NetBootFileXferSize = upload_total;

	// which website will be returned
	if ( failed ) {
		fs_open( file_fail_html.name, &fsfile );
	} else {
		fs_open( file_flashing_html.name, &fsfile );
	}

	httpd_state_reset();

	hs->state = STATE_FILE_REQUEST;
	hs->dataptr = (u8_t *)fsfile.data;
	hs->upload = fsfile.len;

	uip_send( hs->dataptr, (hs->upload > uip_mss() ? uip_mss() : hs->upload) );
}

// called for http server app
void httpd_appcall(void){
	struct fs_file fsfile;
	unsigned int i;

	switch(uip_conn->lport){

		case HTONS(80):

			// app state
			hs = (struct httpd_state *)(uip_conn->appstate);

			// closed connection
			if(uip_closed()){
				httpd_state_reset();
				uip_close();
				return;
			}

			// aborted connection or time out occured
			if(uip_aborted() || uip_timedout()){
				httpd_state_reset();
				uip_abort();
				return;
			}

			// if we are pooled
			if(uip_poll()){
				if(hs->count++ >= 100){
					httpd_state_reset();
					uip_abort();
				}
				return;
			}

			// new connection
			if(uip_connected()){
				httpd_state_reset();
				return;
			}

			// new data in STATE_NONE
			if(uip_newdata() && hs->state == STATE_NONE){

				// GET or POST request?
				if(uip_appdata[0] == ISO_G && uip_appdata[1] == ISO_E && uip_appdata[2] == ISO_T && (uip_appdata[3] == ISO_space || uip_appdata[3] == ISO_tab)){
					hs->state = STATE_FILE_REQUEST;
				} else if(uip_appdata[0] == ISO_P && uip_appdata[1] == ISO_O && uip_appdata[2] == ISO_S && uip_appdata[3] == ISO_T && (uip_appdata[4] == ISO_space || uip_appdata[4] == ISO_tab)){
					hs->state = STATE_UPLOAD_REQUEST;
				}

				// anything else -> abort the connection!
				if(hs->state == STATE_NONE){
					httpd_state_reset();
					uip_abort();
					return;
				}

				// get file or firmware upload?
				if(hs->state == STATE_FILE_REQUEST){

					// we are looking for GET file name
					for(i = 4; i < 30; i++){
						if(uip_appdata[i] == ISO_space || uip_appdata[i] == ISO_cr || uip_appdata[i] == ISO_nl || uip_appdata[i] == ISO_tab){
							uip_appdata[i] = 0;
							i = 0;
							break;
						}
					}

					if(i != 0){
						printf("*** ERROR: request file name too long!\n");
						httpd_state_reset();
						uip_abort();
						return;
					}

					printf("Request for: ");
					printf("%s\n", &uip_appdata[4]);

					// request for /
					if(uip_appdata[4] == ISO_slash && uip_appdata[5] == 0){
						fs_open(file_index_html.name, &fsfile);
					} else {
						// check if we have requested file
						if(!fs_open((const char *)&uip_appdata[4], &fsfile)){
							printf("*** ERROR: file not found!\n");
							fs_open(file_404_html.name, &fsfile);
						}
					}

					hs->state = STATE_FILE_REQUEST;
					hs->dataptr = (u8_t *)fsfile.data;
					hs->upload = fsfile.len;

					// send first (and maybe the last) chunk of data
					uip_send(hs->dataptr, (hs->upload > uip_mss() ? uip_mss() : hs->upload));
					return;

				} else if(hs->state == STATE_UPLOAD_REQUEST){

					char *start = NULL;
					char *end = NULL;

					// end buffer data with NULL
					uip_appdata[uip_len] = '\0';

					/*
					 * We got first packet with POST request
					 *
					 * Some browsers don't include first chunk of data in the first
					 * POST request packet (like Google Chrome, IE and Safari)!
					 * So we must now find two values:
					 * - Content-Length
					 * - boundary
					 * Headers with these values can be in any order!
					 * If we don't find these values in first packet, connection will be aborted!
					 *
					 */

					// Content-Length pos
					start = (char *)strstr((char*)uip_appdata, "Content-Length:");

					if(start){

						start += sizeof("Content-Length:");

						// find end of the line with "Content-Length:"
						end = (char *)strstr(start, eol);

						if(end){

							hs->upload_total = atoi(start);
#ifdef DEBUG_UIP
							printf("Expecting %d bytes in body request message\n", hs->upload_total);
#endif

						} else {
							printf("*** ERROR: couldn't find 'Content-Length'!\n");
							httpd_state_reset();
							uip_abort();
							return;
						}

					} else {
						printf("*** ERROR: couldn't find 'Content-Length'!\n");
						httpd_state_reset();
						uip_abort();
						return;
					}

					// check for inline commands
					if ( hs->upload_total < 256 && httpd_inline_commands( end ) >= 0 ) {
						httpd_post_done( hs, 0, webfailsafe_upload_failed );
						return;
					}

					// we don't support very small files (< 10 KB)
					if(hs->upload_total < 10240){
						printf("*** ERROR: request for upload < 10 KB data!\n");
						httpd_state_reset();
						uip_abort();
						return;
					}

					// boundary value
					start = NULL;
					end = NULL;

					start = (char *)strstr((char *)uip_appdata, "boundary=");

					if(start){

						// move pointer over "boundary="
						start += 9;

						// find end of line with boundary value
						end = (char *)strstr((char *)start, eol);

						if(end){

							// malloc space for boundary value + '--' and '\0'
							boundary_value = (char*)malloc(end - start + 3);

							if(boundary_value){

								memcpy(&boundary_value[2], start, end - start);

								// add -- at the begin and 0 at the end
								boundary_value[0] = '-';
								boundary_value[1] = '-';
								boundary_value[end - start + 2] = '\0';

#ifdef DEBUG_UIP
								printf("Found boundary value: \"%s\"\n", boundary_value);
#endif

							} else {
								printf("*** ERROR: couldn't allocate memory for boundary!\n");
								httpd_state_reset();
								uip_abort();
								return;
							}

						} else {
							printf("*** ERROR: couldn't find boundary!\n");
							httpd_state_reset();
							uip_abort();
							return;
						}

					} else {
						printf("*** ERROR: couldn't find boundary!\n");
						httpd_state_reset();
						uip_abort();
						return;
					}

					/*
					 * OK, if we are here, it means that we found
					 * Content-Length and boundary values in headers
					 *
					 * We can now try to 'allocate memory' and
					 * find beginning of the data in first packet
					 */
					webfailsafe_data_pointer = NULL;
					webfailsafe_data_pointer_base = NULL;

					if(httpd_findandstore_firstchunk()){
						data_start_found = 1;
					} else {
						data_start_found = 0;
					}

					return;

				} /* else if(hs->state == STATE_UPLOAD_REQUEST) */

			} /* uip_newdata() && hs->state == STATE_NONE */

			// if we got ACK from remote host
			if(uip_acked()){

				// if we are in STATE_FILE_REQUEST state
				if(hs->state == STATE_FILE_REQUEST){

					// data which we send last time was received (we got ACK)
					// if we send everything last time -> gently close the connection
					if(hs->upload <= uip_mss()){

						// post upload completed?
						if(webfailsafe_post_done){

							if(!webfailsafe_upload_failed){
								webfailsafe_ready_for_upgrade = 1;
							}

							webfailsafe_post_done = 0;
							webfailsafe_upload_failed = 0;
						}

						httpd_state_reset();
						uip_close();
						return;
					}

					// otherwise, send another chunk of data
					// last time we sent uip_conn->len size of data
					hs->dataptr += uip_conn->len;
					hs->upload -= uip_conn->len;

					uip_send(hs->dataptr, (hs->upload > uip_mss() ? uip_mss() : hs->upload));
				}

				return;

			}

			// if we need to retransmit
			if(uip_rexmit()){

				// if we are in STATE_FILE_REQUEST state
				if(hs->state == STATE_FILE_REQUEST){
					// send again chunk of data without changing pointer and length of data left to send
					uip_send(hs->dataptr, (hs->upload > uip_mss() ? uip_mss() : hs->upload));
				}

				return;

			}

			// if we got new data frome remote host
			if(uip_newdata()){

				// if we are in STATE_UPLOAD_REQUEST state
				if(hs->state == STATE_UPLOAD_REQUEST){

					// end buffer data with NULL
					uip_appdata[uip_len] = '\0';

					// do we have to find start of data?
					if(!data_start_found){

						if(!httpd_findandstore_firstchunk()){
							printf("*** ERROR: couldn't find start of data in next packet!\n");
							httpd_state_reset();
							uip_abort();
							return;
						} else {
							data_start_found = 1;
						}

						return;
					}

					hs->upload += (unsigned int)uip_len;

					if(!webfailsafe_upload_failed){
						memcpy((void *)webfailsafe_data_pointer, (void *)uip_appdata, uip_len);
						webfailsafe_data_pointer += uip_len;
					}

					httpd_download_progress();

					// if we have collected all data
					if(hs->upload >= hs->upload_total){
						printf("\n\n");
						if ( validate_uploaded_data( webfailsafe_data_pointer_base, (ulong)hs->upload_total ) != 0 ) {
							webfailsafe_upload_failed = 1;
						}
						httpd_post_done( hs, (ulong)hs->upload_total, webfailsafe_upload_failed );
					}
				} // if ( hs->state == STATE_UPLOAD_REQUEST )
				return;
			} // if ( uip_newdata() )
			break;
		default:
			// we shouldn't get here... we are listening only on port 80
			uip_abort();
			break;
	}
}
