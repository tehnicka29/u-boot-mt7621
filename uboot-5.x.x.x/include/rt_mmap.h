/**************************************************************************
 *
 *  BRIEF MODULE DESCRIPTION
 *     register definition for Ralink RT-series SoC
 *
 *  Copyright 2007 Ralink Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 **************************************************************************
 */

#ifndef __RALINK_MMAP__
#define __RALINK_MMAP__

#define RALINK_SYSCTL_BASE              0xBE000000
#define RALINK_TIMER_BASE               0xBE000100
#define RALINK_INTCL_BASE               0xBE000200
#define RALINK_RBUS_MATRIXCTL_BASE      0xBE000400
#define RALINK_MIPS_CNT_BASE            0x1E000500
#define RALINK_PIO_BASE                 0xBE000600
#define RALINK_SPDIF_BASE               0xBE000700
#define RALINK_DMA_ARB_BASE             0xBE000800
#define RALINK_I2C_BASE                 0xBE000900
#define RALINK_I2S_BASE                 0xBE000A00
#define RALINK_SPI_BASE                 0xBE000B00
#define RALINK_UART_LITE_BASE           0xBE000C00
#define RALINK_UART_LITE2_BASE          0xBE000D00
#define RALINK_UART_LITE3_BASE          0xBE000E00
#define RALINK_PCM_BASE                 0xBE002000
#define RALINK_GDMA_BASE                0xBE002800
#define RALINK_NAND_CTRL_BASE           0xBE003000
#define RALINK_NANDECC_CTRL_BASE        0xBE003800
#define RALINK_CRYPTO_ENGINE_BASE       0xBE004000
#define RALINK_MEMCTRL_BASE             0xBE005000
#define RALINK_FRAME_ENGINE_BASE        0xBE100000
#define RALINK_ETH_GMAC_BASE            0xBE110000
#define RALINK_ETH_SW_BASE              0xBE110000
#define RALINK_ROM_BASE                 0xBE118000
#define RALINK_MSDC_BASE                0xBE130000
#define RALINK_PCI_BASE                 0xBE140000
#define RALINK_USB_HOST_BASE            0x1E1C0000

#define RALINK_CHIP_ID_0_3      (RALINK_SYSCTL_BASE+0x00) /* Chip ID ASCII chars 0..3 */
#define RALINK_CHIP_ID_4_7      (RALINK_SYSCTL_BASE+0x04) /* Chip ID ASCII chars 4..7 */
#define RALINK_CHIP_REV_ID      (RALINK_SYSCTL_BASE+0x0C) /* Chip Revision ID */
#define RALINK_SYSCFG_REG       (RALINK_SYSCTL_BASE+0x10) /* System Configuration Register #0 */
#define RALINK_SYSCFG1_REG      (RALINK_SYSCTL_BASE+0x14) /* System Configuration Register #1 */
#define RALINK_CLKCFG0_REG      (RALINK_SYSCTL_BASE+0x2C) /* Clock Configuration Register #0 */
#define RALINK_CLKCFG1_REG      (RALINK_SYSCTL_BASE+0x30) /* Clock Configuration Register #1 */
#define RALINK_RSTCTRL_REG      (RALINK_SYSCTL_BASE+0x34) /* Reset Control Register */
#define RALINK_RSTSTAT_REG      (RALINK_SYSCTL_BASE+0x38) /* Reset Status Register */
#define RALINK_CPLLCFG0_REG     (RALINK_SYSCTL_BASE+0x54)
#define RALINK_CPLLCFG1_REG     (RALINK_SYSCTL_BASE+0x58)
#define RALINK_GPIOMODE_REG     (RALINK_SYSCTL_BASE+0x60) /* GPIO Mode Control Register */
#define RALINK_CUR_CLK_STS_REG  (RALINK_SYSCTL_BASE+0x44) /* Current Clock Status Register */

/* Timer Controls */
#define RALINK_TGLB_REG         (RALINK_TIMER_BASE+0x00) /* RISC Global Control Register */
#define RALINK_WDTCTL_REG       (RALINK_TIMER_BASE+0x20) /* WDT Control Register */
#define RALINK_WDTLMT_REG       (RALINK_TIMER_BASE+0x24) /* WDT Limit Register */
#define RALINK_WDT_REG          (RALINK_TIMER_BASE+0x28) /* WDT Register */

/* CPU PLL Control */
#define RALINK_DYN_CFG0_REG             0xBE000410

/* Interrupt Controller */
#define RALINK_INTCTL_SYSCTL            (1<<0)
#define RALINK_INTCTL_TIMER0            (1<<1)
#define RALINK_INTCTL_WDTIMER           (1<<2)
#define RALINK_INTCTL_ILL_ACCESS        (1<<3)
#define RALINK_INTCTL_PCM               (1<<4)
#define RALINK_INTCTL_UART              (1<<5)
#define RALINK_INTCTL_PIO               (1<<6)
#define RALINK_INTCTL_DMA               (1<<7)
#define RALINK_INTCTL_PC                (1<<9)
#define RALINK_INTCTL_I2S               (1<<10)
#define RALINK_INTCTL_SPI               (1<<11)
#define RALINK_INTCTL_UARTLITE          (1<<12)
#define RALINK_INTCTL_CRYPTO            (1<<13)
#define RALINK_INTCTL_ESW               (1<<17)
#define RALINK_INTCTL_UHST              (1<<18)
#define RALINK_INTCTL_UDEV              (1<<19)
#define RALINK_INTCTL_GLOBAL            (1<<31)

/* Reset Control Register */
#define RALINK_SYS_RST                  (1<<0) /* Whole System Reset Control */
#define RALINK_MCM_RST                  (1<<2) /* MT7530 */
#define RALINK_HSDMA_RST                (1<<5)
#define RALINK_FE_RST                   (1<<6)
#define RALINK_SPDIFTX_RST              (1<<7)
#define RALINK_TIMER_RST                (1<<8)
#define RALINK_INTC_RST                 (1<<9)
#define RALINK_MC_RST                   (1<<10)
#define RALINK_PCM_RST                  (1<<11)
#define RALINK_UART_RST                 (1<<12)
#define RALINK_PIO_RST                  (1<<13)
#define RALINK_GDMA_RST                 (1<<14)
#define RALINK_NFI_RST                  (1<<15)
#define RALINK_I2C_RST                  (1<<16)
#define RALINK_I2S_RST                  (1<<17)
#define RALINK_SPI_RST                  (1<<18)
#define RALINK_UART1_RST                (1<<19)
#define RALINK_UART2_RST                (1<<20)
#define RALINK_UART3_RST                (1<<21)
#define RALINK_UHST_RST                 (1<<22)
#define RALINK_ESW_RST                  (1<<23) /* ETH/GMAC */
#define RALINK_PCIE0_RST                (1<<24)
#define RALINK_PCIE1_RST                (1<<25)
#define RALINK_PCIE2_RST                (1<<26)
#define RALINK_MIPS_CNT_RST             (1<<28)
#define RALINK_CRYPTO_RST               (1<<29)
#define RALINK_SDXC_RST                 (1<<30)
#define RALINK_PPE_RST                  (1<<31)

/* Clock Configuration Register #1 */
#define RALINK_HSDMA_CLK_EN             (1<<5)
#define RALINK_FE_CLK_EN                (1<<6)
#define RALINK_NAND_CLK_EN              (1<<15)
#define RALINK_I2C_CLK_EN               (1<<16)
#define RALINK_I2S_CLK_EN               (1<<17)
#define RALINK_SPI_CLK_EN               (1<<18)
#define RALINK_UART1_CLK_EN             (1<<19)
#define RALINK_UART2_CLK_EN             (1<<20)
#define RALINK_UART3_CLK_EN             (1<<21)
#define RALINK_ETH_CLK_EN               (1<<23)
#define RALINK_PCIE0_CLK_EN             (1<<24)
#define RALINK_PCIE1_CLK_EN             (1<<25)
#define RALINK_PCIE2_CLK_EN             (1<<26)
#define RALINK_CRYPTO_CLK_EN            (1<<29)
#define RALINK_SHXC_CLK_EN              (1<<30)

#endif // __RALINK_MMAP__
