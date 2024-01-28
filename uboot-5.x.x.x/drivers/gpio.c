/******************************************************************************
* Filename : gpio.c
* This part is used to control LED and detect button-press
* 
******************************************************************************/

#include <common.h>
#include <rt_mmap.h>
#include <gpio.h>

#define ra_and(addr, value)	ra_outl(addr, (ra_inl(addr) & (value)))
#define ra_or(addr, value)	ra_outl(addr, (ra_inl(addr) | (value)))

#define GPIO_DIR_INPUT		0
#define GPIO_DIR_OUTPUT		1

#if defined (GPIO_LED_INVERTED)
#define GPIO_VAL_LED_SHOW	1
#define GPIO_VAL_LED_HIDE	0
#else
#define GPIO_VAL_LED_SHOW	0
#define GPIO_VAL_LED_HIDE	1
#endif

#define GPIO_VAL_USB_5V_ON	1
#define GPIO_VAL_BTN_PRESSED	0

#define RALINK_GPIOMODE_UART1		(1U << 1)	/* GPIO #1~#2 */
#define RALINK_GPIOMODE_I2C			(1U << 2)	/* GPIO #3~#4 */
#define RALINK_GPIOMODE_UART3		(1U << 3)	/* GPIO #5~#8 */
#define RALINK_GPIOMODE_UART2		(1U << 5)	/* GPIO #9~#12 */
#define RALINK_GPIOMODE_JTAG		(1U << 7)	/* GPIO #13~#17 */
#define RALINK_GPIOMODE_WDT			(1U << 8)	/* GPIO #18 */
#define RALINK_GPIOMODE_PERST		(1U << 10)	/* GPIO #19 */
#define RALINK_GPIOMODE_MDIO		(3U << 12)	/* GPIO #20~#21 */
#define RALINK_GPIOMODE_RGMII1		(1U << 14)	/* GPIO #49~#60 */
#define RALINK_GPIOMODE_RGMII2		(1U << 15)	/* GPIO #22~#33 */
#define RALINK_GPIOMODE_SPI			(1U << 16)	/* GPIO #34~#40 */
#define RALINK_GPIOMODE_SDXC		(1U << 18)	/* GPIO #41~#48 */
#define RALINK_GPIOMODE_ESWINT		(1U << 20)	/* GPIO #61 */

enum gpio_reg_id {
	GPIO_INT = 0,
	GPIO_EDGE,
	GPIO_RMASK,
	GPIO_MASK,
	GPIO_DATA,
	GPIO_DIR,
	GPIO_POL,
	GPIO_SET,
	GPIO_RESET,
	GPIO_MAX_REG
};

static const struct gpio_reg_offset_s {
	unsigned short min_nr, max_nr;
	unsigned short int_offset;
	unsigned short edge_offset;
	unsigned short rmask_offset;
	unsigned short fmask_offset;
	unsigned short data_offset;
	unsigned short dir_offset;
	unsigned short pol_offset;
	unsigned short set_offset;
	unsigned short reset_offset;
} s_gpio_reg_offset[] = {
	{  0, 31, 0x90, 0xA0, 0x50, 0x60, 0x20, 0x00, 0x10, 0x30, 0x40 },
	{ 32, 63, 0x94, 0xA4, 0x54, 0x64, 0x24, 0x04, 0x14, 0x34, 0x44 },
	{ 64, 95, 0x98, 0xA8, 0x58, 0x68, 0x28, 0x08, 0x18, 0x38, 0x48 },
};

static inline int is_valid_gpio_nr(unsigned short gpio_nr)
{
	return (gpio_nr > 61)? 0:1;
}

/* Query GPIO number belongs which item.
 * @gpio_nr:	GPIO number
 * @return:
 * 	NULL:	Invalid parameter.
 *  otherwise:	Pointer to a gpio_reg_offset_s instance.
 */
static const struct gpio_reg_offset_s *get_gpio_reg_item(unsigned short gpio_nr)
{
	unsigned int i;
	const struct gpio_reg_offset_s *p = &s_gpio_reg_offset[0], *ret = NULL;

	if (!is_valid_gpio_nr(gpio_nr))
		return ret;

	for (i = 0; !ret && i < ARRAY_SIZE(s_gpio_reg_offset); ++i, ++p) {
		if (gpio_nr < p->min_nr || gpio_nr > p->max_nr)
			continue;
		ret = p;
	}

	return ret;
}

/* Return bit-shift of a GPIO.
 * @gpio_nr:	GPIO number
 * @return:
 * 	0~31:	bit-shift of a GPIO pin in a register.
 *  	-1:	Invalid parameter.
 */
static int get_gpio_reg_bit_shift(unsigned short gpio_nr)
{
	const struct gpio_reg_offset_s *p;

	if (!(p = get_gpio_reg_item(gpio_nr)))
		return -1;

	return gpio_nr - p->min_nr;
}

/* Return specific GPIO register in accordance with GPIO number
 * @gpio_nr:	GPIO number
 * @return
 * 	0:	invalid parameter
 *  otherwise:	address of GPIO register
 */

static unsigned int get_gpio_reg_addr(unsigned short gpio_nr, enum gpio_reg_id id)
{
	unsigned int ret = 0;
	const struct gpio_reg_offset_s *p;

	if (!(p = get_gpio_reg_item(gpio_nr)) || id < 0 || id >= GPIO_MAX_REG)
		return ret;

	switch (id) {
	case GPIO_INT:
		ret = p->int_offset;
		break;
	case GPIO_EDGE:
		ret = p->edge_offset;
		break;
	case GPIO_RMASK:
		ret = p->rmask_offset;
		break;
	case GPIO_MASK:
		ret = p->fmask_offset;
		break;
	case GPIO_DATA:
		ret = p->data_offset;
		break;
	case GPIO_DIR:
		ret = p->dir_offset;
		break;
	case GPIO_POL:
		ret = p->pol_offset;
		break;
	case GPIO_SET:
		ret = p->set_offset;
		break;
	case GPIO_RESET:
		ret = p->reset_offset;
		break;
	default:
		return 0;
	}

	ret += RT2880_PRGIO_ADDR;

	return ret;
}

/* Set GPIO pin direction.
 * If a GPIO pin is multi-function pin, it would be configured as GPIO mode.
 * @gpio_nr:	GPIO number
 * @gpio_dir:	GPIO direction
 * 	1: 	output
 * 	0:	input
 *  otherwise:	input
 * @return:
 * 	0:	Success
 * 	-1:	Invalid parameter
 */
int mtk_set_gpio_dir(unsigned short gpio_nr, unsigned short gpio_dir_out)
{
	int shift;
	unsigned int msk, val;
	unsigned int reg;

	if (!is_valid_gpio_nr(gpio_nr))
		return -1;

	reg = get_gpio_reg_addr(gpio_nr, GPIO_DIR);
	shift = get_gpio_reg_bit_shift(gpio_nr);

	if (gpio_dir_out) {
		/* output */
		ra_or(reg, 1U << shift);
	} else {
		/* input */
		ra_and(reg, ~(1U << shift));
	}

	/* Handle special GPIO pin */
	shift = -1;
	msk = val = 1;

	if (gpio_nr >= 22 && gpio_nr <= 33) {
		/* RGMII2 */
		shift = 15;
	} else if (gpio_nr >= 41 && gpio_nr <= 48) {
		/* SDXC/NAND */
		shift = 18;
		msk = 3;
		val = 1;
	} else if (gpio_nr == 61) {
		/* ESW INT */
		shift = 20;
	}

	if (shift >= 0) {
		reg = ra_inl(RT2880_GPIOMODE_REG);
		reg &= ~(msk << shift);
		reg |=  (val << shift);
		ra_outl(RT2880_GPIOMODE_REG, reg);
	}

	return 0;
}

/* Read GPIO pin value.
 * @gpio_nr:	GPIO number
 * @return:	GPIO value
 * 	0/1:	Success
 * 	-1:	Invalid parameter
 */

int mtk_get_gpio_pin(unsigned short gpio_nr)
{
	int shift;
	unsigned int reg;
	unsigned int val = 0;

	if (!is_valid_gpio_nr(gpio_nr))
		return -1;

	reg = get_gpio_reg_addr(gpio_nr, GPIO_DATA);
	shift = get_gpio_reg_bit_shift(gpio_nr);

	val = ra_inl(reg);
	val = (val >> shift) & 1U;

	return val;
}

/* Set GPIO pin value
 * @gpio_nr:	GPIO number
 * @val:
 * 	0:	Write 0 to GPIO pin
 *  otherwise:	Write 1 to GPIO pin
 * @return:
 * 	0:	Success
 * 	-1:	Invalid parameter
 */

int mtk_set_gpio_pin(unsigned short gpio_nr, unsigned int val)
{
	int shift;
	unsigned int reg;

	if (!is_valid_gpio_nr(gpio_nr))
		return -1;

	reg = get_gpio_reg_addr(gpio_nr, (val) ? GPIO_SET : GPIO_RESET);
	shift = get_gpio_reg_bit_shift(gpio_nr);

	ra_outl(reg, (1U << shift));

	return 0;
}

void gpio_init(void)
{
	unsigned int gm = RALINK_GPIOMODE_I2C;
	gm |= RALINK_GPIOMODE_UART2|RALINK_GPIOMODE_UART3|RALINK_GPIOMODE_WDT|RALINK_GPIOMODE_JTAG;
	ra_or(RT2880_GPIOMODE_REG, gm);

	/* show all LED (flash after power on) */
#if (GPIO_LED_ALL >= 0)
	mtk_set_gpio_dir(GPIO_LED_ALL, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_ALL, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_POWER >= 0)
	mtk_set_gpio_dir(GPIO_LED_POWER, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_POWER, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_WPS >= 0)
	mtk_set_gpio_dir(GPIO_LED_WPS, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_WPS, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_WAN >= 0)
	mtk_set_gpio_dir(GPIO_LED_WAN, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_WAN, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_INIT1 >= 0)
	mtk_set_gpio_dir(GPIO_LED_INIT1, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_INIT1, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_INIT2 >= 0)
	mtk_set_gpio_dir(GPIO_LED_INIT2, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_INIT2, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_INIT3 >= 0)
	mtk_set_gpio_dir(GPIO_LED_INIT3, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_INIT3, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_INIT4 >= 0)
	mtk_set_gpio_dir(GPIO_LED_INIT4, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_INIT4, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_INIT5 >= 0)
	mtk_set_gpio_dir(GPIO_LED_INIT5, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_INIT5, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_INIT6 >= 0)
	mtk_set_gpio_dir(GPIO_LED_INIT6, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_INIT6, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_INIT7 >= 0)
	mtk_set_gpio_dir(GPIO_LED_INIT7, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_INIT7, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_INIT8 >= 0)
	mtk_set_gpio_dir(GPIO_LED_INIT8, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_LED_INIT8, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_ALL >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALL, GPIO_VAL_LED_SHOW);
#endif

	/* prepare alert LED(s) */
#if ((GPIO_LED_ALERT1 >= 0) && (GPIO_LED_ALERT1 != GPIO_LED_INIT1) && (GPIO_LED_ALERT1 != GPIO_LED_POWER))
	mtk_set_gpio_dir(GPIO_LED_ALERT1, GPIO_DIR_OUTPUT);
#endif
#if ((GPIO_LED_ALERT2 >= 0) && (GPIO_LED_ALERT2 != GPIO_LED_INIT2))
	mtk_set_gpio_dir(GPIO_LED_ALERT2, GPIO_DIR_OUTPUT);
#endif
#if ((GPIO_LED_ALERT3 >= 0) && (GPIO_LED_ALERT3 != GPIO_LED_INIT3))
	mtk_set_gpio_dir(GPIO_LED_ALERT3, GPIO_DIR_OUTPUT);
#endif
#if ((GPIO_LED_ALERT4 >= 0) && (GPIO_LED_ALERT4 != GPIO_LED_INIT4))
	mtk_set_gpio_dir(GPIO_LED_ALERT4, GPIO_DIR_OUTPUT);
#endif

	/* prepare all buttons */
#if (GPIO_BTN_RESET >= 0)
	mtk_set_gpio_dir(GPIO_BTN_RESET, GPIO_DIR_INPUT);
#endif
#if (GPIO_BTN_WPS >= 0)
	mtk_set_gpio_dir(GPIO_BTN_WPS, GPIO_DIR_INPUT);
#endif
#if (GPIO_BTN_WLTOG >= 0)
	mtk_set_gpio_dir(GPIO_BTN_WLTOG, GPIO_DIR_INPUT);
#endif
#if (GPIO_BTN_ROUTER >= 0)
	mtk_set_gpio_dir(GPIO_BTN_ROUTER, GPIO_DIR_INPUT);
#endif

	/* raise reset iNIC (or other peripheral) */
#if (GPIO_RST_INIC >= 0)
	mtk_set_gpio_dir(GPIO_RST_INIC, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_RST_INIC, 0);
	udelay(1000);
#endif
}

void gpio_init_mdio(void)
{
#if defined(RALINK_GPIOMODE_MDIO)
	ra_and(RT2880_GPIOMODE_REG, (~RALINK_GPIOMODE_MDIO));
#endif
}

void gpio_init_usb(int do_wait)
{
#if (GPIO_USB_POWER >= 0)
	mtk_set_gpio_dir(GPIO_USB_POWER, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_USB_POWER, GPIO_VAL_USB_5V_ON);
#endif
#if (GPIO_USB_POWER2 >= 0)
	mtk_set_gpio_dir(GPIO_USB_POWER2, GPIO_DIR_OUTPUT);
	mtk_set_gpio_pin(GPIO_USB_POWER2, GPIO_VAL_USB_5V_ON);
#endif
#if (GPIO_USB_POWER >= 0 || GPIO_USB_POWER2 >= 0)
	if (do_wait)
		udelay(150000);
#endif
}

int DETECT_BTN_RESET(void)
{
	int key = 0;
#if (GPIO_BTN_RESET >= 0)
	if (mtk_get_gpio_pin(GPIO_BTN_RESET) == GPIO_VAL_BTN_PRESSED) {
		key = 1;
		debug("RESET button pressed!\n");
	}
#endif
	return key;
}

int DETECT_BTN_WPS(void)
{
	int key = 0;
#if (GPIO_BTN_WPS >= 0)
	if (mtk_get_gpio_pin(GPIO_BTN_WPS) == GPIO_VAL_BTN_PRESSED) {
		key = 1;
		debug("WPS button pressed!\n");
	}
#endif
	return key;
}

void LED_HIDE_ALL(void)
{
	/* hide all LED (except Power) */
#if (GPIO_LED_INIT1 >= 0)
	mtk_set_gpio_pin(GPIO_LED_INIT1, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_INIT2 >= 0)
	mtk_set_gpio_pin(GPIO_LED_INIT2, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_INIT3 >= 0)
	mtk_set_gpio_pin(GPIO_LED_INIT3, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_INIT4 >= 0)
	mtk_set_gpio_pin(GPIO_LED_INIT4, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_INIT5 >= 0)
	mtk_set_gpio_pin(GPIO_LED_INIT5, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_INIT6 >= 0)
	mtk_set_gpio_pin(GPIO_LED_INIT6, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_INIT7 >= 0)
	mtk_set_gpio_pin(GPIO_LED_INIT7, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_INIT8 >= 0)
	mtk_set_gpio_pin(GPIO_LED_INIT8, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_WAN >= 0)
	mtk_set_gpio_pin(GPIO_LED_WAN, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_WPS >= 0)
	mtk_set_gpio_pin(GPIO_LED_WPS, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_ALL >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALL, GPIO_VAL_LED_HIDE);
#endif
#if ((GPIO_LED_ALERT1 >= 0) && (GPIO_LED_ALERT1 != GPIO_LED_INIT1) && (GPIO_LED_ALERT1 != GPIO_LED_POWER))
	mtk_set_gpio_pin(GPIO_LED_ALERT1, GPIO_VAL_LED_HIDE);
#endif
#if ((GPIO_LED_ALERT2 >= 0) && (GPIO_LED_ALERT2 != GPIO_LED_INIT2))
	mtk_set_gpio_pin(GPIO_LED_ALERT2, GPIO_VAL_LED_HIDE);
#endif
#if ((GPIO_LED_ALERT3 >= 0) && (GPIO_LED_ALERT3 != GPIO_LED_INIT3))
	mtk_set_gpio_pin(GPIO_LED_ALERT3, GPIO_VAL_LED_HIDE);
#endif
#if ((GPIO_LED_ALERT4 >= 0) && (GPIO_LED_ALERT4 != GPIO_LED_INIT4))
	mtk_set_gpio_pin(GPIO_LED_ALERT4, GPIO_VAL_LED_HIDE);
#endif

	/* complete reset iNIC (or other peripheral) */
#if (GPIO_RST_INIC >= 0)
	mtk_set_gpio_pin(GPIO_RST_INIC, 1);
#endif
}

void LED_POWER_ON(void)
{
#if ((GPIO_LED_ALERT1 >= 0) && (GPIO_LED_ALERT1 != GPIO_LED_POWER))
	mtk_set_gpio_pin(GPIO_LED_ALERT1, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_ALERT2 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT2, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_ALERT3 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT3, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_ALERT4 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT4, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_POWER >= 0)
	mtk_set_gpio_pin(GPIO_LED_POWER, GPIO_VAL_LED_SHOW);
#endif
}

void LED_ALERT_ON(void)
{
#if (GPIO_LED_ALERT1 >= 0 || GPIO_LED_ALERT2 >= 0 || GPIO_LED_ALERT3 >= 0 || GPIO_LED_ALERT4 >= 0)
#if ((GPIO_LED_POWER >= 0) && (GPIO_LED_POWER != GPIO_LED_ALERT1))
	mtk_set_gpio_pin(GPIO_LED_POWER, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_ALERT1 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT1, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_ALERT2 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT2, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_ALERT3 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT3, GPIO_VAL_LED_SHOW);
#endif
#if (GPIO_LED_ALERT4 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT4, GPIO_VAL_LED_SHOW);
#endif
#elif (GPIO_LED_POWER >= 0)
	mtk_set_gpio_pin(GPIO_LED_POWER, GPIO_VAL_LED_SHOW);
#endif
}

void LED_ALERT_OFF(void)
{
#if (GPIO_LED_ALERT1 >= 0 || GPIO_LED_ALERT2 >= 0 || GPIO_LED_ALERT3 >= 0 || GPIO_LED_ALERT4 >= 0)
#if (GPIO_LED_ALERT1 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT1, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_ALERT2 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT2, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_ALERT3 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT3, GPIO_VAL_LED_HIDE);
#endif
#if (GPIO_LED_ALERT4 >= 0)
	mtk_set_gpio_pin(GPIO_LED_ALERT4, GPIO_VAL_LED_HIDE);
#endif
#elif (GPIO_LED_POWER >= 0)
	mtk_set_gpio_pin(GPIO_LED_POWER, GPIO_VAL_LED_HIDE);
#endif
}

void LED_ALERT_BLINK(void)
{
	static u32 alert_cnt = 0;

	if (alert_cnt % 2)
		LED_ALERT_ON();
	else
		LED_ALERT_OFF();

	alert_cnt++;
}

void LED_WAN_ON(void)
{
#if (GPIO_LED_WAN >= 0)
	mtk_set_gpio_pin(GPIO_LED_WAN, GPIO_VAL_LED_SHOW);
#endif
}

void LED_WAN_OFF(void)
{
#if (GPIO_LED_WAN >= 0)
	mtk_set_gpio_pin(GPIO_LED_WAN, GPIO_VAL_LED_HIDE);
#endif
}

void LED_WPS_ON(void)
{
#if (GPIO_LED_WPS >= 0)
	mtk_set_gpio_pin(GPIO_LED_WPS, GPIO_VAL_LED_SHOW);
#endif
}

void LED_WPS_OFF(void)
{
#if (GPIO_LED_WPS >= 0)
	mtk_set_gpio_pin(GPIO_LED_WPS, GPIO_VAL_LED_HIDE);
#endif
}

void LED_WPS_BLINK( void )
{
	static u32 cnt = 0;

	if ( cnt % 2 )
		LED_WPS_ON();
	else
		LED_WPS_OFF();

	cnt++;
}
