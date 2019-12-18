/*
 *  driver/mmc/nvt_mmc.c
 *
 *  Author:	SP-KSW
 *  Created:	April 24, 2013
 *  Copyright:	Novatek Inc.
 *
 */

#include <common.h>
#include <asm/io.h>
#include <sdhci.h>
#include "common.h"
#include <errno.h>
#include "nvt_mmc.h"
#include <malloc.h>
#include <dm/device.h>

//#define P //printf("%s %d\n",__func__, __LINE__);
DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_DM_MMC
#error "Sorry, Novatek eMMC driver support DM_MMC only !"
#endif

#define STBC_UNLOCK()	nvt_stbc_unlock_reg();

struct nvt_emmc_data {
	unsigned int sdc_base_addr;
	struct mmc_config cfg;
};

//#define __MMC_DEBUG
//=============================================================================================================
#define WAIT_SECOND_FOR_WRITE (10)

static int _emmc_host_select_clk_src(int src)
{
	if (src == 0) {
		STBC_UNLOCK();
		EMMC_CLK_SRC_CTRL |= EMMC_FAST_CLK_SRC_ENABLE;
	} else if (src == 1) {
		STBC_UNLOCK();
		EMMC_CLK_SRC_CTRL &= ~EMMC_FAST_CLK_SRC_ENABLE;
	} else
		return 0;

	/* 1 micro second at least */
	udelay(1);

	return 1;
}

#ifdef CONFIG_NVT_EMMC_IO_DRV_INIT
static int _emmc_driving_init(void)
{
	
	nvt_stbc_set_keypass(1);
	GPIO_SWITCH_CTRL = 0xFFEFF;
	SET_EMMC_IO_DRV(EMMC_IO_DRV_D0, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_D1, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_D2, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_D3, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_D4, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_D5, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_D6, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_D7, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_CLK, 0xC6);
	udelay(1);

	SET_EMMC_IO_DRV(EMMC_IO_DRV_CMD, 0xC6);
	udelay(1);
}
#endif

static int _emmc_arch_host_preinit(void)
{
#ifdef CONFIG_NVT_EMMC_IO_DRV_INIT
	_emmc_driving_init();
#endif
	STBC_UNLOCK();
	*(volatile unsigned int *)(0xfc040200) &= ~(1<<4);
	udelay(1);

	STBC_UNLOCK();
	udelay(1);
	*(volatile unsigned int *)(0xfc040200) |= (1<<5);
	udelay(1);

	STBC_UNLOCK();
	*(volatile unsigned int *)(0xfc04021c) |= (1<<25);
	udelay(1);

	STBC_UNLOCK();
	*(volatile unsigned int *)(0xfc04021c) &= ~(1<<25);
	udelay(1);

	_emmc_host_select_clk_src(0);

	EMMC_SECURE_LOCK_CTRL &= ~EMMC_SECURE_LOCK_ENABLE;

	return 0;
}

static unsigned long sdc_clk2div(unsigned long clk)
{
	unsigned long q = 0;
	unsigned long r = 0;
	unsigned long clk_src = 0;

extern unsigned long get_emmc_clk(void);
	/* if selected high clk src */ 
	if (EMMC_CLK_SRC_CTRL & EMMC_FAST_CLK_SRC_ENABLE)
		clk_src = get_emmc_clk();
	else
		clk_src = SDCLK_SOURCE_LOW;

	/* the max clk using this approach is clk_src divied by 8 */
	if (clk_src/8 < clk)
		return 0;

	q = (clk_src/(clk*4));
	r = (clk_src%(clk*4));
	q -= 2;

	if (r)
		q+=1;

	return q;
}

static unsigned long _emmc_init_clk(unsigned long rate)
{
	unsigned long divisor = 0;

#ifdef __MMC_DEBUG
	printf("emmc clk rate : %ld\n", rate);
#endif

	if ((rate == 0) || (rate > SDCLK_MAX)) {
		printf("rate(%ld) is out of range", rate);
		return 0;
	}

	/* turn off clk */
	REG_SDC_CLK_CTRL = 0;

	if (divisor > SDC_MAX_CLK_DIV) {
		printf("can not get proper divisor");
		return 0;
	}
	/* if clk is higher than 1/4 of eMMC clk source, use 1/4 clk source, which is the max rate */
	if (rate >= CLK_100M) {
#ifdef __MMC_DEBUG
		printf("hs200 clk new @rate %ld\n", rate);
#endif
		REG_FCR_FUNC_CTRL &= ~FCR_FUNC_CTRL_SD_FLEXIBLE_CLK;
		REG_FCR_HS200_CTRL |= FCR_HS200_CTRL_FASTEST_CLK;
		REG_FCR_HS200_CTRL |= FCR_HS200_CTRL_HW_TRACK_EACH_BLK;
	} else {
		REG_FCR_HS200_CTRL &= ~FCR_HS200_CTRL_FASTEST_CLK;

		divisor = sdc_clk2div(rate);

		/* use NVT's clk scheme */
		REG_FCR_FUNC_CTRL |= FCR_FUNC_CTRL_SD_FLEXIBLE_CLK;
	} 
	/* turn on bus internal clk */
	REG_SDC_CLK_CTRL = SDC_CLK_CTRL_INCLK_ENABLE | SDC_CLK_CTRL_SDCLK_FREQ_SEL_EX(divisor);

	while (!(REG_SDC_CLK_CTRL & SDC_CLK_CTRL_INCLK_STABLE))
		printf("wait for emmc internal clk stable...\n");

	/* turn on bus clk */
	REG_SDC_CLK_CTRL |= SDC_CLK_CTRL_SDCLK_ENABLE;

	/* 1ms at least to wait external bus clk stable */
	mdelay(1);

	return divisor;
}

static int _emmc_bus_init(unsigned long voltage)
{
	if ((voltage != SDC_PW_CTRL_BUS_VOL_33V) &&
		(voltage != SDC_PW_CTRL_BUS_VOL_30V) &&
		(voltage != SDC_PW_CTRL_BUS_VOL_18V)) {
		printf("wrong bus voltage(0x%lx)", voltage);
		return 0;
	}

	/* we support bus power 1.8V and 3.3V */
	REG_SDC_PW_CTRL = SDC_PW_CTRL_BUS_PW_ON | voltage;

	/* 10ms for safety */
	mdelay(10);

	_emmc_init_clk(EMMC_INIT_BUS_CLK);

	return 1;
}

static int _emmc_set_bus_timing_mode(int clk,
                                   enum EMMC_DATA_LATCH data_latch)
{
	enum EMMC_SPEED_MODE speed_mode  = EMMC_NULL_SPEED_MODE;

	if(clk <= CLK_26M) 
		speed_mode = EMMC_LEGACY_SPEED;
	else if( clk <= CLK_52M)
		speed_mode = EMMC_HIGH_SPEED;
	else if( clk <= CLK_200M)
		speed_mode = EMMC_HS200_SPEED;
	else 
		printf("[MMC]error speed mode!!\n");

	if(speed_mode == EMMC_NULL_SPEED_MODE)
		return -1;
#ifdef __MMC_DEBUG	
	printf("[MMC] speed_mode = %d\n", speed_mode);
	printf("[MMC] latch = %d\n", data_latch);
#endif
        if ((speed_mode == EMMC_HIGH_SPEED) || (speed_mode == EMMC_HS200_SPEED)) {
			REG_SDC_HOST_CTRL |= SDC_HOST_CTRL_HIGH_SPEED;
		//mdelay(1);
            if (speed_mode == EMMC_HS200_SPEED) {
				REG_FCR_HS200_CTRL |= FCR_HS200_CTRL_DISABLE_CMD_CONFLICT;
				REG_FCR_HS200_CTRL &= ~FCR_HS200_OUTPUT_SELECT_MASK;
				REG_FCR_HS200_CTRL |= FCR_HS200_CTRL_ENABLE;
				REG_FCR_HS200_CTRL |= FCR_HS200_OUTPUT_SELECT_PHASE(0x1);
            } else {
				REG_FCR_HS200_CTRL &= ~FCR_HS200_CTRL_ENABLE;
				REG_FCR_HS200_CTRL &= ~FCR_HS200_CTRL_DISABLE_CMD_CONFLICT;
            }

        } else if (speed_mode == EMMC_LEGACY_SPEED) {
			REG_FCR_HS200_CTRL &= ~FCR_HS200_CTRL_DISABLE_CMD_CONFLICT;
			REG_FCR_HS200_CTRL &= ~ FCR_HS200_CTRL_ENABLE;
			REG_SDC_HOST_CTRL &=  ~SDC_HOST_CTRL_HIGH_SPEED;
		} else {
			printf("[MMC] not support host speed mode(%d)", speed_mode);
            return -1;
        }

		mdelay(1);

        if (data_latch == EMMC_SINGLE_LATCH) {
                REG_FCR_CPBLT &= ~FCR_CPBLT_DUAL_DATA_RATE_ENABLE;
        } else if (data_latch == EMMC_DUAL_LATCH) {
                REG_FCR_CPBLT |= FCR_CPBLT_DUAL_DATA_RATE_ENABLE;
        } else {
                printf("cfg->data_latch(%d) is invalid.", data_latch); 
                return 0;
        }

	//udelay(1);

        return 0;
}
          
static int _emmc_host_switch_bus_width(enum EMMC_BUS_WIDTH bus_width)
{
	switch (bus_width) {
		case EMMC_BW_1BIT:
			REG_FCR_FUNC_CTRL &= ~FCR_FUNC_CTRL_MMC_8BIT;
			break;

		case EMMC_BW_4BIT:
			REG_FCR_FUNC_CTRL &= ~FCR_FUNC_CTRL_MMC_8BIT;
			break;

		case EMMC_BW_8BIT:
			REG_FCR_FUNC_CTRL |= FCR_FUNC_CTRL_MMC_8BIT;
			break;

		default:
			printf("invalid bus width(%d)", bus_width);	
			return 0;
	}
//	mdelay(1);
	return 1;
}

static void nvt_sdhci_set_control_reg(struct sdhci_host *host)
{
	struct mmc *mmc = host->mmc;

	/* set clock rate */
	if (mmc->clock){ 
	static int cur_clk = 0;
#if 1
		if(mmc->clock > CLK_52M  ){
            /* our controller has a bug, need to reset CMD&DATA after enabling HS200 speed mode */
            /* 1. switch to higher clk */
            _emmc_init_clk(13000000);
			cur_clk = 13000000;
            /* 2. reset CMD&DATA */
            REG_SDC_SW_RESET |= SDC_SW_RESET_CMD_LINE | SDC_SW_RESET_DAT_LINE;
            /* OK, wait for reset done */
            while (REG_SDC_SW_RESET & SDC_SW_RESET_CMD_LINE);
            while (REG_SDC_SW_RESET & SDC_SW_RESET_DAT_LINE);
		}
#endif
		if(mmc->clock != cur_clk)
			_emmc_init_clk(mmc->clock);
		cur_clk = mmc->clock;
		host->clock = mmc->clock; //for common interface
	}

	/* Set the bus width */
	if (mmc->bus_width) {

		switch (mmc->bus_width) {
		case 1:
			_emmc_host_switch_bus_width(EMMC_BW_1BIT);
			break;
		case 4:
			_emmc_host_switch_bus_width(EMMC_BW_4BIT);
			break;
		case 8:
			_emmc_host_switch_bus_width(EMMC_BW_8BIT);
			break;
		default:
			printf("Invalid bus width: %d\n", mmc->bus_width);
			break;
		}
	}

	/* check relation between ddr_mode and latch , remove it when data latch is not removed.
	*/	
	switch(mmc->ddr_mode){
		case 0:
			_emmc_set_bus_timing_mode( mmc->tran_speed, EMMC_SINGLE_LATCH);
			break;
		case 1:
			_emmc_set_bus_timing_mode( mmc->tran_speed, EMMC_DUAL_LATCH);
			break;
		default : 
			//printf("Invalid data latch: %d\n", dev->data_latch);
			break;		
	
	}

}

/*
 * mmc_host_init - initialize the mmc controller.
 * Set initial clock and power for mmc slot.
 * Initialize mmc struct and register with mmc framework.
 */
int nvt_mmc_init(void)
{
	
	//init host controler
	_emmc_arch_host_preinit();

	/* this register might be modified by STBC, need to set its default value again, or cmd might error */
	REG_FCR_CPBLT = 0xf8934ff;

	/* set DMA beat mode */
	REG_FCR_FUNC_CTRL = (EMMC_DMA_BEAT_16_8_4<<20) | 0xf3020;

	/* enable SW card detect function */
	REG_FCR_FUNC_CTRL |= FCR_FUNC_CTRL_SW_CDWP_ENABLE;

	/* force card always exists */
	REG_FCR_FUNC_CTRL &= ~FCR_FUNC_CTRL_SW_SD_CD;

	/* data bus is little endian */
	REG_FCR_FUNC_CTRL |= FCR_FUNC_CTRL_LITTLE_ENDIAN;

	/* do not sd clk bypass */
	REG_FCR_CPBLT &= ~FCR_CPBLT_SD_CLK_BYPASS;

	/* since NFC has nand and eMMC, select eMMC */
	REG_NFC_SYS_CTRL |= NFC_EMMC_SEL;


	/* SW reset */
	REG_SDC_SW_RESET = SDC_SW_RESET_ALL | SDC_SW_RESET_CMD_LINE | SDC_SW_RESET_DAT_LINE;

	while (REG_SDC_SW_RESET & (SDC_SW_RESET_ALL | SDC_SW_RESET_CMD_LINE | SDC_SW_RESET_DAT_LINE)) {
		//printf("wait for sdc sw reset stable...");
		printf("...");
	}

	udelay(1);

	/* how much time to timeout when waiting device's DATA[0:7] */
	REG_SDC_TIMEOUT_CTRL = SDC_TIMEOUT_CTRL_DATA_TIMEOUT(0xf);

	REG_SDC_INT_ENABLE = 0;         // mask all interrupt
	REG_SDC_INT_STAT_ENABLE = 0;    // disable all interrupt status
	REG_SDC_INT_STAT = -1;          // clear all interrupt status
	REG_SDC_INT_STAT_ENABLE = -1;   // enable all interrupt status

	/* delay latch data timing, clk will not delay for now */
	REG_FCR_FUNC_CTRL &= ~FCR_FUNC_CTRL_SD_SIG_DELAY_MASK;
	REG_FCR_FUNC_CTRL |= FCR_FUNC_CTRL_SD_SIG_DELAY(0); 

	/* host will read data x ns after latching, x is 0(0x00), 3(0x01), 6(0x02), or 9(0x03) on [28:29], now is 0 */
	REG_FCR_FUNC_CTRL &= ~FCR_FUNC_CTRL_READ_CLK_DELAY_MASK;
	REG_FCR_FUNC_CTRL |= FCR_FUNC_CTRL_READ_CLK_DELAY(0);

	/* disable all of HS200 functions for now, 
	 * make sure emmc clk is not used the fastest clk setting, which will force the bus clk = CLKSRC/3 */
	REG_FCR_HS200_CTRL = 0;

	/* must use 3.3V to set up...beacause our design issue... but do not worry, the bus power depends on external CKT :-) */
	if (!_emmc_bus_init(SDC_PW_CTRL_BUS_VOL_33V)) {
		printf("bus init failed");
		return -1;
	}

	/* before issue any cmd, wait at least 74 clk */		
	mdelay(1);

	return 0;
}

static int do_mmc_test(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char cmd[64] ;
	unsigned char *buf = (unsigned char *)(0x5000000);
	int i =0;
	int ret = 0;
//test 1
	printf("pass1\n");
	for(i = 0;i<512;i++){
		buf[i] = 0xee;
	}
	for(i = 0;i<512;i++){
		buf[i+512] = 0xcc;
	}
	sprintf(cmd,"mmc write 0x%lx 100 2", (unsigned long)buf);
	run_command(cmd, 0 );
	for(i = 0;i<512*2;i++){
		buf[i] = 0x0;
	}
	sprintf(cmd,"mmc read 0x%lx 100 2", (unsigned long)buf);
	run_command(cmd, 0 );
	for(i = 0;i<512;i++){
		if(buf[i] != 0xee){
			printf("mmc test error @%d %x:0xee\n",i,(unsigned char)buf[i]  );
			ret = 1;
		}
	}
	for(i = 0;i<512;i++){
		if(buf[i+512] != 0xcc){
			printf("mmc test error @%d %x:0xcc\n",i+512,(unsigned char)buf[i+512]  );
			ret = 1;
		}
	}
	printf("pass2\n");
//test 2 
	for(i = 0;i<256;i++){
		buf[i] = i;
	}
	for(i = 256;i<512;i++){
		buf[i] = (512-i-1);
	}
	for(i = 0;i<256;i++){
		buf[i+512] = 256-i-1;
	}
	for(i = 256;i<512;i++){
		buf[i+512] = i-256;
	}
	sprintf(cmd,"mmc write 0x%lx 100 2", (unsigned long)buf);
	run_command(cmd, 0 );
	for(i = 0;i<512*2;i++){
		buf[i] = 0x0;
	}
	sprintf(cmd,"mmc read 0x%lx 100 2", (unsigned long)buf);
	run_command(cmd, 0 );
	for(i = 0;i<256;i++){
		if(buf[i] != i){
			printf("mmc test error @%d %x:0x%x\n",i,(unsigned char)buf[i]  , i);
			ret = 1;
		}
	}
	for(i = 256;i<512;i++){
		if(buf[i] != (512-i-1)){
			printf("mmc test error @%d %x:0x%x\n",i,(unsigned char)buf[i] ,(512-i-1) );
			ret = 1;
		}
	}
	for(i = 0;i<256;i++){
		if(buf[i+512] != (256-i-1)){
			printf("mmc test error @%d %x:%x\n",i+512,(unsigned char)buf[i+512] , (256-i-1) );
			ret = 1;
		}
	}
	for(i = 256;i<512;i++){
		buf[i+512] = i-256;
		if(buf[i+512] != i-256){
			printf("mmc test error @%d %x:%x\n",i+512,(unsigned char)buf[i+512] , i-256);
			ret = 1;
		}
	}
	return ret;
	
}

U_BOOT_CMD(
	mmc_test, 3, 1, do_mmc_test,
	"mmc test ",
	"mmc test write read compare\n"
	"mmc test write read compare"
);

static int nvt_emmc_ofdata_to_platdata(struct udevice *dev)
{
	struct nvt_emmc_data *priv = dev_get_priv(dev);
	struct mmc_config *cfg;
	const void *fdt = gd->fdt_blob;
	int node = dev->of_offset;


	cfg = &priv->cfg;
	cfg->f_min = CLK_400K;
	cfg->f_max = CLK_52M;
#ifdef CONFIG_NVT_EMMC_DDR52
	cfg->host_caps |= MMC_MODE_DDR_52MHz;
#endif
#ifdef CONFIG_NVT_EMMC_HS200
	cfg->host_caps |=  MMC_MODE_HS200;
	cfg->f_max = fdtdec_get_int(fdt, node, "max-bus-frequency", 52000000);
#endif
	printf("%d\n", cfg->f_max);

	return 0;
}

static int nvt_emmc_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct nvt_emmc_data *priv = dev_get_priv(dev);
	struct sdhci_host *host = dev_get_priv(dev);

	host = (struct sdhci_host *)calloc(1, sizeof(struct sdhci_host));
	if (!host) {
		printf("%s: sdhci_host calloc failed\n", __func__);
		return -ENOMEM;
	}

	host->name = strdup(dev->name);
	host->ioaddr = (void *)dev_get_addr(dev);
	host->set_control_reg = &nvt_sdhci_set_control_reg;
	host->version = sdhci_readw(host, SDHCI_HOST_VERSION);
	host->quirks = 0;

	nvt_mmc_init();

	//CONFIG_NVT_EMMC_SET_PLL is for ASIC only
#ifdef CONFIG_NVT_EMMC_SET_PLL
	extern unsigned long set_emmc_clk(unsigned long  freq);	
	//emmc pll = emmc clk *4
	set_emmc_clk(priv->cfg.f_max *4);
#endif

	add_sdhci(host, priv->cfg.f_max, priv->cfg.f_min);
	host->mmc->dev = dev;
	upriv->mmc = host->mmc;

	return 0;
}

static const struct udevice_id nvt_emmc_ids[] = {
	{ .compatible = "nvt,hsmmc" },
	{ }
};
U_BOOT_DRIVER(nvt_emmc) = {
	.name	= "nvt_emmc",
	.id	= UCLASS_MMC,
	.of_match = nvt_emmc_ids,
	.ofdata_to_platdata = nvt_emmc_ofdata_to_platdata,
	.probe	= nvt_emmc_probe,
	.priv_auto_alloc_size = sizeof(struct nvt_emmc_data),
};
#if defined(CONFIG_EMMC_WRITE_RELIABILITY_TEST)
int nvt_mmc_power_down(u32 wr_test_cnt, u32 wr_total_test_cnt)
{
    if(wr_test_cnt == wr_total_test_cnt && wr_total_test_cnt!=0){
	printf("\neMMC Write Reliability Test End!\n");

    } else {
	nvt_stbc_set_keypass(1);
	GPIO_SWITCH_CTRL = 0xFFEFF;
        SET_EMMC_POWER_DOWN_FUN |= (1<<3);
        SET_EMMC_POWER_DOWN_DIR |= (1<<3);
        SET_EMMC_POWER_DOWN_PUL &= (~(1<<3));
        printf("\neMMC Power Down\n");
        run_command("reboot", 0);
    }
        while(1);

        return 0;

}
#endif

