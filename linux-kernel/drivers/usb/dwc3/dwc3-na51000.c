/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <mach/efuse_protected.h>

#include "core.h"

#define PLL_SETREG(ofs,value)	writel(value, (NVT_CG_BASE_VIRT+(ofs)))
#define PLL_GETREG(ofs)		readl(NVT_CG_BASE_VIRT+(ofs))

#define USB3_SETREG(ofs,value)	writel((value), (NVT_USB3_BASE_VIRT+(ofs)))
#define USB3_GETREG(ofs)	readl(NVT_USB3_BASE_VIRT+(ofs))

#define USB2APB_SETREG(ofs,value)	writel((value), (volatile void __iomem *)(0xFD600000+(ofs)))
#define USB3APB_SETREG(ofs,value)	writel((value), (volatile void __iomem *)(0xFD670000+(ofs)))
#define USB3APB_GETREG(ofs)	readl((volatile void __iomem *)(0xFD670000+(ofs)))

#define USB3PHY_SETREG(ofs,value)	writel((value), (volatile void __iomem *)(0xFD672000+((ofs)<<2)))
#define USB3PHY_GETREG(ofs)	readl((volatile void __iomem *)(0xFD672000+((ofs)<<2)))
#define USB3U2PHY_SETREG(ofs,value)	writel((value), (volatile void __iomem *)(0xFD671000+((ofs)<<2)))
#define USB3U2PHY_GETREG(ofs)	readl((volatile void __iomem *)(0xFD671000+((ofs)<<2)))

#define USB3_SIMULATE_POWEROFF		0

#define PLL_OFS 					0x0
#define PLL12_MASK 					0x1000
#define AXI_BRDE_OFS 				0xB4
#define AXI_BRDE_MASK 				0x100000
#define USB3_REF_SUSP_CLK_OFS 		0x90
#define USB3_REF_SUSP_MASK 			0x300
#define USB3_RST_OFS 				0xA8
#define USB3_RST_BIT 				(0x01<<17)
#define USB3_GCTL_OFS 				0xC110
#define USB3_GCTL_CONFIG 			0x17712000
#define USB3_GCTL_MASK 				0xFFF93030
#define USB3_GSBUSCFG0_OFS 			0xC100
#define USB3_GSBUSCFG1_OFS 			0xC104
#define USB3_GSBUSCFG0_DEFAULT 		0x00000001
#define USB3_GSBUSCFG1_DEFAULT 		0x00000300
#define USB3_GUSB2PHYCFG_OFS 		0xC200
#define USB3_GUSB3PIPECTL_OFS 		0xC2C0
#define USB3_GUSB2PHYCFG_DEFAULT	0x40102408
#define USB3_GUSB3PIPECTL_DEFAULT	0x010C0003

static struct clk *clk_susp,*clk_itp;

void dwc3_nvtivot_device_init(struct platform_device *pdev)
{
	u32 reg;
	UINT8 u3_trim_rint_sel=8,u3_trim_swctrl=4, u3_trim_sqsel=4,u3_trim_icdr=0xB;

	#if USB3_SIMULATE_POWEROFF
	printk("dwc3_nvtivot_device_init\n");
	{
		u32 temp;
		printk("dwc3_nvtivot_device_init assert reset\n");
		temp = readl(NVT_CG_BASE_VIRT+(0xA8));
		temp &= ~(0x1<<17);
		writel(temp, (NVT_CG_BASE_VIRT+(0xA8)));
		udelay(1000);
		temp |= (0x1<<17);
		writel(temp, (NVT_CG_BASE_VIRT+(0xA8)));
		udelay(1000);
		printk("R114=0x%02X\n",USB3PHY_GETREG(0x114));
	}
	#endif

	if (clk_susp == NULL) {
		clk_susp = clk_get(&pdev->dev, "f1700000.u3susp");
	}

	if (IS_ERR(clk_susp)) {
		printk("faile to get clock f1700000.u3susp\n");
	} else {
		/* USB3 reset is toggled here */
		clk_prepare(clk_susp);
		/* PLL12 & u3susp clocks would be enabled here */
		clk_enable(clk_susp);
	}

	if (clk_itp == NULL) {
		clk_itp = clk_get(&pdev->dev, "f1700000.u3itp");
	}

	if (IS_ERR(clk_itp)) {
		printk("faile to get clock f1700000.u3itp\n");
	} else {
		/* AXI Bridge reset is released here */
		clk_prepare(clk_itp);
		/* u3itp clock would be enabled here */
		clk_enable(clk_itp);
	}

	udelay(2);

	if (!(PLL_GETREG(0x4) & PLL12_MASK)) {
		printk("Error! PLL12 not enabled!\r\n\n");
	}

	/* Config Core Control Register GCTL*/
	reg = USB3_GETREG(USB3_GCTL_OFS);
	reg &= ~USB3_GCTL_MASK;
	USB3_SETREG(USB3_GCTL_OFS, reg | USB3_GCTL_CONFIG);

	/* Config GSBUSCFG0/GSBUSCFG1*/
	USB3_SETREG(USB3_GSBUSCFG0_OFS, USB3_GSBUSCFG0_DEFAULT);
	USB3_SETREG(USB3_GSBUSCFG1_OFS, USB3_GSBUSCFG1_DEFAULT);

	/* Setting PHY Control Default value*/
	USB3_SETREG(USB3_GUSB2PHYCFG_OFS,  USB3_GUSB2PHYCFG_DEFAULT);
	USB3_SETREG(USB3_GUSB3PIPECTL_OFS, USB3_GUSB3PIPECTL_DEFAULT);

	/* APB Read delay. This makes APB PHY REG read functioning */
	USB3APB_SETREG(0x34, 0x33);

	{
		UINT16 data=0;
		INT32 result;
		UINT32 temp;

		result= efuse_read_param_ops(EFUSE_USBC_TRIM_DATA, &data);
		if(result >= 0) {
			u3_trim_swctrl = data&0x7;
			u3_trim_sqsel  = (data>>3)&0x7;
			u3_trim_rint_sel = (data>>6)&0x1F;
		}

		result= efuse_read_param_ops(EFUSE_USBC2_TRIM_DATA, &data);
		if(result >= 0) {
			u3_trim_icdr= data&0xF;
		}

		//usb3_validateTrim();
		USB3PHY_SETREG(0x1A0, 0x40+u3_trim_rint_sel);
		USB3PHY_SETREG(0x1A3, 0x60);

		temp = USB3U2PHY_GETREG(0x06);
		temp &= ~(0x7<<1);
		temp |= (u3_trim_swctrl<<1);
		USB3U2PHY_SETREG(0x06, temp);

		temp = USB3U2PHY_GETREG(0x05);
		temp &= ~(0x7<<2);
		temp |= (u3_trim_sqsel<<2);
		USB3U2PHY_SETREG(0x05, temp);
	}

	/* Set VBUS */
	USB3APB_SETREG(0x10, 0x3);

	/* asd_mode=1 (toggle rate) */
	USB3PHY_SETREG(0x198, 0x04);
	/* RX_ICTRL's offset = 0 */
    USB3PHY_SETREG(0x1BF, 0x40);
	/* TX_AMP_CTL=3, TX_DEC_EM_CTL=f */
    USB3PHY_SETREG(0x014, 0xfb);
	/* TX_LFPS_AMP_CTL = 1 */
    USB3PHY_SETREG(0x034, 0xfc);
	/* PHY Power Mode Change ready reponse time. (3 is 1ms.)(4 is 1.3ms.) */
    USB3PHY_SETREG(0x114, 0x0B);
    USB3PHY_SETREG(0x152, 0x2E);
    USB3PHY_SETREG(0x153, 0x01);
    USB3PHY_SETREG(0x1B0, 0xC0);
    USB3PHY_SETREG(0x1B1, 0x91);
    USB3PHY_SETREG(0x1B2, 0x00);
    USB3PHY_SETREG(0x135, 0x88);
    USB3PHY_SETREG(0x12A, 0x50);
    USB3PHY_SETREG(0x1F0, 0x80);
    USB3PHY_SETREG(0x1F5, 0x01|(u3_trim_icdr<<4));//0xB1
    USB3PHY_SETREG(0x105, 0x01);
    udelay(2);
    USB3PHY_SETREG(0x102, 0x20);
    udelay(10);
    USB3PHY_SETREG(0x102, 0x00);
    udelay(300);

	/* Force termination turn on */
	//USB3PHY_SETREG(0x029, 0x1);

	/* disable_power_saving */
	USB3APB_SETREG((0x1000+(0xE<<2)), 	0x04);
	USB2APB_SETREG((0x1000+(0xE<<2)), 	0x04);

	#if USB3_SIMULATE_POWEROFF
	printk("R114=0x%02X\n",USB3PHY_GETREG(0x114));
	#endif

}


void dwc3_nvtivot_device_uninit(struct platform_device *pdev)
{
	/* enable_power_saving */
	USB3APB_SETREG((0x1000+(0xE<<2)), 	0x00);
	USB2APB_SETREG((0x1000+(0xE<<2)), 	0x00);

	/* Set PHY Suspend */
	USB3_SETREG(USB3_GUSB2PHYCFG_OFS,  USB3_GETREG(USB3_GUSB2PHYCFG_OFS) |(0x1<<6));
	USB3_SETREG(USB3_GUSB3PIPECTL_OFS, USB3_GETREG(USB3_GUSB3PIPECTL_OFS)|(0x1<<17));

	if (clk_itp != NULL) {
		if (IS_ERR(clk_itp)) {
			printk("faile to get clock f1700000.u3itp\n");
		} else {
			clk_disable(clk_itp);
			clk_unprepare(clk_itp);
			clk_put(clk_itp);
			clk_itp = NULL;
		}
	} else {
		printk("faile to get clock f1700000.u3itp\n");
	}

	if (clk_susp != NULL) {
		if (IS_ERR(clk_susp)) {
			printk("faile to get clock f1700000.u3susp\n");
		} else {
			clk_disable(clk_susp);
			clk_unprepare(clk_susp);
			clk_put(clk_susp);
			clk_susp = NULL;
		}
	} else {
		printk("faile to get clock f1700000.u3susp\n");
	}
}

void dwc3_nvtivot_fuse_log(struct dwc3 *dwc)
{
	u32 fusedata=0;

	fusedata += ((USB3PHY_GETREG(0x1A0) & 0xFF) << 0);
	fusedata += ((USB3APB_GETREG(0x1000+(0x6<<2)) & 0xFF) << 8);
	fusedata += ((USB3APB_GETREG(0x1000+(0x5<<2)) & 0xFF) << 16);
	dwc->fuseData = fusedata;
}

void dwc3_nvtivot_fuse_restore(struct dwc3 *dwc)
{
	USB3PHY_SETREG(0x1A0, 				(dwc->fuseData >> 0)&0xFF);
	USB3APB_SETREG((0x1000+(0x6<<2)), 	(dwc->fuseData >> 8)&0xFF);
	USB3APB_SETREG((0x1000+(0x5<<2)), 	(dwc->fuseData >> 16)&0xFF);
}

void dwc3_nvtivot_enable_power_saving(void)
{
	USB3APB_SETREG((0x1000+(0xE<<2)), 	0x00);
	USB2APB_SETREG((0x1000+(0xE<<2)), 	0x00);
}

void dwc3_nvtivot_disable_power_saving(void)
{
	USB3APB_SETREG((0x1000+(0xE<<2)), 	0x04);
	USB2APB_SETREG((0x1000+(0xE<<2)), 	0x04);
}

