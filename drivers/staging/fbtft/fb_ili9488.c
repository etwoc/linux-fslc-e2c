/*
 * FB driver for the ILI9488 LCD display controller
 *
 * This display uses 9-bit SPI: Data/Command bit + 8 data bits
 * For platforms that doesn't support 9-bit, the driver is capable
 * of emulating this using 8-bit transfer.
 * This is done by transferring eight 9-bit words in 9 bytes.
 *
 * Copyright (C) 2013 Christian Vogelgsang
 * Based on adafruit22fb.c by Noralf Tronnes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define DEBUG 1
#define VERBOSE_DEBUG 1

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include "fbtft.h"

#define DRVNAME		"fb_ili9488"
#define WIDTH		320
#define HEIGHT		480


static int init_display(struct fbtft_par *par)
{
	int ret = 0;
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	par->fbtftops.reset(par);

	/* startup sequence for ER-TFT035-6 */
	write_reg(par, 0x01); /* software reset */

	gpio_set_value_cansleep(par->gpio.reset, 1);
	mdelay(5);
	gpio_set_value_cansleep(par->gpio.reset, 0);
	mdelay(20);
	gpio_set_value_cansleep(par->gpio.reset, 1);
	mdelay(150);


	pr_err("************* e2c: LCD reset is done! par->gpio.reset %d ************\n", par->gpio.reset);

	write_reg(par, 0x28); /* display off */
	write_reg(par, 0xE0, 0x00, 0x03, 0x09, 0x08, 0x16, \
			0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, \
			0x08, 0x16, 0x1A, 0x0F);
	write_reg(par, 0xE1, 0x00, 0x16, 0x19, 0x03, 0x0F, \
			0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, \
			0x0D, 0x35, 0x37, 0x0F);
	write_reg(par, 0xC0, 0x17, 0x15);
	write_reg(par, 0xC1, 0x41);	
	write_reg(par, 0xC5, 0x00, 0x12, 0x80);
	write_reg(par, 0x36, 0x48);
//	write_reg(par, 0x3A, 0x66);
//	write_reg(par, 0x3A, 0x55);
//	write_reg(par, 0x3A, 0x77);
	write_reg(par, 0x3A, 0x66);
//	write_reg(par, 0xB0, 0x80);
	write_reg(par, 0xB0, 0x00);
	write_reg(par, 0xB1, 0xA0);
	write_reg(par, 0xB4, 0x02);
//	write_reg(par, 0xB6, 0x02, 0x02, 0x3B);
	write_reg(par, 0xB6, 0x02, 0x02);
//	write_reg(par, 0xB6, 0x08, 0x82, 0x27);
	write_reg(par, 0xE9, 0x00);
	write_reg(par, 0x53, 0x28);//New
	write_reg(par, 0x51, 0x7F);//New
//	write_reg(par, 0xF7, 0xA9, 0x51, 0x2C, 0x82);
	write_reg(par, 0xF7, 0xA9, 0x51, 0x2C, 0x02);
	write_reg(par, 0x11); /* sleep out */
	mdelay(150);
	write_reg(par, 0x29); /* display on */
	mdelay(120);

	pr_err("************* e2c: LCD driver is initialized! ************\n");

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	fbtft_par_dbg(DEBUG_SET_ADDR_WIN, par,
		"%s(xs=%d, ys=%d, xe=%d, ye=%d)\n", __func__, xs, ys, xe, ye);

	/* column address */
	write_reg(par, 0x2a, xs >> 8, xs & 0xff, xe >> 8, xe & 0xff);

	/* Row address */
	write_reg(par, 0x2b, ys >> 8, ys & 0xff, ye >> 8, ye & 0xff);

	/* memory write */
	write_reg(par, 0x2c);
}

#define HFLIP 0x01
#define VFLIP 0x02
#define ROWxCOL 0x20
static int set_var(struct fbtft_par *par)
{
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	switch (par->info->var.rotate) {
	case 270:
		write_reg(par, 0x36, ROWxCOL | HFLIP | VFLIP | (par->bgr << 3));
		break;
	case 180:
		write_reg(par, 0x36, VFLIP | (par->bgr << 3));
		break;
	case 90:
		write_reg(par, 0x36, ROWxCOL | (par->bgr << 3));
		break;
	default:
		write_reg(par, 0x36, HFLIP | (par->bgr << 3));
		break;
	}

	return 0;
}

/* 16bpp converted to 18bpp stored in 24-bit over 8-bit databus */
int write_vmem16_bus8(struct fbtft_par *par, size_t offset, size_t len)
{
	u16 *vmem16;
	u8 *txbuf = par->txbuf.buf;
	size_t remain;
	size_t to_copy;
	size_t tx_array_size;
	int i;
	int ret = 0;

	/* fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		__func__, offset, len); */

	/* remaining number of pixels to send */
	remain = len / 2;
	vmem16 = (u16 *)(par->info->screen_buffer + offset);

	if (par->gpio.dc != -1)
		gpio_set_value(par->gpio.dc, 1);

	/* number of pixels that fits in the transmit buffer */
	tx_array_size = par->txbuf.len / 3;

	while (remain) {
		/* number of pixels to copy in one iteration of the loop */
		to_copy = min(tx_array_size, remain);
		dev_dbg(par->info->device, "    to_copy=%zu, remain=%zu\n",
						to_copy, remain - to_copy);

		for (i = 0; i < to_copy; i++) {
			u16 pixel = vmem16[i];
			u16 b = pixel & 0x1f;
			u16 g = (pixel & (0x3f << 5)) >> 5;
			u16 r = (pixel & (0x1f << 11)) >> 11;

			u8 r8 = (r & 0x1F) << 3;
			u8 g8 = (g & 0x3F) << 2;
			u8 b8 = (b & 0x1F) << 3;

			txbuf[i * 3 + 0] = r8;
			txbuf[i * 3 + 1] = g8;
			txbuf[i * 3 + 2] = b8;
		}

		vmem16 = vmem16 + to_copy;
		ret = par->fbtftops.write(par, par->txbuf.buf, to_copy * 3);
		if (ret < 0)
			return ret;
		remain -= to_copy;
	}

	return ret;
}


static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.fbtftops = {
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.init_display = init_display,
		.write_vmem = write_vmem16_bus8,
	},
};


FBTFT_REGISTER_DRIVER(DRVNAME, "ilitek,ili9488", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:ili9488");
MODULE_ALIAS("platform:ili9488");

MODULE_DESCRIPTION("FB driver for the ILI9488 LCD Controller");
MODULE_AUTHOR("Titus Rathinaraj Stalin");
MODULE_LICENSE("GPL");
