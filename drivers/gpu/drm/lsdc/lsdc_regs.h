/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LSDC_REGS_H__
#define __LSDC_REGS_H__

#include <linux/bitops.h>
#include <linux/types.h>

/*
 * PIXEL PLL Reference clock
 */
#define LSDC_PLL_REF_CLK                100000           /* kHz */

/*
 * Those PLL registers are relative to LSxxxxx_CFG_REG_BASE. xxxxx = 7A1000,
 * 2K1000, 2K2000 etc
 */

#define LS2K1000_PIX_PLL0_REG           0x04B0
#define LS2K1000_PIX_PLL1_REG           0x04C0

#if defined(__mips__)
#define LS2K1000_CFG_REG_BASE           0x1fe10000    /* mips64r2 edition */
#else
#define LS2K1000_CFG_REG_BASE           0x1fe00000    /* LoongArch edition */
#endif

/* LS7A1000 and LS2K2000 */
#define LS7A1000_PIX_PLL0_REG           0x04B0
#define LS7A1000_PIX_PLL1_REG           0x04C0
#define LS7A1000_CFG_REG_BASE           0x10010000

#define CFG_PIX_FMT_MASK                GENMASK(2, 0)

enum lsdc_pixel_format {
	LSDC_PF_NONE = 0,
	LSDC_PF_ARGB4444 = 1,  /* ARGB A:4 bits R/G/B: 4 bits each [16 bits] */
	LSDC_PF_ARGB1555 = 2,  /* ARGB A:1 bit RGB:15 bits [16 bits] */
	LSDC_PF_RGB565 = 3,    /* RGB [16 bits] */
	LSDC_PF_XRGB8888 = 4,  /* XRGB [32 bits] */
	LSDC_PF_RGBA8888 = 5,  /* ARGB [32 bits] */
};

/*
 * Each crtc has two set fb address registers usable, CFG_FB_IN_USING of
 * LSDC_CRTCx_CFG_REG specify which fb address register is currently
 * in using by the CRTC. CFG_PAGE_FLIP of LSDC_CRTCx_CFG_REG is used to
 * trigger the switch which will be finished at the very vblank. If you
 * want to switch back, set CFG_PAGE_FLIP again.
 */
#define CFG_PAGE_FLIP                   BIT(7)
#define CFG_OUTPUT_EN                   BIT(8)
/* CRTC0 clone from CRTC1 or CRTC1 clone from CRTC0 using hardware logic */
#define CFG_PANEL_SWITCH                BIT(9)
/* Indicate witch fb addr reg is in using, currently */
#define CFG_FB_IN_USING                 BIT(11)
#define CFG_GAMMA_EN                    BIT(12)

/* The DC get soft reset if voltage level change from 1 -> 0 */
#define CFG_RESET_N                     BIT(20)

/* The DMA step of the DC in LS7A2000 is configurable */
#define CFG_DMA_STEP_MASK              GENMASK(17, 16)
#define CFG_DMA_STEP_SHIFT             16
enum lsdc_dma_steps {
	LSDC_DMA_STEP_256_BYTES = 0 << CFG_DMA_STEP_SHIFT,
	LSDC_DMA_STEP_128_BYTES = 1 << CFG_DMA_STEP_SHIFT,
	LSDC_DMA_STEP_64_BYTES = 2 << CFG_DMA_STEP_SHIFT,
	LSDC_DMA_STEP_32_BYTES = 3 << CFG_DMA_STEP_SHIFT,
};

#define CFG_HSYNC_EN                    BIT(30)
#define CFG_HSYNC_INV                   BIT(31)
#define CFG_VSYNC_EN                    BIT(30)
#define CFG_VSYNC_INV                   BIT(31)

/******** CRTC0 & DVO0 ********/
#define LSDC_CRTC0_CFG_REG              0x1240

/*
 * If FB0_XX_ADDR_REG is in using, we write the address to FB0_XX_ADDR_REG,
 * if FB1_XX_ADDR_REG is in using, we write the address to FB1_XX_ADDR_REG.
 * For each CRTC, the switch from using FB0_XX_ADDR_REG to FB1_XX_ADDR_REG
 * is triggered by set CFG_PAGE_FLIP bit of LSDC_CRTCx_CFG_REG, trigger it
 * again if you want switch back.
 */
#define LSDC_CRTC0_FB0_LO_ADDR_REG      0x1260
#define LSDC_CRTC0_FB0_HI_ADDR_REG      0x15A0
#define LSDC_CRTC0_FB1_LO_ADDR_REG      0x1580
#define LSDC_CRTC0_FB1_HI_ADDR_REG      0x15C0
#define LSDC_CRTC0_STRIDE_REG           0x1280
#define LSDC_CRTC0_FB_ORIGIN_REG        0x1300

/* [27:16] total number of pixels, [11:0] Active number of pixels, horizontal */
#define LSDC_CRTC0_HDISPLAY_REG         0x1400
/* [12:0] hsync start [28:16] hsync end, 30: hsync enable, 31: hsync invert */
#define LSDC_CRTC0_HSYNC_REG            0x1420
/* [27:16] total number of pixels, [11:0] Active number of pixels, vertical */
#define LSDC_CRTC0_VDISPLAY_REG         0x1480
/* [12:0] vsync start [28:16] vsync end, 30: vsync enable, 31: vsync invert */
#define LSDC_CRTC0_VSYNC_REG            0x14A0

#define LSDC_CRTC0_GAMMA_INDEX_REG      0x14E0
#define LSDC_CRTC0_GAMMA_DATA_REG       0x1500

/******** CTRC1 & DVO1 ********/
#define LSDC_CRTC1_CFG_REG              0x1250
#define LSDC_CRTC1_FB0_LO_ADDR_REG      0x1270
#define LSDC_CRTC1_FB0_HI_ADDR_REG      0x15B0
#define LSDC_CRTC1_FB1_LO_ADDR_REG      0x1590
#define LSDC_CRTC1_FB1_HI_ADDR_REG      0x15D0
#define LSDC_CRTC1_STRIDE_REG           0x1290
#define LSDC_CRTC1_FB_ORIGIN_REG        0x1310
#define LSDC_CRTC1_HDISPLAY_REG         0x1410
#define LSDC_CRTC1_HSYNC_REG            0x1430
#define LSDC_CRTC1_VDISPLAY_REG         0x1490
#define LSDC_CRTC1_VSYNC_REG            0x14B0
#define LSDC_CRTC1_GAMMA_INDEX_REG      0x14F0
#define LSDC_CRTC1_GAMMA_DATA_REG       0x1510

/*
 * LS7A2000 has the hardware which record the scan position of the CRTC
 * [31:16] : current X position, [15:0] : current Y position
 */
#define LSDC_CRTC0_SCAN_POS_REG         0x14C0
#define LSDC_CRTC1_SCAN_POS_REG         0x14D0

/*
 * LS7A2000 has the hardware which count the number of vblank generated
 */
#define LSDC_CRTC0_VSYNC_COUNTER_REG    0x1A00
#define LSDC_CRTC1_VSYNC_COUNTER_REG    0x1A10

/*
 * In gross, LSDC_CRTC1_XXX_REG - LSDC_CRTC0_XXX_REG = 0x10,
 * but not all lsdc registers obay this rule, our hardware
 * engineer are lack experiance when they are young...
 */
#define CRTC_PIPE_OFFSET                0x10

/*
 * There is only one hardware cursor unit in ls7a1000 and ls2k1000.
 * While ls7a2000 has two hardware cursor unit.
 */
#define CURSOR_FORMAT_MASK              GENMASK(1, 0)
enum lsdc_cursor_format {
	CURSOR_FORMAT_DISABLE = 0,
	CURSOR_FORMAT_MONOCHROME = 1,
	CURSOR_FORMAT_ARGB8888 = 2,
};

#define CURSOR_SIZE_64X64               BIT(2)  /* 1: 64x64, 0: 32x32 */
#define CURSOR_LOCATION                 BIT(4)

#define LSDC_CURSOR0_CFG_REG            0x1520
#define LSDC_CURSOR0_ADDR_LO_REG        0x1530
#define LSDC_CURSOR0_ADDR_HI_REG        0x15e0
#define LSDC_CURSOR0_POSITION_REG       0x1540
#define LSDC_CURSOR0_BG_COLOR_REG       0x1550  /* background color */
#define LSDC_CURSOR0_FG_COLOR_REG       0x1560  /* foreground color */

#define LSDC_CURSOR1_CFG_REG            0x1670
#define LSDC_CURSOR1_ADDR_LO_REG        0x1680
#define LSDC_CURSOR1_ADDR_HI_REG        0x16e0
#define LSDC_CURSOR1_POSITION_REG       0x1690  /* [31:16] Y, [15:0] X */
#define LSDC_CURSOR1_BG_COLOR_REG       0x16A0  /* background color */
#define LSDC_CURSOR1_FG_COLOR_REG       0x16B0  /* foreground color */

/*
 * DC Interrupt Control Register, 32bit, Address Offset: 1570
 *
 * Bits 15:0 inidicate the interrupt status
 * Bits 31:16 control enable interrupts corresponding to bit 15:0 or not
 * Write 1 to enable, write 0 to disable
 *
 * RF: Read Finished
 * IDBU: Internal Data Buffer Underflow
 * IDBFU: Internal Data Buffer Fatal Underflow
 * CBRF: Cursor Buffer Read Finished Flag, no use.
 *
 * +-------+--------------------------+-------+--------+--------+-------+
 * | 31:27 |         26:16            | 15:11 |   10   |   9    |   8   |
 * +-------+--------------------------+-------+--------+--------+-------+
 * |  N/A  | Interrupt Enable Control |  N/A  | IDBFU0 | IDBFU1 | IDBU0 |
 * +-------+--------------------------+-------+--------+--------+-------+
 *
 * +-------+-----+-----+------+--------+--------+--------+--------+
 * |   7   |  6  |  5  |  4   |   3    |   2    |   1    |   0    |
 * +-------+-----+-----+------+--------+--------+--------+--------+
 * | IDBU1 | RF0 | RF1 | CRRF | HSYNC0 | VSYNC0 | HSYNC1 | VSYNC1 |
 * +-------+-----+-----+------+--------+--------+--------+--------+
 *
 * unfortunately, CRTC0's interrupt is mess with CRTC1's interrupt
 */

#define LSDC_INT_REG                    0x1570

#define INT_CRTC0_VSYNC                 BIT(2)
#define INT_CRTC0_HSYNC                 BIT(3)
#define INT_CRTC0_RF                    BIT(6)
#define INT_CRTC0_IDBU                  BIT(8)
#define INT_CRTC0_IDBFU                 BIT(10)

#define INT_CRTC1_VSYNC                 BIT(0)
#define INT_CRTC1_HSYNC                 BIT(1)
#define INT_CRTC1_RF                    BIT(5)
#define INT_CRTC1_IDBU                  BIT(7)
#define INT_CRTC1_IDBFU                 BIT(9)

#define INT_CRTC0_VSYNC_EN              BIT(18)
#define INT_CRTC0_HSYNC_EN              BIT(19)
#define INT_CRTC0_RF_EN                 BIT(22)
#define INT_CRTC0_IDBU_EN               BIT(24)
#define INT_CRTC0_IDBFU_EN              BIT(26)

#define INT_CRTC1_VSYNC_EN              BIT(16)
#define INT_CRTC1_HSYNC_EN              BIT(17)
#define INT_CRTC1_RF_EN                 BIT(21)
#define INT_CRTC1_IDBU_EN               BIT(23)
#define INT_CRTC1_IDBFU_EN              BIT(25)

#define INT_STATUS_MASK                 GENMASK(15, 0)

/*
 * LS7A1000/LS7A2000 have 4 gpios which are used to emulated I2C.
 * They are under control of the LS7A_DC_GPIO_DAT_REG and LS7A_DC_GPIO_DIR_REG
 * register, Those GPIOs has no relationship whth the GPIO hardware on the
 * bridge chip itself. Those offsets are relative to DC register base address
 *
 * LS2k1000 don't have those registers, they use hardware i2c or general GPIO
 * emulated i2c from linux i2c subsystem.
 *
 * GPIO data register, address offset: 0x1650
 *   +---------------+-----------+-----------+
 *   | 7 | 6 | 5 | 4 |  3  |  2  |  1  |  0  |
 *   +---------------+-----------+-----------+
 *   |               |    DVO1   |    DVO0   |
 *   +      N/A      +-----------+-----------+
 *   |               | SCL | SDA | SCL | SDA |
 *   +---------------+-----------+-----------+
 */
#define LS7A_DC_GPIO_DAT_REG            0x1650

/*
 *  GPIO Input/Output direction control register, address offset: 0x1660
 */
#define LS7A_DC_GPIO_DIR_REG            0x1660

/*
 *  LS7A2000 has two built-in HDMI Encoder and one VGA encoder
 */

/*
 * Number of continuous packets may be present
 * in HDMI hblank and vblank zone, should >= 48
 */
#define LSDC_HDMI0_ZONE_REG             0x1700
#define LSDC_HDMI1_ZONE_REG             0x1710

#define HDMI_INTERFACE_EN               BIT(0)
#define HDMI_PACKET_EN                  BIT(1)
#define HDMI_AUDIO_EN                   BIT(2)
#define HDMI_VIDEO_PREAMBLE_MASK        GENMASK(7, 4)
#define HDMI_VIDEO_PREAMBLE_SHIFT       4
#define HDMI_HW_I2C_EN                  BIT(8)
#define HDMI_CTL_PERIOD_MODE            BIT(9)
#define LSDC_HDMI0_CTRL_REG             0x1720
#define LSDC_HDMI1_CTRL_REG             0x1730

/*
 * Preamble:
 * Immediately preceding each video data period or data island period is the
 * preamble. This is a sequence of eight identical control characters that
 * indicate whether the upcoming data period is a video data period or is a
 * data island. The values of CTL0, CTL1, CTL2, and CTL3 indicate the type of
 * data period that follows.
 */

#define HDMI_PHY_EN                     BIT(0)
#define HDMI_PHY_RESET_N                BIT(1)
#define HDMI_PHY_TERM_L_EN              BIT(8)
#define HDMI_PHY_TERM_H_EN              BIT(9)
#define HDMI_PHY_TERM_DET_EN            BIT(10)
#define HDMI_PHY_TERM_STATUS            BIT(11)
#define LSDC_HDMI0_PHY_CTRL_REG         0x1800
#define LSDC_HDMI1_PHY_CTRL_REG         0x1810

/*
 *  IDF: Input Division Factor
 *  ODF: Output Division Factor
 *   LF: Loop Factor
 *    M: Required Mult
 *
 *  +--------------------------------------------------------+
 *  |     Fin (kHZ)     | M  | IDF | LF | ODF |   Fout(Mhz)  |
 *  |-------------------+----+-----+----+-----+--------------|
 *  |  170000 ~ 340000  | 10 | 16  | 40 |  1  | 1700 ~ 3400  |
 *  |   85000 ~ 170000  | 10 |  8  | 40 |  2  |  850 ~ 1700  |
 *  |   42500 ~  85000  | 10 |  4  | 40 |  4  |  425 ~ 850   |
 *  |   21250 ~  42500  | 10 |  2  | 40 |  8  | 212.5 ~ 425  |
 *  |   20000 ~  21250  | 10 |  1  | 40 | 16  |  200 ~ 212.5 |
 *  +--------------------------------------------------------+
 */
#define LSDC_HDMI0_PHY_PLL_REG          0x1820
#define LSDC_HDMI1_PHY_PLL_REG          0x1830

#define HDMI_PLL_ENABLE                 BIT(0)
#define HDMI_PLL_LOCKED                 BIT(16)
#define HDMI_PLL_BYPASS                 BIT(17)

#define HDMI_PLL_IDF_SHIFT              1
#define HDMI_PLL_IDF_MASK               GENMASK(5, 1)
#define HDMI_PLL_LF_SHIFT               6
#define HDMI_PLL_LF_MASK                GENMASK(12, 6)
#define HDMI_PLL_ODF_SHIFT              13
#define HDMI_PLL_ODF_MASK               GENMASK(15, 13)

/* LS7A2000/LS2K2000 have hpd support */
#define LSDC_HDMI_HPD_STATUS_REG        0x1BA0
#define HDMI0_HPD_FLAG                  BIT(0)
#define HDMI1_HPD_FLAG                  BIT(1)

/* AVI InfoFrame */
#define LSDC_HDMI0_AVI_CONTENT0         0x18e0
#define LSDC_HDMI1_AVI_CONTENT0         0x18f0
#define LSDC_HDMI0_AVI_CONTENT1         0x1900
#define LSDC_HDMI1_AVI_CONTENT1         0x1910
#define LSDC_HDMI0_AVI_CONTENT2         0x1920
#define LSDC_HDMI1_AVI_CONTENT2         0x1930
#define LSDC_HDMI0_AVI_CONTENT3         0x1940
#define LSDC_HDMI1_AVI_CONTENT3         0x1950

/* 1: enable avi infoframe packet, 0: disable avi infoframe packet */
#define AVI_PKT_ENABLE                  BIT(0)
/* 1: send one every two frame, 0: send one each frame */
#define AVI_PKT_SEND_FREQ               BIT(1)
/* 1: write 1 to flush avi reg content0 ~ content3 to the packet to be send */
#define AVI_PKT_UPDATE                  BIT(2)

#define LSDC_HDMI0_AVI_INFO_CRTL        0x1960
#define LSDC_HDMI1_AVI_INFO_CRTL        0x1970

#endif
