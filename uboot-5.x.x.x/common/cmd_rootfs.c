#include <common.h>
#include <command.h>

#include <spi_api.h>

//#if defined(CONFIG_CMD_FLASH)

#define JFFS2_MAGIC_BITMASK 0x1985
#define SQUASHFS_MAGIC 0x73717368

struct jffs2_unknown_node
{
	__u16 magic; /* JFFS2_MAGIC */
	__u16 nodetype;
	__u32 totlen;
	__u32 hdr_crc;
} __attribute__( (packed) );

struct squashfs_header
{
	__u32 magic;		/* SQUASHFS_MAGIC */
	__u32 inode_count;
	__u32 modification_time;
	__u32 block_size;
	__u32 fragment_entry_count;
	__u16 compression_id;
	__u16 block_log;
	__u16 flags;
	__u16 id_count;
	__u16 version_major;
	__u16 version_minor;
	__u64 root_inode_ref;
	__u64 bytes_used;	/* including header size */
	__u64 id_table_start;
	__u64 xattr_id_table_start;
	__u64 inode_table_start;
	__u64 directory_table_start;
	__u64 fragment_table_start;
	__u64 export_table_start;
} __attribute__( (packed) );

//extern flash_info_t flash_info[]; /* info for FLASH chips */

static void make_crc_table( u32 *crc_table )
{
	const u32 poly = 0xedb88320L;
	int n, k;

	for ( n = 0; n < 256; n++ )
	{
		u32 c = (u32)n;
		for ( k = 0; k < 8; k++ )
		{
			c = ( c & 1 ) ? poly ^ (c >> 1) : c >> 1;
		}
		crc_table[ n ] = c;
	}
}

static u32 crc32_jffs2( const unsigned char *buf, int len )
{
	static int crc_table_inited = 0;
	static u32 crc_table[ 256 ];
	u32 crc = 0;

	if ( crc_table_inited == 0 )
	{
		crc_table_inited = 1;
		make_crc_table( crc_table );
	}

	while ( --len >= 0 )
	{
	    crc = crc_table[ (crc ^ (*buf++)) & 0xff ] ^ (crc >> 8);
	}

	return crc;
}

#define PAD(base,align) (((base) + ((align) - 1)) & ~((align) - 1))

static const unsigned char eof_mark[] = { 0xde, 0xad, 0xc0, 0xde };

static void write_eof(int offset) {
	unsigned char write_buf[sizeof(eof_mark) * 4];
	int i;
	for (i = 0; i < 4; i++) {
		memcpy(write_buf + i * sizeof(eof_mark), eof_mark, sizeof(eof_mark));
	}
	printf("setting EOF marker at %08x ...\n", offset);
	raspi_erase(offset, sizeof(write_buf));
	raspi_write(write_buf, offset, sizeof(write_buf));
}

/*
  input parameters:
   check - perform either jffs2 status check/validation (1) or reset (0)
   output codes:
   < 0 - unsupporrted/error
   = 0 - can be cleared/success
   > 0 - already cleared
*/
#define FLASH_SECT_SIZE (64*1024)
int rootfs_func(int check) {
	DECLARE_GLOBAL_DATA_PTR;
	union {
		image_header_t ih;
		struct jffs2_unknown_node node;
		struct squashfs_header sh;
	} u;
	uint32_t offset, max_offset, crc;
	uint64_t size64;
	unsigned char write_buf[4];

	//printf("%s MODE: %s\n", __func__, check ? "check" : "erase");

	max_offset = CFG_MAX_ROOTFS_OFFSET - FLASH_SECT_SIZE;

	offset = CFG_KERNEL_OFFSET;

	raspi_read((char*)&u.ih, offset, sizeof(u.ih));

	/* rude platform check*/
	if (strncmp(u.ih.ih_name, "MIPS OpenWrt", 12) != 0) {
		char mem[33];
		memcpy(mem, u.ih.ih_name, 32); mem[32] = '\0';
		printf("WARNING: unsupported firmware vendor: %s\n", mem);
		return -1;
	}

	//printf("ih_size: %08x\n", ntohl(u.ih.ih_size));
	offset += ntohl(u.ih.ih_size);
	if (offset + sizeof(image_header_t) > max_offset) {
		printf("incorrect kernel image size: %i\n", offset);
		return -2;
	}

	offset += sizeof(image_header_t);

	raspi_read((char*)&u.sh, offset, sizeof(u.sh));
	if (u.sh.magic != SQUASHFS_MAGIC) {
		printf("bad squashfs magic %08x\n", u.sh.magic);
		return -3;
	}

	size64 = u.sh.bytes_used;
	if (offset + size64 > max_offset) {
		printf("incorrect squashfs size: %08x\n", (uint32_t)size64);
		return -4;
	}

	printf("squashfs magic found at 0x%08x, filesystem size 0x%x\n", offset, (uint32_t)size64);

	offset += size64;
	offset = PAD(offset, FLASH_SECT_SIZE);
	if (offset > max_offset) {
		printf("too big jffs2 offset %08x\n", offset);
		return -5;
	}

	raspi_read((char*)&u.node, offset, sizeof(u.node));
	if (memcmp(&u.node, eof_mark, sizeof(eof_mark)) == 0) {
		if (check == 0) {
			write_eof(offset);
		} else {
			printf("jffs2 is already cleared at 0x%08x\n", offset);
		}
		return 1;
	}

	if (u.node.magic != JFFS2_MAGIC_BITMASK) {
		printf("bad jffs2 magic %04x at 0x%08x\n", u.node.magic, offset);
		return -6;
	}

	printf("jffs2 magic found at 0x%08x\n", offset);
	crc = crc32_jffs2((const unsigned char*)&u.node, sizeof(u.node) - sizeof(u.node.hdr_crc));
	if (crc != u.node.hdr_crc) {
		printf("bad jffs2 node crc: got %08x, expected %08x\n", u.node.hdr_crc, crc);
		return -7;
	}

	if (check == 0) {
		write_eof(offset);
	} else {
		printf("filesystem can be cleared at 0x%08x\n", offset);
	}

	return 0;
}

int do_rootfs( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[] )
{
	int check;

	check = 1;
	if (argc >= 2) {
		if (strcmp(argv[1], "reset") == 0) {
			check = 0;
		}
	}

	return rootfs_func(check);
}

#ifdef RALINK_CMDLINE
U_BOOT_CMD( rootfs, 2, 0, do_rootfs, "rootfs  - manage rootfs/jffs2 overlay filesystem\n", NULL );
#endif

//#endif /* defined(CONFIG_CMD_FLASH) */
