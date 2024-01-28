#include <common.h>
#include <command.h>

#if defined (CONFIG_COMMANDS) && defined(CONFIG_RT2880_ETH)

#include <malloc.h>
#include <net.h>
#include <asm/addrspace.h>
#include <rt_mmap.h>

#undef DEBUG
#define BIT(x)              ((1 << x))

/* ====================================== */
//GDMA1 uni-cast frames destination port
#define GDM_UFRC_P_CPU     ((u32)(~(0x7 << 12)))
#define GDM_UFRC_P_GDMA1   (1 << 12)
#define GDM_UFRC_P_GDMA2   (2 << 12)
#define GDM_UFRC_P_DROP    (7 << 12)
//GDMA1 broad-cast MAC address frames
#define GDM_BFRC_P_CPU     ((u32)(~(0x7 << 8)))
#define GDM_BFRC_P_GDMA1   (1 << 8)
#define GDM_BFRC_P_GDMA2   (2 << 8)
#define GDM_BFRC_P_PPE     (6 << 8)
#define GDM_BFRC_P_DROP    (7 << 8)
//GDMA1 multi-cast MAC address frames
#define GDM_MFRC_P_CPU     ((u32)(~(0x7 << 4)))
#define GDM_MFRC_P_GDMA1   (1 << 4)
#define GDM_MFRC_P_GDMA2   (2 << 4)
#define GDM_MFRC_P_PPE     (6 << 4)
#define GDM_MFRC_P_DROP    (7 << 4)
//GDMA1 other MAC address frames destination port
#define GDM_OFRC_P_CPU     ((u32)(~(0x7)))
#define GDM_OFRC_P_GDMA1   1
#define GDM_OFRC_P_GDMA2   2
#define GDM_OFRC_P_PPE     6
#define GDM_OFRC_P_DROP    7

#define RST_DRX_IDX0      BIT(16)
#define RST_DTX_IDX0      BIT(0)

#define TX_WB_DDONE       BIT(6)
#define RX_DMA_BUSY       BIT(3)
#define TX_DMA_BUSY       BIT(1)
#define RX_DMA_EN         BIT(2)
#define TX_DMA_EN         BIT(0)

#define GP1_FRC_EN        BIT(15)
#define GP1_FC_TX         BIT(11)
#define GP1_FC_RX         BIT(10)
#define GP1_LNK_DWN       BIT(9)
#define GP1_AN_OK         BIT(8)

/*
 * FE_INT_STATUS
 */
#define CNT_PPE_AF       BIT(31)
#define CNT_GDM1_AF      BIT(29)
#define PSE_P1_FC        BIT(22)
#define PSE_P0_FC        BIT(21)
#define PSE_FQ_EMPTY     BIT(20)
#define GE1_STA_CHG      BIT(18)
#define TX_COHERENT      BIT(17)
#define RX_COHERENT      BIT(16)

#define TX_DONE_INT1     BIT(9)
#define TX_DONE_INT0     BIT(8)
#define RX_DONE_INT0     BIT(2)
#define TX_DLY_INT       BIT(1)
#define RX_DLY_INT       BIT(0)

/*
 * Ethernet chip registers.RT2880
 */
/* Old FE with New PDMA */
#define PDMA_RELATED		0x0800
/* 1. PDMA */
#define TX_BASE_PTR0            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x000)
#define TX_MAX_CNT0             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x004)
#define TX_CTX_IDX0             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x008)
#define TX_DTX_IDX0             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x00C)

#define TX_BASE_PTR1            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x010)
#define TX_MAX_CNT1             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x014)
#define TX_CTX_IDX1             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x018)
#define TX_DTX_IDX1             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x01C)

#define TX_BASE_PTR2            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x020)
#define TX_MAX_CNT2             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x024)
#define TX_CTX_IDX2             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x028)
#define TX_DTX_IDX2             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x02C)

#define TX_BASE_PTR3            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x030)
#define TX_MAX_CNT3             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x034)
#define TX_CTX_IDX3             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x038)
#define TX_DTX_IDX3             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x03C)

#define RX_BASE_PTR0            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x100)
#define RX_MAX_CNT0             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x104)
#define RX_CALC_IDX0            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x108)
#define RX_DRX_IDX0             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x10C)

#define RX_BASE_PTR1            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x110)
#define RX_MAX_CNT1             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x114)
#define RX_CALC_IDX1            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x118)
#define RX_DRX_IDX1             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x11C)

#define PDMA_INFO               (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x200)
#define PDMA_GLO_CFG            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x204)
#define PDMA_RST_IDX            (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x208)
#define PDMA_RST_CFG            (RALINK_FRAME_ENGINE_BASE + PDMA_RST_IDX)
#define DLY_INT_CFG             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x20C)
#define FREEQ_THRES             (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x210)
#define INT_STATUS              (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x220) /* FIXME */
#define INT_MASK                (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x228) /* FIXME */
#define PDMA_WRR                (RALINK_FRAME_ENGINE_BASE + PDMA_RELATED+0x280)
#define PDMA_SCH_CFG            (PDMA_WRR)

/* TODO: change FE_INT_STATUS->INT_STATUS 
 * FE_INT_ENABLE->INT_MASK */
#define MDIO_ACCESS         RALINK_FRAME_ENGINE_BASE + 0x00
#define MDIO_CFG            RALINK_FRAME_ENGINE_BASE + 0x04
#define FE_DMA_GLO_CFG      RALINK_FRAME_ENGINE_BASE + 0x08
#define FE_RST_GLO          RALINK_FRAME_ENGINE_BASE + 0x0C
#define FE_INT_STATUS       RALINK_FRAME_ENGINE_BASE + 0x10
#define FE_INT_ENABLE       RALINK_FRAME_ENGINE_BASE + 0x14
#define FC_DROP_STA         RALINK_FRAME_ENGINE_BASE + 0x18
#define FOE_TS_T            RALINK_FRAME_ENGINE_BASE + 0x1C

#define PAD_RGMII2_MDIO_CFG            RALINK_SYSCTL_BASE + 0x58

#define GDMA1_RELATED       0x0500
#define GDMA1_FWD_CFG       (RALINK_FRAME_ENGINE_BASE + GDMA1_RELATED + 0x00)
#define GDMA1_SHRP_CFG      (RALINK_FRAME_ENGINE_BASE + GDMA1_RELATED + 0x04)
#define GDMA1_MAC_ADRL      (RALINK_FRAME_ENGINE_BASE + GDMA1_RELATED + 0x08)
#define GDMA1_MAC_ADRH      (RALINK_FRAME_ENGINE_BASE + GDMA1_RELATED + 0x0C)
#define GDMA2_RELATED       0x1500
#define GDMA2_FWD_CFG       (RALINK_FRAME_ENGINE_BASE + GDMA2_RELATED + 0x00)
#define GDMA2_SHRP_CFG      (RALINK_FRAME_ENGINE_BASE + GDMA2_RELATED + 0x04)
#define GDMA2_MAC_ADRL      (RALINK_FRAME_ENGINE_BASE + GDMA2_RELATED + 0x08)
#define GDMA2_MAC_ADRH      (RALINK_FRAME_ENGINE_BASE + GDMA2_RELATED + 0x0C)

#define PSE_RELATED         0x0040
#define PSE_FQFC_CFG        (RALINK_FRAME_ENGINE_BASE + PSE_RELATED + 0x00)
#define CDMA_FC_CFG         (RALINK_FRAME_ENGINE_BASE + PSE_RELATED + 0x04)
#define GDMA1_FC_CFG        (RALINK_FRAME_ENGINE_BASE + PSE_RELATED + 0x08)
#define GDMA2_FC_CFG        (RALINK_FRAME_ENGINE_BASE + PSE_RELATED + 0x0C)
#define CDMA_OQ_STA         (RALINK_FRAME_ENGINE_BASE + PSE_RELATED + 0x10)
#define GDMA1_OQ_STA        (RALINK_FRAME_ENGINE_BASE + PSE_RELATED + 0x14)
#define GDMA2_OQ_STA        (RALINK_FRAME_ENGINE_BASE + PSE_RELATED + 0x18)
#define PSE_IQ_STA          (RALINK_FRAME_ENGINE_BASE + PSE_RELATED + 0x1C)

#define CDMA_RELATED        0x0080
#define CDMA_CSG_CFG        (RALINK_FRAME_ENGINE_BASE + CDMA_RELATED + 0x00)
#define CDMA_SCH_CFG        (RALINK_FRAME_ENGINE_BASE + CDMA_RELATED + 0x04)

#define INTERNAL_LOOPBACK_ENABLE 1
#define INTERNAL_LOOPBACK_DISABLE 0

//#define CONFIG_UNH_TEST

#define TOUT_LOOP   1000
#define ENABLE 1
#define DISABLE 0

VALID_BUFFER_STRUCT  rt2880_free_buf_list;
VALID_BUFFER_STRUCT  rt2880_busing_buf_list;
static BUFFER_ELEM   rt2880_free_buf[PKTBUFSRX];

/*=========================================
      PDMA RX Descriptor Format define
=========================================*/

//-------------------------------------------------
typedef struct _PDMA_RXD_INFO1_  PDMA_RXD_INFO1_T;

struct _PDMA_RXD_INFO1_
{
    unsigned int    PDP0;
};
//-------------------------------------------------
typedef struct _PDMA_RXD_INFO2_    PDMA_RXD_INFO2_T;

struct _PDMA_RXD_INFO2_
{
	unsigned int    PLEN1                   : 14;
	unsigned int    LS1                     : 1;
	unsigned int    UN_USED                 : 1;
	unsigned int    PLEN0                   : 14;
	unsigned int    LS0                     : 1;
	unsigned int    DDONE_bit               : 1;
};
//-------------------------------------------------
typedef struct _PDMA_RXD_INFO3_  PDMA_RXD_INFO3_T;

struct _PDMA_RXD_INFO3_
{
	unsigned int    PDP1;
};
//-------------------------------------------------
typedef struct _PDMA_RXD_INFO4_    PDMA_RXD_INFO4_T;

struct _PDMA_RXD_INFO4_
{
	unsigned int    FOE_Entry           	: 14;
	unsigned int    CRSN                	: 5;
	unsigned int    SP               	: 3;
	unsigned int    L4F                 	: 1;
	unsigned int    L4VLD               	: 1;
	unsigned int    TACK                	: 1;
	unsigned int    IP4F                	: 1;
	unsigned int    IP4                 	: 1;
	unsigned int    IP6                 	: 1;
	unsigned int    UN_USE1             	: 4;
};

struct PDMA_rxdesc {
	PDMA_RXD_INFO1_T rxd_info1;
	PDMA_RXD_INFO2_T rxd_info2;
	PDMA_RXD_INFO3_T rxd_info3;
	PDMA_RXD_INFO4_T rxd_info4;
};
/*=========================================
      PDMA TX Descriptor Format define
=========================================*/
//-------------------------------------------------
typedef struct _PDMA_TXD_INFO1_  PDMA_TXD_INFO1_T;

struct _PDMA_TXD_INFO1_
{
	unsigned int    SDP0;
};
//-------------------------------------------------
typedef struct _PDMA_TXD_INFO2_    PDMA_TXD_INFO2_T;

struct _PDMA_TXD_INFO2_
{
	unsigned int    SDL1                  : 14;
	unsigned int    LS1_bit               : 1;
	unsigned int    BURST_bit             : 1;
	unsigned int    SDL0                  : 14;
	unsigned int    LS0_bit               : 1;
	unsigned int    DDONE_bit             : 1;
};
//-------------------------------------------------
typedef struct _PDMA_TXD_INFO3_  PDMA_TXD_INFO3_T;

struct _PDMA_TXD_INFO3_
{
	unsigned int    SDP1;
};
//-------------------------------------------------
typedef struct _PDMA_TXD_INFO4_    PDMA_TXD_INFO4_T;

struct _PDMA_TXD_INFO4_
{
    unsigned int    VLAN_TAG            :16;
    unsigned int    INS                 : 1;
    unsigned int    RESV                : 2;
    unsigned int    UDF                 : 6;
    unsigned int    FPORT               : 3;
    unsigned int    TSO                 : 1;
    unsigned int    TUI_CO              : 3;
};

struct PDMA_txdesc {
	PDMA_TXD_INFO1_T txd_info1;
	PDMA_TXD_INFO2_T txd_info2;
	PDMA_TXD_INFO3_T txd_info3;
	PDMA_TXD_INFO4_T txd_info4;
};

static  struct PDMA_txdesc tx_ring0_cache[NUM_TX_DESC] __attribute__ ((aligned(32))); /* TX descriptor ring         */
static  struct PDMA_rxdesc rx_ring_cache[NUM_RX_DESC] __attribute__ ((aligned(32))); /* RX descriptor ring         */

static int rx_dma_owner_idx0;                             /* Point to the next RXD DMA wants to use in RXD Ring#0.  */
static int rx_wants_alloc_idx0;                           /* Point to the next RXD CPU wants to allocate to RXD Ring #0. */
static int tx_cpu_owner_idx0;                             /* Point to the next TXD in TXD_Ring0 CPU wants to use */
static volatile struct PDMA_rxdesc *rx_ring;
static volatile struct PDMA_txdesc *tx_ring0;

static char rxRingSize;
static char txRingSize;

static int   rt2880_eth_init(struct eth_device* dev, bd_t* bis);
static int   rt2880_eth_send(struct eth_device* dev, volatile void *packet, int length);
static int   rt2880_eth_recv(struct eth_device* dev);
void  rt2880_eth_halt(struct eth_device* dev);

#ifdef RALINK_MDIO_ACCESS_FUN
#ifdef RALINK_EPHY_INIT
int   mii_mgr_read(u32 phy_addr, u32 phy_register, u32 *read_data);
int   mii_mgr_write(u32 phy_addr, u32 phy_register, u32 write_data);
#else
#define mii_mgr_read(x,y,z)	do{}while(0)
#define mii_mgr_write(x,y,z)    do{}while(0)
#endif // RALINK_EPHY_INIT //
#else
#define mii_mgr_read(x,y,z)	do{}while(0)
#define mii_mgr_write(x,y,z)    do{}while(0)
#endif // RALINK_MDIO_ACCESS_FUN //


static int   rt2880_eth_setup(struct eth_device* dev);
static int   rt2880_eth_initd;
char         console_buffer[CFG_CBSIZE];		/* console I/O buffer	*/


#define phys_to_bus(a) (a & 0x1FFFFFFF)

#define PCI_WAIT_INPUT_CHAR(ch) while((ch = getc())== 0)

struct eth_device* 	rt2880_pdev;

volatile uchar	*PKT_HEADER_Buf;// = (uchar *)CFG_EMBEDED_SRAM_SDP0_BUF_START;
static volatile uchar	PKT_HEADER_Buf_Pool[(PKTBUFSRX * PKTSIZE_ALIGN) + PKTALIGN];
extern volatile uchar	*NetTxPacket;	/* THE transmit packet			*/
extern volatile uchar	*PktBuf;
extern volatile uchar	Pkt_Buf_Pool[];

extern int rtl8367_gsw_init_post(void);

#define PIODIR_R  (RALINK_PIO_BASE + 0X24)
#define PIODATA_R (RALINK_PIO_BASE + 0X20)
#define PIODIR3924_R  (RALINK_PIO_BASE + 0x4c)
#define PIODATA3924_R (RALINK_PIO_BASE + 0x48)

#define FREEBUF_OFFSET(CURR)  ((int)(((0x0FFFFFFF & (u32)CURR) - (u32) (0x0FFFFFFF & (u32) rt2880_free_buf[0].pbuf)) / 1536))

void START_ETH(struct eth_device *dev ) {
	s32 omr;
	omr=RALINK_REG(PDMA_GLO_CFG);
	udelay(100);
	omr |= TX_WB_DDONE | RX_DMA_EN | TX_DMA_EN ;
		
	RALINK_REG(PDMA_GLO_CFG)=omr;
	udelay(500);
}


void STOP_ETH(struct eth_device *dev)
{
	s32 omr;
	omr=RALINK_REG(PDMA_GLO_CFG);
	udelay(100);
	omr &= ~(TX_WB_DDONE | RX_DMA_EN | TX_DMA_EN) ;
	RALINK_REG(PDMA_GLO_CFG)=omr;
	udelay(500);
}


BUFFER_ELEM *rt2880_free_buf_entry_dequeue(VALID_BUFFER_STRUCT *hdr)
{
	int     zero = 0;           /* causes most compilers to place this */
	/* value in a register only once */
	BUFFER_ELEM  *node;

	/* Make sure we were not passed a null pointer. */
	if (!hdr) {
		return (NULL);
	}

	/* If there is a node in the list we want to remove it. */
	if (hdr->head) {
		/* Get the node to be removed */
		node = hdr->head;

		/* Make the hdr point the second node in the list */
		hdr->head = node->next;

		/* If this is the last node the headers tail pointer needs to be nulled
		   We do not need to clear the node's next since it is already null */
		if (!(hdr->head)) {
			hdr->tail = (BUFFER_ELEM *)zero;
		}

		node->next = (BUFFER_ELEM *)zero;
	}
	else {
		node = NULL;
		return (node);
	}

	/*  Restore the previous interrupt lockout level.  */

	/* Return a pointer to the removed node */

	//shnat_validation_flow_table_entry[node->index].state = SHNAT_FLOW_TABLE_NODE_USED;
	return (node);
}

static BUFFER_ELEM *rt2880_free_buf_entry_enqueue(VALID_BUFFER_STRUCT *hdr, BUFFER_ELEM *item)
{
	int zero =0;

	if (!hdr) {
		return (NULL);
	}

	if (item != NULL)
	{
		/* Temporarily lockout interrupts to protect global buffer variables. */
		// Sys_Interrupt_Disable_Save_Flags(&cpsr_flags);

		/* Set node's next to point at NULL */
		item->next = (BUFFER_ELEM *)zero;

		/*  If there is currently a node in the linked list, we want to add the
		    new node to the end. */
		if (hdr->head) {
			/* Make the last node's next point to the new node. */
			hdr->tail->next = item;

			/* Make the roots tail point to the new node */
			hdr->tail = item;
		}
		else {
			/* If the linked list was empty, we want both the root's head and
			   tial to point to the new node. */
			hdr->head = item;
			hdr->tail = item;
		}

		/*  Restore the previous interrupt lockout level.  */

	}
	else
	{
		printf("\n shnat_flow_table_free_entry_enqueue is called,item== NULL \n");
	}

	return(item);

} /* MEM_Buffer_Enqueue */


int rt2880_eth_initialize(bd_t *bis)
{
	struct	eth_device* 	dev;
	int	i;
	u32	regValue;

	if (!(dev = (struct eth_device *) malloc (sizeof *dev))) {
		printf("Failed to allocate memory\n");
		return 0;
	}

	memset(dev, 0, sizeof(*dev));

	sprintf(dev->name, "eth2");

	dev->iobase = RALINK_FRAME_ENGINE_BASE;
	dev->init   = rt2880_eth_init;
	dev->halt   = rt2880_eth_halt;
	dev->send   = rt2880_eth_send;
	dev->recv   = rt2880_eth_recv;

	eth_register(dev);
	rt2880_pdev = dev;

	rt2880_eth_initd =0;
	PktBuf = Pkt_Buf_Pool;
	PKT_HEADER_Buf = PKT_HEADER_Buf_Pool;
	NetTxPacket = NULL;
	rx_ring = (struct PDMA_rxdesc *)KSEG1ADDR((ulong)&rx_ring_cache[0]);
	tx_ring0 = (struct PDMA_txdesc *)KSEG1ADDR((ulong)&tx_ring0_cache[0]);

	rt2880_free_buf_list.head = NULL;
	rt2880_free_buf_list.tail = NULL;

	rt2880_busing_buf_list.head = NULL;
	rt2880_busing_buf_list.tail = NULL;

	//2880_free_buf

	/*
	 *	Setup packet buffers, aligned correctly.
	 */
	rt2880_free_buf[0].pbuf = (unsigned char *)(&PktBuf[0] + (PKTALIGN - 1));
	rt2880_free_buf[0].pbuf -= (ulong)rt2880_free_buf[0].pbuf % PKTALIGN;
	rt2880_free_buf[0].next = NULL;

	rt2880_free_buf_entry_enqueue(&rt2880_free_buf_list,&rt2880_free_buf[0]);

#ifdef DEBUG
	printf("\n rt2880_free_buf[0].pbuf = 0x%08X \n",rt2880_free_buf[0].pbuf);
#endif
	for (i = 1; i < PKTBUFSRX; i++) {
		rt2880_free_buf[i].pbuf = rt2880_free_buf[0].pbuf + (i)*PKTSIZE_ALIGN;
		rt2880_free_buf[i].next = NULL;
#ifdef DEBUG
		printf("\n rt2880_free_buf[%d].pbuf = 0x%08X\n",i,rt2880_free_buf[i].pbuf);
#endif
		rt2880_free_buf_entry_enqueue(&rt2880_free_buf_list,&rt2880_free_buf[i]);
	}

	for (i = 0; i < PKTBUFSRX; i++)
	{
		rt2880_free_buf[i].tx_idx = NUM_TX_DESC;
#ifdef DEBUG
		printf("\n rt2880_free_buf[%d] = 0x%08X,rt2880_free_buf[%d].next=0x%08X \n",i,&rt2880_free_buf[i],i,rt2880_free_buf[i].next);
#endif
	}

	return 1;
}

static int rt2880_eth_init(struct eth_device* dev, bd_t* bis)
{
	if(rt2880_eth_initd == 0)
	{
		rt2880_eth_setup(dev);
	}
	else
	{
		START_ETH(dev);
	}

	rt2880_eth_initd = 1;
	return (1);
}

void IsSwitchVlanTableBusy(void)
{
	int j = 0;
	unsigned int value = 0;

	for (j = 0; j < 20; j++) {
	    mii_mgr_read(31, 0x90, &value);
	    if ((value & 0x80000000) == 0 ){ //table busy
			break;
	    }
	    udelay(70000);
	}
	if (j == 20)
	    printf("set vlan timeout value=0x%x.\n", value);
}

void LANWANPartition(void)
{
	unsigned int i;

/*Set  MT7530 */
#ifdef RALINK_PVLAN_WLLLL
	printf("set LAN/WAN WLLLL\n");
	//WLLLL, wan at P0, demo board
	//LAN/WAN ports as security mode
	mii_mgr_write(31, 0x2004, 0xff0003);//port0
	mii_mgr_write(31, 0x2104, 0xff0003);//port1
	mii_mgr_write(31, 0x2204, 0xff0003);//port2
	mii_mgr_write(31, 0x2304, 0xff0003);//port3
	mii_mgr_write(31, 0x2404, 0xff0003);//port4
	//mii_mgr_write(31, 0x2504, 0xff0003);//port5
	//mii_mgr_write(31, 0x2604, 0xff0003);//port5

	//set PVID
	mii_mgr_write(31, 0x2014, 0x10002);//port0
	mii_mgr_write(31, 0x2114, 0x10001);//port1
	mii_mgr_write(31, 0x2214, 0x10001);//port2
	mii_mgr_write(31, 0x2314, 0x10001);//port3
	mii_mgr_write(31, 0x2414, 0x10001);//port4
	//mii_mgr_write(31, 0x2514, 0x10001);//port5
	//mii_mgr_write(31, 0x2614, 0x10001);//port6

	/*port6 */
	//VLAN member
	IsSwitchVlanTableBusy();
	mii_mgr_write(31, 0x94, 0x407e0001);//VAWD1
	mii_mgr_write(31, 0x90, 0x80001001);//VTCR, VID=1
	IsSwitchVlanTableBusy();

	mii_mgr_write(31, 0x94, 0x40610001);//VAWD1
	mii_mgr_write(31, 0x90, 0x80001002);//VTCR, VID=2
	IsSwitchVlanTableBusy();
#endif
#if defined (RALINK_PVLAN_LLLLW) || defined (RALINK_PVLAN_LLLLX) || defined(RALINK_PVLAN_LLLWW) || defined(RALINK_PVLAN_LLLXX)

#if defined (RALINK_PVLAN_LLLLW)
	printf(" Set LAN/WAN port mapping LLLLW\n");
#endif
#if defined (RALINK_PVLAN_LLLLX)
	printf(" Set LAN/WAN port mapping LLLLX\n");
#endif
#if defined (RALINK_PVLAN_LLLWW)
	printf(" Set LAN/WAN  port mapping LLLWW\n");
#endif
#if defined (RALINK_PVLAN_LLLXX)
	printf(" Set LAN/WAN port mapping LLLXX\n");
#endif
	/* LAN/WAN ports as security mode */
	mii_mgr_write(31, 0x2004, 0xff0003); //port0 PCR: MATRIX=0xFF, PORT_VLAN=0x3 (Security Mode)
	mii_mgr_write(31, 0x2104, 0xff0003); //port1
	mii_mgr_write(31, 0x2204, 0xff0003); //port2
	mii_mgr_write(31, 0x2304, 0xff0003); //port3
	mii_mgr_write(31, 0x2404, 0xff0003); //port4

	/* set PVID */
	mii_mgr_write(31, 0x2014, 0x10001); //port0 PPBV1: G1_PORT_VID=1, G0_PORT_VID=1
	mii_mgr_write(31, 0x2114, 0x10001); //port1
	mii_mgr_write(31, 0x2214, 0x10001); //port2
	mii_mgr_write(31, 0x2314, 0x10001); //port3
	mii_mgr_write(31, 0x2414, 0x10002); //port4 PPBV1: G1_PORT_VID=1, G0_PORT_VID=2

	IsSwitchVlanTableBusy();
#if defined (RALINK_PVLAN_LLLLW) || defined (RALINK_PVLAN_LLLLX)
	/* VLAN 1: P0..P3 <-> P6 */
	mii_mgr_write(31, 0x94, 0x404f0001);// VAWD1: port member 0x4f (01001111)
	mii_mgr_write(31, 0x90, 0x80001001);// VTCR: VID=1
#else
	/* VLAN 1: P0..P2 <-> P6 */
	mii_mgr_write(31, 0x94, 0x40470001);// VAWD1: port member 0x47 (01000111)
	mii_mgr_write(31, 0x90, 0x80001001);// VTCR: VID=1
#endif
	IsSwitchVlanTableBusy();

#if defined (RALINK_PVLAN_LLLLW)
	/* VLAN 2: P4 <-> P6 */
	mii_mgr_write(31, 0x94, 0x40500001);// VAWD1: port member 0x50 (0101 0000)
	mii_mgr_write(31, 0x90, 0x80001002);// VTCR: write VID=2
#endif
#if defined (RALINK_PVLAN_LLLLX)
	/* VLAN 2: P4 to nothing */
	mii_mgr_write(31, 0x94, 0x40500001);// VAWD1: port member 0x10 (0001 0000)
	mii_mgr_write(31, 0x90, 0x80001002);// VTCR: write VID=2
#endif
#if defined (RALINK_PVLAN_LLLWW)
	/* VLAN 2: P3 <-> P6 */
	mii_mgr_write(31, 0x94, 0x40480001); // VAWD1: port member 0x48 (0100 1000)
	mii_mgr_write(31, 0x90, 0x80001002); // VTCR: write VID=2
	IsSwitchVlanTableBusy();
	/* VLAN 3: P4 <-> P6 */
	mii_mgr_write(31, 0x94, 0x40500001); // VAWD1: port member 0x50 (0101 0000)
	mii_mgr_write(31, 0x90, 0x80001003); // VTCR: write VID=3
#endif
#if defined (RALINK_PVLAN_LLLXX)
	/* VLAN 2: P3 to nothing */
	mii_mgr_write(31, 0x94, 0x40080001); // VAWD1: port member 0x08 (0000 1000)
	mii_mgr_write(31, 0x90, 0x80001002); // VTCR: write VID=2
	IsSwitchVlanTableBusy();
	/* VLAN 3: P4 to nothing */
	mii_mgr_write(31, 0x94, 0x40100001); // VAWD1: port member 0x10 (0001 0000)
	mii_mgr_write(31, 0x90, 0x80001003); // VTCR: write VID=3
#endif
	IsSwitchVlanTableBusy();
#endif

#if defined(EPHY_LINK_UP)
	// turn on GSW PHY + restart AN
	for (i = 0; i <= 4; i++)
		mii_mgr_write(i, 0x0, 0x1340);
#endif
}

static void ResetSWusingGPIOx(void)
{
#ifdef GPIOx_RESET_MODE
	/* TODO: reset MT7530 switch */
#endif // GPIOx_RESET_MODE //
}

int isDMABusy(struct eth_device* dev)
{
	u32 reg;

	reg = RALINK_REG(PDMA_GLO_CFG);

	if((reg & RX_DMA_BUSY)){
		return 1;
	}

	if((reg & TX_DMA_BUSY)){
		printf("\n  TX_DMA_BUSY !!! ");
		return 1;
	}
	return 0;
}

void setup_internal_gsw(void)
{
	u32	i;
	u32	regValue;

	//enable MDIO
	RALINK_REG(RALINK_GPIOMODE_REG) &= ~(3 << 12); //set MDIO to Normal mode (0)
	RALINK_REG(RALINK_GPIOMODE_REG) &= ~(1 << 14); //set RGMII1 to Normal mode (0)

	// reset phy
	printf("\n Reset MT7530\n");
	regValue = RALINK_REG(RALINK_RSTCTRL_REG);
	if (regValue & RALINK_MCM_RST) {
		// already asserted in start_1004k.S
	} else {
		regValue |= RALINK_MCM_RST;
		RALINK_REG(RALINK_RSTCTRL_REG) = regValue;
		udelay(1000);
	}
	regValue &= ~RALINK_MCM_RST;
	RALINK_REG(RALINK_RSTCTRL_REG) = regValue;
	udelay(10000);

	/* reduce MDIO PAD driving strength */
	regValue = RALINK_REG(PAD_RGMII2_MDIO_CFG);
	regValue &= ~(0x3<<4);	// reduce Tx driving strength to 2mA (WDT_E4_E2)
	RALINK_REG(PAD_RGMII2_MDIO_CFG) = regValue;

	for (i = 0; i <= 4; i++)
	{
	    //turn off PHY
	    mii_mgr_read(i, 0x0 ,&regValue);
	    regValue |= (0x1<<11);
	    mii_mgr_write(i, 0x0, regValue);
	}

	mii_mgr_write(31, 0x3500, 0x8000);
	mii_mgr_write(31, 0x3600, 0x8000);//force MAC link down before reset

	mii_mgr_write(31, 0x7000, 0x3);//reset MT7530
	udelay(100);

	RALINK_REG(RALINK_ETH_SW_BASE+0x200) = 0x00008000;//(GE2, Force LinkDown)
	RALINK_REG(RALINK_ETH_SW_BASE+0x100) = 0x2105e33b;//(GE1, Force 1000M/FD, FC ON)
	mii_mgr_write(31, 0x3600, 0x5e33b);
	mii_mgr_write(31, 0x3500, 0x8000);
	
	RALINK_REG(GDMA1_FWD_CFG) = 0x20710000;
	RALINK_REG(GDMA2_FWD_CFG) = 0x20717777;

	/* Enable MT7530 Port 6 */
	mii_mgr_write(31, 0x7804, 0x117ccf);

	regValue = RALINK_REG(RALINK_SYSCFG_REG);
	regValue = (regValue >> 6) & 0x7;
	if(regValue >= 6) { // 6..7: 25 Mhz Xtal
		/* do nothing */
	} else if(regValue >= 3) { // 3..5: 40Mhz Xtal
	    mii_mgr_write(0, 13, 0x1f);  // disable MT7530 core clock
	    mii_mgr_write(0, 14, 0x410);
	    mii_mgr_write(0, 13, 0x401f);
	    mii_mgr_write(0, 14, 0x0);

	    mii_mgr_write(0, 13, 0x1f);  // disable MT7530 PLL
	    mii_mgr_write(0, 14, 0x40d);
	    mii_mgr_write(0, 13, 0x401f);
	    mii_mgr_write(0, 14, 0x2020);

	    mii_mgr_write(0, 13, 0x1f);  // for MT7530 core clock = 500Mhz
	    mii_mgr_write(0, 14, 0x40e);
	    mii_mgr_write(0, 13, 0x401f);
	    mii_mgr_write(0, 14, 0x119);

	    mii_mgr_write(0, 13, 0x1f);  // enable MT7530 PLL
	    mii_mgr_write(0, 14, 0x40d);
	    mii_mgr_write(0, 13, 0x401f);
	    mii_mgr_write(0, 14, 0x2820);

	    udelay(20); //suggest by CD

	    mii_mgr_write(0, 13, 0x1f);  // enable MT7530 core clock
	    mii_mgr_write(0, 14, 0x410);
	    mii_mgr_write(0, 13, 0x401f);
	    mii_mgr_write(0, 14, 0x1);
	} else { //20 Mhz Xtal
		/* TODO */
	}

	/*Tx Driving*/
	mii_mgr_write(31, 0x7a54, 0x44);  //lower driving
	mii_mgr_write(31, 0x7a5c, 0x44);  //lower driving
	mii_mgr_write(31, 0x7a64, 0x44);  //lower driving
	mii_mgr_write(31, 0x7a6c, 0x44);  //lower driving
	mii_mgr_write(31, 0x7a74, 0x44);  //lower driving
	mii_mgr_write(31, 0x7a7c, 0x44);  //lower driving

	/*Disable EEE LPI*/
	for(i=0;i<=4;i++)
	{
	    mii_mgr_write(i, 13, 0x7);
	    mii_mgr_write(i, 14, 0x3C);
	    mii_mgr_write(i, 13, 0x4007);
	    mii_mgr_write(i, 14, 0x0);
	}

	mii_mgr_read(31, 0x7808 ,&regValue);
	regValue |= (3<<16); //Enable INTR
	mii_mgr_write(31, 0x7808 ,regValue);
}

static int rt2880_eth_setup(struct eth_device* dev)
{
	u32	i;
	u32	regValue;
	u16	wTmp;
	uchar	*temp;

//	printf("\n Waitting for RX_DMA_BUSY status Start... ");
	for (i=0; i<1000; i++) {
		if(!isDMABusy(dev))
			break;
		udelay(500);
	}
//	printf("done\n\n");

	//enable MDIO
	RALINK_REG(0xbe000060) &= ~(3 << 12); //set MDIO to Normal mode
	RALINK_REG(0xbe000060) &= ~(1 << 14); //set RGMII1 to Normal mode

#if !defined (EPHY_LINK_UP)
	// turn on PHY + restart AN

	// MT7530
	for (i = 0; i <= 4; i++ )
		mii_mgr_write(i, 0x0, 0x1340);

#endif /* !EPHY_LINK_UP */

	/* Set MAC address. */
	wTmp = (u16)dev->enetaddr[0];
	regValue = (wTmp << 8) | dev->enetaddr[1];
	RALINK_REG(GDMA1_MAC_ADRH)=regValue;

	wTmp = (u16)dev->enetaddr[2];
	regValue = (wTmp << 8) | dev->enetaddr[3];
	regValue = regValue << 16;
	wTmp = (u16)dev->enetaddr[4];
	regValue |= (wTmp<<8) | dev->enetaddr[5];
	RALINK_REG(GDMA1_MAC_ADRL)=regValue;

	regValue = RALINK_REG(GDMA1_FWD_CFG);

	//Uni-cast frames forward to CPU
	regValue &= GDM_UFRC_P_CPU;
	//Broad-cast MAC address frames forward to CPU
	regValue &= GDM_BFRC_P_CPU;
	//Multi-cast MAC address frames forward to CPU
	regValue &= GDM_MFRC_P_CPU;
	//Other MAC address frames forward to CPU
	regValue &= GDM_OFRC_P_CPU;

	RALINK_REG(GDMA1_FWD_CFG)=regValue;
	udelay(500);
	regValue = RALINK_REG(GDMA1_FWD_CFG);

	for (i = 0; i < NUM_RX_DESC; i++) {
		temp = memset((void *)&rx_ring[i],0,16);
		rx_ring[i].rxd_info2.DDONE_bit = 0;

		{
			BUFFER_ELEM *buf;
			buf = rt2880_free_buf_entry_dequeue(&rt2880_free_buf_list);
			NetRxPackets[i] = buf->pbuf;
			rx_ring[i].rxd_info2.LS0= 0;
			rx_ring[i].rxd_info2.PLEN0= PKTSIZE_ALIGN;
			rx_ring[i].rxd_info1.PDP0 = cpu_to_le32(phys_to_bus((u32) NetRxPackets[i]));
		}
	}

	for (i = 0; i < NUM_TX_DESC; i++) {
		temp = memset((void *)&tx_ring0[i],0,16);
		tx_ring0[i].txd_info2.LS0_bit = 1;
		tx_ring0[i].txd_info2.DDONE_bit = 1;
		/* PN:
		 *  0:CPU
		 *  1:GE1
		 *  2:GE2 (for RT2883)
		 *  6:PPE
		 *  7:Discard
		 */
		tx_ring0[i].txd_info4.FPORT=1;
	}

	rxRingSize = NUM_RX_DESC;
	txRingSize = NUM_TX_DESC;

	rx_dma_owner_idx0 = 0;
	rx_wants_alloc_idx0 = (NUM_RX_DESC - 1);
	tx_cpu_owner_idx0 = 0;

	regValue=RALINK_REG(PDMA_GLO_CFG);
	udelay(100);

	{
		regValue &= 0x0000FFFF;

		RALINK_REG(PDMA_GLO_CFG)=regValue;
		udelay(500);
		regValue=RALINK_REG(PDMA_GLO_CFG);
	}

	/* Tell the adapter where the TX/RX rings are located. */
	RALINK_REG(RX_BASE_PTR0)=phys_to_bus((u32) &rx_ring[0]);

	//printf("\n rx_ring=%08X ,RX_BASE_PTR0 = %08X \n",&rx_ring[0],RALINK_REG(RX_BASE_PTR0));
	RALINK_REG(TX_BASE_PTR0)=phys_to_bus((u32) &tx_ring0[0]);

	//printf("\n tx_ring0=%08X, TX_BASE_PTR0 = %08X \n",&tx_ring0[0],RALINK_REG(TX_BASE_PTR0));

	RALINK_REG(RX_MAX_CNT0)=cpu_to_le32((u32) NUM_RX_DESC);
	RALINK_REG(TX_MAX_CNT0)=cpu_to_le32((u32) NUM_TX_DESC);

	RALINK_REG(TX_CTX_IDX0)=cpu_to_le32((u32) tx_cpu_owner_idx0);
	RALINK_REG(PDMA_RST_IDX)=cpu_to_le32((u32)RST_DTX_IDX0);

	RALINK_REG(RX_CALC_IDX0)=cpu_to_le32((u32) (NUM_RX_DESC - 1));
	RALINK_REG(PDMA_RST_IDX)=cpu_to_le32((u32)RST_DRX_IDX0);
	
	udelay(500);
	START_ETH(dev);
	
	return 1;
}

static int rt2880_eth_send(struct eth_device* dev, volatile void *packet, int length)
{
	int		status = -1;
	int		i;
	int		retry_count = 0, temp;

Retry:
	if (retry_count > 10) {
		return (status);
	}

	if (length <= 0) {
		printf("%s: bad packet size: %d\n", dev->name, length);
		return (status);
	}

	for(i = 0; tx_ring0[tx_cpu_owner_idx0].txd_info2.DDONE_bit == 0 ; i++)
	{
		if (i >= TOUT_LOOP) {
			//printf("%s: TX DMA is Busy !! TX desc is Empty!\n", dev->name);
			goto Done;
		}
	}
	//dump_reg();

	temp = RALINK_REG(TX_DTX_IDX0);

	if(temp == (tx_cpu_owner_idx0+1) % NUM_TX_DESC) {
		puts(" @ ");
		goto Done;
	}

	tx_ring0[tx_cpu_owner_idx0].txd_info1.SDP0 = cpu_to_le32(phys_to_bus((u32) packet));
	tx_ring0[tx_cpu_owner_idx0].txd_info2.SDL0 = length;

#if 0
	printf("==========TX==========(CTX=%d)\n",tx_cpu_owner_idx0);
	printf("tx_ring0[tx_cpu_owner_idx0].txd_info1 =%x\n",tx_ring0[tx_cpu_owner_idx0].txd_info1);
	printf("tx_ring0[tx_cpu_owner_idx0].txd_info2 =%x\n",tx_ring0[tx_cpu_owner_idx0].txd_info2);
	printf("tx_ring0[tx_cpu_owner_idx0].txd_info3 =%x\n",tx_ring0[tx_cpu_owner_idx0].txd_info3);
	printf("tx_ring0[tx_cpu_owner_idx0].txd_info4 =%x\n",tx_ring0[tx_cpu_owner_idx0].txd_info4);
#endif

	tx_ring0[tx_cpu_owner_idx0].txd_info2.DDONE_bit = 0;
	status = length;

	tx_cpu_owner_idx0 = (tx_cpu_owner_idx0+1) % NUM_TX_DESC;
	RALINK_REG(TX_CTX_IDX0)=cpu_to_le32((u32) tx_cpu_owner_idx0);

	return status;
Done:
	udelay(500);
	retry_count++;
	goto Retry;
}


static int rt2880_eth_recv(struct eth_device* dev)
{
	int length = 0,hdr_len=0,bb=0;
	int inter_loopback_cnt =0;
	u32 *rxd_info;

	for (; ; ) {
		rxd_info = (u32 *)KSEG1ADDR(&rx_ring[rx_dma_owner_idx0].rxd_info2);

		if ( (*rxd_info & BIT(31)) == 0 )
		{
			hdr_len =0;
			break;
		}

		udelay(1);
			length = rx_ring[rx_dma_owner_idx0].rxd_info2.PLEN0;

		if(length == 0)
		{
			printf("\n Warring!! Packet Length has error !!,In normal mode !\n");
		}

		if(rx_ring[rx_dma_owner_idx0].rxd_info4.SP == 6)
		{// Packet received from CPU port
			printf("\n Normal Mode,Packet received from CPU port,plen=%d \n",length);
			//print_packet((void *)KSEG1ADDR(NetRxPackets[rx_dma_owner_idx0]),length);
			inter_loopback_cnt++;
			length = inter_loopback_cnt;//for return
		}
		else {
			NetReceive((void *)KSEG1ADDR(NetRxPackets[rx_dma_owner_idx0]), length );
		}

		rx_ring[rx_dma_owner_idx0].rxd_info2.DDONE_bit = 0;
		rx_ring[rx_dma_owner_idx0].rxd_info2.LS0= 0;
		rx_ring[rx_dma_owner_idx0].rxd_info2.PLEN0= PKTSIZE_ALIGN;

#if 0
		printf("=====RX=======(CALC=%d LEN=%d)\n",rx_dma_owner_idx0, length);
		printf("rx_ring[rx_dma_owner_idx0].rxd_info1 =%x\n",rx_ring[rx_dma_owner_idx0].rxd_info1);
		printf("rx_ring[rx_dma_owner_idx0].rxd_info2 =%x\n",rx_ring[rx_dma_owner_idx0].rxd_info2);
		printf("rx_ring[rx_dma_owner_idx0].rxd_info3 =%x\n",rx_ring[rx_dma_owner_idx0].rxd_info3);
		printf("rx_ring[rx_dma_owner_idx0].rxd_info4 =%x\n",rx_ring[rx_dma_owner_idx0].rxd_info4);
#endif
		/* Tell the adapter where the TX/RX rings are located. */
		RALINK_REG(RX_BASE_PTR0)=phys_to_bus((u32) &rx_ring[0]);

		//udelay(10000);
		/*  Move point to next RXD which wants to alloc*/
		RALINK_REG(RX_CALC_IDX0)=cpu_to_le32((u32) rx_dma_owner_idx0);

		/* Update to Next packet point that was received.
		 */
		rx_dma_owner_idx0 = (rx_dma_owner_idx0 + 1) % NUM_RX_DESC;

		//printf("\n ************************************************* \n");
		//printf("\n RX_CALC_IDX0=%d \n", RALINK_REG(RX_CALC_IDX0));
		//printf("\n RX_DRX_IDX0 = %d \n",RALINK_REG(RX_DRX_IDX0));
		//printf("\n ************************************************* \n");
	}
	return length;
}

void rt2880_eth_halt(struct eth_device* dev)
{
	 STOP_ETH(dev);
	//gmac_phy_switch_gear(DISABLE);
	//printf(" STOP_ETH \n");
	//dump_reg();
}

#if 0
static void print_packet( u8 * buf, int length )
{

	int i;
	int remainder;
	int lines;


	printf("Packet of length %d \n", length );


	lines = length / 16;
	remainder = length % 16;

	for ( i = 0; i < lines ; i ++ ) {
		int cur;

		for ( cur = 0; cur < 8; cur ++ ) {
			u8 a, b;

			a = *(buf ++ );
			b = *(buf ++ );
			printf("%02X %02X ", a, b );
		}
		printf("\n");
	}
	for ( i = 0; i < remainder/2 ; i++ ) {
		u8 a, b;

		a = *(buf ++ );
		b = *(buf ++ );
		printf("%02X %02X ", a, b );
	}
	printf("\n");

}
#endif

static char erase_seq[] = "\b \b";              /* erase sequence       */
static char   tab_seq[] = "        ";           /* used to expand TABs  */

static char * delete_char (char *buffer, char *p, int *colp, int *np, int plen)
{
	char *s;

	if (*np == 0) {
		return (p);
	}

	if (*(--p) == '\t') {			/* will retype the whole line	*/
		while (*colp > plen) {
			puts (erase_seq);
			(*colp)--;
		}
		for (s=buffer; s<p; ++s) {
			if (*s == '\t') {
				puts (tab_seq+((*colp) & 07));
				*colp += 8 - ((*colp) & 07);
			} else {
				++(*colp);
				putc (*s);
			}
		}
	} else {
		puts (erase_seq);
		(*colp)--;
	}
	(*np)--;
	return (p);
}

/*
 * Prompt for input and read a line.
 * If  CONFIG_BOOT_RETRY_TIME is defined and retry_time >= 0,
 * time out when time goes past endtime (timebase time in ticks).
 * Return:	number of read characters
 *		-1 if break
 *		-2 if timed out
 */
int readline (const char *const prompt, int show_buf)
{
	char   *p = console_buffer;
	int	n = 0;				/* buffer index		*/
	int	plen = 0;			/* prompt length	*/
	int	col;				/* output column cnt	*/
	char	c;

	/* print prompt */
	if (prompt) {
		plen = strlen (prompt);
		puts (prompt);
	}
	if (show_buf) {
		puts (p);
		n = strlen(p);
		col = plen + strlen(p);
		p += strlen(p);
	}
	else
		col = plen;

	for (;;) {
#ifdef CONFIG_BOOT_RETRY_TIME
		while (!tstc()) {	/* while no incoming data */
			if (retry_time >= 0 && get_ticks() > endtime)
				return (-2);	/* timed out */
		}
#endif
//		WATCHDOG_RESET();		/* Trigger watchdog, if needed */

#ifdef CONFIG_SHOW_ACTIVITY
		while (!tstc()) {
			extern void show_activity(int arg);
			show_activity(0);
		}
#endif
		c = getc();

		/*
		 * Special character handling
		 */
		switch (c) {
		case '\r':				/* Enter		*/
		case '\n':
			*p = '\0';
			puts ("\r\n");
#ifdef CONFIG_CMD_HISTORY
			if (history_counter < HISTORY_SIZE) history_counter++;
			history_last_idx++;
			history_last_idx %= HISTORY_SIZE;
			history_cur_idx = history_last_idx;
			strcpy(&console_history[history_last_idx][0], console_buffer);
#endif
			return (p - console_buffer);

		case '\0':				/* nul			*/
			continue;

		case 0x03:				/* ^C - break		*/
			console_buffer[0] = '\0';	/* discard input */
			return (-1);

		case 0x15:				/* ^U - erase line	*/
			while (col > plen) {
				puts (erase_seq);
				--col;
			}
			p = console_buffer;
			n = 0;
			continue;

		case 0x17:				/* ^W - erase word 	*/
			p=delete_char(console_buffer, p, &col, &n, plen);
			while ((n > 0) && (*p != ' ')) {
				p=delete_char(console_buffer, p, &col, &n, plen);
			}
			continue;

		case 0x08:				/* ^H  - backspace	*/
		case 0x7F:				/* DEL - backspace	*/
			p=delete_char(console_buffer, p, &col, &n, plen);
			continue;

#ifdef CONFIG_CMD_HISTORY
		case 0x1B:	// ESC : ^[
			history_enable = 1;
			break;
		case 0x5B: // [
			if (history_enable == 0)
				goto normal_cond;
			break;
		case 0x41:  // up [0x1b 0x41]
		case 0x42:	// down [0x1b 0x41]
			if (history_enable == 0)
				goto normal_cond;

			if (history_last_idx == -1)
				break;

			if (c == 0x41) {
				if (history_cur_idx > 0) 
					history_cur_idx--;
				else
					history_cur_idx = history_counter-1;									
			} else {
				if (history_cur_idx < history_counter-1) 
					history_cur_idx++;
				else
					history_cur_idx = 0;													
			}											

			while (col > plen) {
				puts (erase_seq);
				--col;
			}			
			strcpy(console_buffer, &console_history[history_cur_idx][0]);
			puts(console_buffer);
			n = strlen(console_buffer);
			p = console_buffer+n; 
			col = n+plen;	
			history_enable = 0;
			break;
normal_cond:
#endif
		default:
			/*
			 * Must be a normal character then
			 */
#ifdef CONFIG_CMD_HISTORY
			history_enable = 0;
#endif
			if (n < CFG_CBSIZE-2) {
				if (c == '\t') {	/* expand TABs		*/
#ifdef CONFIG_AUTO_COMPLETE
					/* if auto completion triggered just continue */
					*p = '\0';
					if (cmd_auto_complete(prompt, console_buffer, &n, &col)) {
						p = console_buffer + n;	/* reset */
						continue;
					}
#endif
					puts (tab_seq+(col&07));
					col += 8 - (col&07);
				} else {
					++col;		/* echo input		*/
					putc (c);					
				}
				*p++ = c;
				++n;
			} else {			/* Buffer full		*/
				putc ('\a');
			}
		}
	}
}

void input_value(u8 *str)
{
	if (str)
		strcpy(console_buffer, str);
	else
		console_buffer[0] = '\0';
	while(1)
	{
		if (readline ("==:", 1) > 0)
		{
			strcpy (str, console_buffer);
			break;
		}
		else
			break;
	}
}

#ifdef RALINK_SWITCH_DEBUG_FUN
#define RALINK_VLAN_ID_BASE	(RALINK_ETH_SW_BASE + 0x50)
#define RALINK_VLAN_MEMB_BASE	(RALINK_ETH_SW_BASE + 0x70)

#define RALINK_TABLE_SEARCH	(RALINK_ETH_SW_BASE + 0x24)
#define RALINK_TABLE_STATUS0	(RALINK_ETH_SW_BASE + 0x28)
#define RALINK_TABLE_STATUS1	(RALINK_ETH_SW_BASE + 0x2c)
#define RALINK_TABLE_STATUS2	(RALINK_ETH_SW_BASE + 0x30)
#define RALINK_WT_MAC_AD0	(RALINK_ETH_SW_BASE + 0x34)
#define RALINK_WT_MAC_AD1	(RALINK_ETH_SW_BASE + 0x38)
#define RALINK_WT_MAC_AD2	(RALINK_ETH_SW_BASE + 0x3C)
#define RALINK_WT_MAC_AD2	(RALINK_ETH_SW_BASE + 0x3C)

void table_dump(void)
{
	int i, j, value, mac;
	int vid[16];

	for (i = 0; i < 8; i++) {
		value = RALINK_REG(RALINK_VLAN_ID_BASE + 4*i);
		vid[2 * i] = value & 0xfff;
		vid[2 * i + 1] = (value & 0xfff000) >> 12;
	}

	RALINK_REG(RALINK_TABLE_SEARCH) = 0x1;
	printf("hash  port(0:6)  vidx  vid  age   mac-address  filt\n");
	for (i = 0; i < 0x400; i++) {
		while(1) {
			value = RALINK_REG(RALINK_TABLE_STATUS0);
			if (value & 0x1) { //search_rdy
				if ((value & 0x70) == 0) {
					printf("found an unused entry (age = 3'b000), please check!\n");
					return;
				}
				printf("%03x:   ", (value >> 22) & 0x3ff); //hash_addr_lu
				j = (value >> 12) & 0x7f; //r_port_map
				printf("%c", (j & 0x01)? '1':'-');
				printf("%c", (j & 0x02)? '1':'-');
				printf("%c", (j & 0x04)? '1':'-');
				printf("%c", (j & 0x08)? '1':'-');
				printf("%c ", (j & 0x10)? '1':'-');
				printf("%c", (j & 0x20)? '1':'-');
				printf("%c", (j & 0x40)? '1':'-');
				printf("   %2d", (value >> 7) & 0xf); //r_vid
				printf("  %4d", vid[(value >> 7) & 0xf]);
				printf("    %1d", (value >> 4) & 0x7); //r_age_field
				mac = RALINK_REG(RALINK_TABLE_STATUS2);
				printf("  %08x", mac);
				mac = RALINK_REG(RALINK_TABLE_STATUS1);
				printf("%04x", (mac & 0xffff));
				printf("     %c\n", (value & 0x8)? 'y':'-');
				if (value & 0x2) {
					printf("end of table %d\n", i);
					return;
				}
				break;
			}
			else if (value & 0x2) { //at_table_end
				printf("found the last entry %d (not ready)\n", i);
				return;
			}
			udelay(5000);
		}
		RALINK_REG(RALINK_TABLE_SEARCH) = 0x2; //search for next address
	}
}

void table_add(int argc, char *argv[])
{
	int i, j, value, is_filter;
	char tmpstr[9];

	is_filter = (argv[1][0] == 'f')? 1 : 0;
	if (!argv[2] || strlen(argv[2]) != 12) {
		printf("MAC address format error, should be of length 12\n");
		return;
	}
	strncpy(tmpstr, argv[2], 8);
	tmpstr[8] = '\0';
	value = simple_strtoul(tmpstr, NULL, 16);
	RALINK_REG(RALINK_WT_MAC_AD2) = value;

	strncpy(tmpstr, argv[2]+8, 4);
	tmpstr[4] = '\0';
	value = simple_strtoul(tmpstr, NULL, 16);
	RALINK_REG(RALINK_WT_MAC_AD1) = value;

	if (!argv[3] || strlen(argv[3]) != 7) {
		if (is_filter)
			argv[3] = "1111111";
		else {
			printf("portmap format error, should be of length 7\n");
			return;
		}
	}
	j = 0;
	for (i = 0; i < 7; i++) {
		if (argv[3][i] != '0' && argv[3][i] != '1') {
			printf("portmap format error, should be of combination of 0 or 1\n");
			return;
		}
		j += (argv[3][i] - '0') * (1 << i);
	}
	value = j << 12; //w_port_map

	if (argc > 4) {
		j = simple_strtoul(argv[4], NULL, 0);
		if (j < 0 || 15 < j) {
			printf("wrong member index range, should be within 0~15\n");
			return;
		}
		value += (j << 7); //w_index
	}

	if (argc > 5) {
		j = simple_strtoul(argv[5], NULL, 0);
		if (j < 1 || 7 < j) {
			printf("wrong age range, should be within 1~7\n");
			return;
		}
		value += (j << 4); //w_age_field
	}
	else
		value += (7 << 4); //w_age_field

	if (is_filter)
		value |= (1 << 3); //sa_filter

	value += 1; //w_mac_cmd
	RALINK_REG(RALINK_WT_MAC_AD0) = value;

	for (i = 0; i < 20; i++) {
		value = RALINK_REG(RALINK_WT_MAC_AD0);
		if (value & 0x2) { //w_mac_done
			printf("done.\n");
			return;
		}
		udelay(1000);
	}
	if (i == 20)
		printf("timeout.\n");
}

void table_del(int argc, char *argv[])
{
	int i, j, value;
	char tmpstr[9];

	if (!argv[2] || strlen(argv[2]) != 12) {
		printf("MAC address format error, should be of length 12\n");
		return;
	}
	strncpy(tmpstr, argv[2], 8);
	tmpstr[8] = '\0';
	value = simple_strtoul(tmpstr, NULL, 16);
	RALINK_REG(RALINK_WT_MAC_AD2) = value;

	strncpy(tmpstr, argv[2]+8, 4);
	tmpstr[4] = '\0';
	value = simple_strtoul(tmpstr, NULL, 16);
	RALINK_REG(RALINK_WT_MAC_AD1) = value;

	value = 0;
	if (argc > 3) {
		j = simple_strtoul(argv[3], NULL, 0);
		if (j < 0 || 15 < j) {
			printf("wrong member index range, should be within 0~15\n");
			return;
		}
		value += (j << 7); //w_index
	}

	value += 1; //w_mac_cmd
	RALINK_REG(RALINK_WT_MAC_AD0) = value;

	for (i = 0; i < 20; i++) {
		value = RALINK_REG(RALINK_WT_MAC_AD0);
		if (value & 0x2) { //w_mac_done
			if (argv[1] != NULL)
				printf("done.\n");
			return;
		}
		udelay(1000);
	}
	if (i == 20)
		printf("timeout.\n");
}

void table_clear(void)
{
	int i, value, mac;
	char v[2][13];
	char *argv[4];

	memset(argv, 0, sizeof(v));
	memset(argv, 0, sizeof(argv));

	RALINK_REG(RALINK_TABLE_SEARCH) = 0x1;
	for (i = 0; i < 0x400; i++) {
		while(1) {
			value = RALINK_REG(RALINK_TABLE_STATUS0);
			if (value & 0x1) { //search_rdy
				if ((value & 0x70) == 0) {
					return;
				}
				sprintf(v[1], "%d", (value >> 7) & 0xf);
				mac = RALINK_REG(RALINK_TABLE_STATUS2);
				sprintf(v[0], "%08x", mac);
				mac = RALINK_REG(RALINK_TABLE_STATUS1);
				sprintf(v[0]+8, "%04x", (mac & 0xffff));
				argv[2] = v[0];
				argv[3] = v[1];
				table_del(4, argv);
				if (value & 0x2) {
					return;
				}
				break;
			}
			else if (value & 0x2) { //at_table_end
				return;
			}
			udelay(5000);
		}
		RALINK_REG(RALINK_TABLE_SEARCH) = 0x2; //search for next address
	}
}

void vlan_dump(void)
{
	int i, vid, value;

	printf("idx   vid  portmap\n");
	for (i = 0; i < 8; i++) {
		vid = RALINK_REG(RALINK_VLAN_ID_BASE + 4*i);
		value = RALINK_REG(RALINK_VLAN_MEMB_BASE + 4*(i/2));
		printf(" %2d  %4d  ", 2*i, vid & 0xfff);
		if (i%2 == 0) {
			printf("%c", (value & 0x00000001)? '1':'-');
			printf("%c", (value & 0x00000002)? '1':'-');
			printf("%c", (value & 0x00000004)? '1':'-');
			printf("%c", (value & 0x00000008)? '1':'-');
			printf("%c", (value & 0x00000010)? '1':'-');
			printf("%c", (value & 0x00000020)? '1':'-');
			printf("%c\n", (value & 0x00000040)? '1':'-');
		}
		else {
			printf("%c", (value & 0x00010000)? '1':'-');
			printf("%c", (value & 0x00020000)? '1':'-');
			printf("%c", (value & 0x00040000)? '1':'-');
			printf("%c", (value & 0x00080000)? '1':'-');
			printf("%c", (value & 0x00100000)? '1':'-');
			printf("%c", (value & 0x00200000)? '1':'-');
			printf("%c\n", (value & 0x00400000)? '1':'-');
		}
		printf(" %2d  %4d  ", 2*i+1, ((vid & 0xfff000) >> 12));
		if (i%2 == 0) {
			printf("%c", (value & 0x00000100)? '1':'-');
			printf("%c", (value & 0x00000200)? '1':'-');
			printf("%c", (value & 0x00000400)? '1':'-');
			printf("%c", (value & 0x00000800)? '1':'-');
			printf("%c", (value & 0x00001000)? '1':'-');
			printf("%c", (value & 0x00002000)? '1':'-');
			printf("%c\n", (value & 0x00004000)? '1':'-');
		}
		else {
			printf("%c", (value & 0x01000000)? '1':'-');
			printf("%c", (value & 0x02000000)? '1':'-');
			printf("%c", (value & 0x04000000)? '1':'-');
			printf("%c", (value & 0x08000000)? '1':'-');
			printf("%c", (value & 0x10000000)? '1':'-');
			printf("%c", (value & 0x20000000)? '1':'-');
			printf("%c\n", (value & 0x40000000)? '1':'-');
		}
	}
}

void vlan_set(int argc, char *argv[])
{
	int i, j, value;
	int idx, vid;

	if (argc != 6) {
		printf("insufficient arguments!\n");
		return;
	}
	idx = simple_strtoul(argv[3], NULL, 0);
	if (idx < 0 || 15 < idx) {
		printf("wrong member index range, should be within 0~15\n");
		return;
	}
	vid = simple_strtoul(argv[4], NULL, 0);
	if (vid < 0 || 0xfff < vid) {
		printf("wrong vlan id range, should be within 0~4095\n");
		return;
	}
	if (strlen(argv[5]) != 7) {
		printf("portmap format error, should be of length 7\n");
		return;
	}
	j = 0;
	for (i = 0; i < 7; i++) {
		if (argv[5][i] != '0' && argv[5][i] != '1') {
			printf("portmap format error, should be of combination of 0 or 1\n");
			return;
		}
		j += (argv[5][i] - '0') * (1 << i);
	}

	//set vlan identifier
	value = RALINK_REG(RALINK_VLAN_ID_BASE + 4*(idx/2));
	if (idx % 2 == 0) {
		value &= 0xfff000;
		value |= vid;
	}
	else {
		value &= 0xfff;
		value |= (vid << 12);
	}
	RALINK_REG(RALINK_VLAN_ID_BASE + 4*(idx/2)) = value;

	//set vlan member
	value = RALINK_REG(RALINK_VLAN_MEMB_BASE + 4*(idx/4));
	if (idx % 4 == 0) {
		value &= 0xffffff00;
		value |= j;
	}
	else if (idx % 4 == 1) {
		value &= 0xffff00ff;
		value |= (j << 8);
	}
	else if (idx % 4 == 2) {
		value &= 0xff00ffff;
		value |= (j << 16);
	}
	else {
		value &= 0x00ffffff;
		value |= (j << 24);
	}
	RALINK_REG(RALINK_VLAN_MEMB_BASE + 4*(idx/4)) = value;
}

int rt3052_switch_command(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}
	if (argc == 2) {
		if (!strncmp(argv[1], "dump", 5))
			table_dump();
		else if (!strncmp(argv[1], "clear", 6)) {
			table_clear();
			printf("done.\n");
		}
		else {
			printf ("Usage:\n%s\n", cmdtp->usage);
			return 1;
		}
	}
	else if (!strncmp(argv[1], "add", 4))
		table_add(argc, argv);
	else if (!strncmp(argv[1], "filt", 5))
		table_add(argc, argv);
	else if (!strncmp(argv[1], "del", 4))
		table_del(argc, argv);
	else if (!strncmp(argv[1], "vlan", 5)) {
		if (argc < 3)
			printf ("Usage:\n%s\n", cmdtp->usage);
		if (!strncmp(argv[2], "dump", 5))
			vlan_dump();
		else if (!strncmp(argv[2], "set", 4))
			vlan_set(argc, argv);
		else
			printf ("Usage:\n%s\n", cmdtp->usage);
	}
	else {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}
	return 0;
}

U_BOOT_CMD(
 	switch,	6,	1,	rt3052_switch_command,
 	"switch  - rt3052 embedded switch command\n",
 	"switch dump - dump switch table\n"
	"switch clear - clear switch table\n"
 	"switch add [mac] [portmap] - add an entry to switch table\n"
 	"switch add [mac] [portmap] [vlan idx] - add an entry to switch table\n"
 	"switch add [mac] [portmap] [vlan idx] [age] - add an entry to switch table\n"
 	"switch filt [mac] - add an SA filtering entry (with portmap 1111111) to switch table\n"
 	"switch filt [mac] [portmap] - add an SA filtering entry to switch table\n"
 	"switch filt [mac] [portmap] [vlan idx] - add an SA filtering entry to switch table\n"
 	"switch filt [mac] [portmap] [vlan idx] [age] - add an SA filtering entry to switch table\n"
 	"switch del [mac] - delete an entry from switch table\n"
 	"switch del [mac] [vlan idx] - delete an entry from switch table\n"
	"switch vlan dump - dump switch table\n"
	"switch vlan set [vlan idx] [vid] [portmap] - set vlan id and associated member\n"
);
#endif // RALINK_SWITCH_DEBUG_FUN //

#endif	/* CONFIG_TULIP */
