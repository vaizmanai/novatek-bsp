#include <nvt_nand_fwupdate.h>
#include "update_cmds.h"
#include <common.h>
#include <nvt_nand_utils.h>

struct cmd_mapping cmdmap[] = {
    { "npt",         "n33005_nand_partition.csv",       nvt_nand_update_npt,     nvt_nand_erase_common, "partition table" },
    { "xboot",       "xboot.bin",                       nvt_nand_update_loader,  nvt_nand_erase_common, "xboot image" },
    { "ddrcfg",      "673_ddr_cfg.ddr",                 nvt_nand_update_bin,     nvt_nand_erase_common, "ddr script"  },
    { "ddrbud",      "673_ddr_cfg.bud",                 nvt_nand_update_bin,     nvt_nand_erase_common, "ddr external" },
    { "xbootdat",    "xbootdat.bin",                    nvt_nand_update_bin,     nvt_nand_erase_common, "xboot data" },
    { "secos",       "secos.img",                       nvt_nand_update_bin,     nvt_nand_erase_common, "secos image" },
    { "uboot",       "u-boot.img",                      nvt_nand_update_bin,     nvt_nand_erase_common, "uboot image" },
    { "fdt0",        "nvtca53_673.dtb.img",             nvt_nand_update_fdt,     nvt_nand_erase_common, "Device Tree" },
    { "fdt1",        "nvtca53_673_sup.dtb.img",         nvt_nand_update_fdt,     nvt_nand_erase_common, "Device Tree" },
    { "ker0",        "uImage.ngz",                      nvt_nand_update_bin,     nvt_nand_erase_common, "kernel image" },
    { "ker1",        "uImage_sup.ngz",                  nvt_nand_update_bin,     nvt_nand_erase_common, "kernel image" },
    { "fs0",         "rootfs.img",                      nvt_nand_update_ubi,     nvt_nand_erase_common, "root file system" },
    { "ap0",         "board.img",                       nvt_nand_update_ubi,     nvt_nand_erase_common, "board image" },
    { "apdat0",      "conf.img",                        nvt_nand_update_ubi,     nvt_nand_erase_common, "apdat image" },
    { "buffer",      "buffer.img",                      nvt_nand_update_ubi,     nvt_nand_erase_common, "buffer image" },
    { "logo",        "logo.jpg",                        nvt_nand_update_bin,     nvt_nand_erase_common, "boot logo image" },
    { NULL,          NULL,                              NULL,                    NULL,                   NULL }

};

void nvt_fwupdate_help_init(void)
{
	_nvt_fwupdate_help_init(cmdmap);
}

static int do_nvt_nwriteall(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{

#if defined(CONFIG_CMD_NVT_SCRIPT_UPDATE)
	if(run_command("nvt_script_update", 0) == 0) {
		printf("[Finish] Please reboot this device.\n");
		while(1);
	}
#endif

	nvt_run_cmd("swrite stbc", 0, "Writing STBC to SPI");
	nvt_run_cmd("nand erase.chip",0, "Erease Chip");

	nvt_run_cmd("nwrite npt", 0, "Writing NPT");
	nvt_run_cmd("nwrite xboot", 0, "Writing xboot");
	nvt_run_cmd("nwrite ddrcfg", 0, "Writing DDR cfg");
	nvt_run_cmd("nwrite xbootdat", 0, "Writing Panel config");
	nvt_run_cmd("nwrite fdt0", 0, "Writing fdt0");
	nvt_run_cmd("nwrite fdt1", 0, "Writing fdt1");
	nvt_run_cmd("nwrite secos", 0, "Writing Sec OS to NAND");
	nvt_run_cmd("nwrite uboot", 0, "Writing U-Boot");
	nvt_run_cmd("nwrite ker0", 0, "Writing ker0");
	nvt_run_cmd("nwrite ker1", 0, "Writing ker1");
	nvt_run_cmd("nwrite fs0", 0, "Writing fs0");
	nvt_run_cmd("nwrite ap0", 0, "Writing ap0");
	nvt_run_cmd("nwrite apdat0", 0, "Writing apdat0");
	nvt_run_cmd("nwrite buffer", 0, "Writing buffer");
	nvt_run_cmd("nwrite logo", 0, "Writing logo");

	nvt_run_cmd("env default -a", 0, "Setting default env");
	nvt_run_cmd("xsave kercmd", 0, "Generating kernel cmdline");

	printf("[Finish] Please reboot this device.\n");

	while(1);

	return -1;
}

U_BOOT_CMD(
        nwriteall,3,    0,  do_nvt_nwriteall,
        "nwriteall    - write all images to nand\n",
        ""
);

