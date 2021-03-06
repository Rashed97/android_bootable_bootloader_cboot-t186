/*
 * Copyright (c) 2014-2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_LINUXBOOT

#include "build_config.h"

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <libfdt.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_utils.h>
#include <tegrabl_linuxboot.h>
#include <tegrabl_linuxboot_helper.h>
#include <tegrabl_plugin_manager.h>
#include <tegrabl_odmdata_lib.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_board_info.h>
#include <tegrabl_nct.h>
#include <tegrabl_malloc.h>
#include <arch/tegrabl_arm64_smccc.h>
#include <tegrabl_cache.h>

#if defined(POPULATE_BOARDINFO)
/* Add boardinfo under 'chosen' node in DTB. */
static tegrabl_error_t add_board_info(void *fdt, int nodeoffset)
{
	int err;
	uint32_t idx;
	int node;
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	uint32_t boardinfo[TEGRABL_LINUXBOOT_BOARD_MAX_FIELDS];
	struct {
		char *name;
		enum tegrabl_linuxboot_board_type type;
	} boardlist[] = {
		{ "proc-board", TEGRABL_LINUXBOOT_BOARD_TYPE_PROCESSOR } ,
		{ "pmu-board", TEGRABL_LINUXBOOT_BOARD_TYPE_PMU } ,
		{ "display-board", TEGRABL_LINUXBOOT_BOARD_TYPE_DISPLAY } ,
	};

	TEGRABL_ASSERT(fdt);

#define set_board_prop(prop, name) do {									\
		err = fdt_setprop_cell(fdt, node, (name), boardinfo[(prop)]);	\
		if (err < 0) {													\
			pr_error(						\
				"%s: Unable to set /chosen/%s (%s)\n", __func__,		\
				(name), fdt_strerror(err));								\
			return TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);	\
		}																\
	} while (0)

	for (idx = 0; idx < ARRAY_SIZE(boardlist); idx++) {
		status = tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_BOARD,
			&(boardlist[idx].type), boardinfo);
		if (status != TEGRABL_NO_ERROR) {
			pr_error("Unable to fetch %s info\n", boardlist[idx].name);
			goto fail;
		}

		node = tegrabl_add_subnode_if_absent(fdt, nodeoffset,
			boardlist[idx].name);
		if (node < 0) {
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
			goto fail;
		}

		/* Set 'board_id' */
		set_board_prop(TEGRABL_LINUXBOOT_BOARD_ID, "id");

		/* Set 'sku' */
		set_board_prop(TEGRABL_LINUXBOOT_BOARD_SKU, "sku");

		/* Set 'fab' */
		set_board_prop(TEGRABL_LINUXBOOT_BOARD_FAB, "fab");

		/* Set 'major_revision' */
		set_board_prop(TEGRABL_LINUXBOOT_BOARD_MAJOR_REV, "major_revision");

		/* Set 'minor_revision' */
		set_board_prop(TEGRABL_LINUXBOOT_BOARD_MINOR_REV, "minor_revision");

		pr_debug("Updated %s info to DTB\n", "board");
	}

fail:
	return status;
}
#endif

/* Add odmdata under 'chosen/plugin-manager' node in DTB. */
static tegrabl_error_t add_odmdata_info(void *fdt, int nodeoffset)
{
	int err;
	uint32_t idx;
	int node;
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	uint32_t odmdata = tegrabl_odmdata_get();
	uint32_t mask;
	uint32_t val;
	struct odmdata_params *podmdata_list = NULL;
	uint32_t odmdata_list_size;

	TEGRABL_ASSERT(fdt);

	status = tegrabl_odmdata_params_get(&podmdata_list, &odmdata_list_size);
	if (status) {
		goto fail;
	}

	node = tegrabl_add_subnode_if_absent(fdt, nodeoffset, "plugin-manager");
	if (node < 0) {
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 1);
		goto fail;
	}

	node = tegrabl_add_subnode_if_absent(fdt, node, "odm-data");
	if (node < 0) {
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 2);
		goto fail;
	}

	for (idx = 0; idx < odmdata_list_size; idx++) {
		mask = podmdata_list[idx].mask;
		val = podmdata_list[idx].val;
		if ((odmdata & mask) != val) {
			continue;
		}

		/* Add property */
		err = fdt_setprop_cell(fdt, node, podmdata_list[idx].name, 1);
		if (err < 0) {
			pr_error("Unable to set /chosen/plugin-manager/%s (%s)\n",
				podmdata_list[idx].name, fdt_strerror(err));
			status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
			goto fail;
		}
	}

	pr_debug("Updated %s info to DTB\n", "odmdata");

fail:
	if (status) {
		pr_error("Error = %d in 'add_odmdata_info'\n", status);
	}
	return status;
}

#define MAX_MEM_CHUNKS	20

static tegrabl_error_t add_memory_info(void *fdt, int nodeoffset)
{
	uint32_t num_memory_chunks = 0, idx;
	uint64_t buf[2 * MAX_MEM_CHUNKS];
	uint64_t tmp;
	struct tegrabl_linuxboot_memblock memblock;
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	int err;
	char *name;
	uint32_t odmdata = tegrabl_odmdata_get();

	/*TOS In-sync structures - BEGIN*/
	struct tos_ns_mem_map_entry_t {
		uint64_t base;
		uint64_t size;
	};

	struct __attribute__ ((__packed__)) tos_ns_mem_map_t {
		uint32_t magic;
		uint32_t ver;
		uint32_t num;
		uint8_t reserved[2];
		struct tos_ns_mem_map_entry_t mappings[0];
	};

	#define TOS_NS_MEM_MAP_SIZE_MAX (sizeof(struct tos_ns_mem_map_t) \
				+ (MAX_MEM_CHUNKS * sizeof(struct tos_ns_mem_map_entry_t)))
	#define TOS_NS_MEM_MAP_MAGIC_VALUE (0xfeedbeef)
	#define TOS_NS_MEM_MAP_CUR_VERSION (0x1)
	#define SMC_FC_REGISTER_NS_DRAM_RANGES 0xBC00000C /* Fastcall For Trusty */
	/*TOS In-sync structures - END*/

	TEGRABL_ASSERT(fdt);

	struct tegrabl_arm64_smc64_params regs = {
		{SMC_FC_REGISTER_NS_DRAM_RANGES}
	};

	struct tos_ns_mem_map_t *ns_mem_map = NULL;
	ns_mem_map = (struct tos_ns_mem_map_t *)tegrabl_calloc(1,
			TOS_NS_MEM_MAP_SIZE_MAX);
	if (NULL == ns_mem_map) {
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
	}

	for (idx = 0; idx < MAX_MEM_CHUNKS; idx++) {
		status = tegrabl_linuxboot_helper_get_info(
				TEGRABL_LINUXBOOT_INFO_MEMORY, &idx, &memblock);

		if ((status != TEGRABL_NO_ERROR) ||
					(memblock.size == 0) ||
					(num_memory_chunks >= MAX_MEM_CHUNKS)) {
			break;
		}

		/* if odmdata bit[6] is set, only pass lower 4G memory to kernel */
		#define ODMDATA_EVAL_4G_MEM (1 << 6)
		#define SDRAM_START_ADDRESS (0x80000000)
		#define SDRAM_OFFSET_4G (SDRAM_START_ADDRESS + 0x100000000LLU)
		if ((odmdata & ODMDATA_EVAL_4G_MEM) &&
			(memblock.size + memblock.base > SDRAM_OFFSET_4G)) {
			if (memblock.base >= SDRAM_OFFSET_4G) {
				break;
			} else {
				memblock.size = SDRAM_OFFSET_4G - memblock.base;
			}
		}

		/* Align start/base to 2MB */
		tmp = ROUND_UP_POW2(memblock.base, 0x200000LLU);
		if (tmp >= (memblock.base + memblock.size)) {
			continue;
		}

		/* Adjust the size accordingly */
		memblock.size -= (tmp - memblock.base);

		/* kernel crashes if bootloader passed odd number of MBs */
		memblock.size &= ~0x1FFFFFLLU;

		memblock.base = tmp;

		if (memblock.size > 0) {
			ns_mem_map->mappings[num_memory_chunks].base = memblock.base;
			ns_mem_map->mappings[num_memory_chunks].size = memblock.size;
			buf[num_memory_chunks * 2 + 0] = cpu_to_fdt64(memblock.base);
			buf[num_memory_chunks * 2 + 1] = cpu_to_fdt64(memblock.size);
			pr_info("added [base:0x%"PRIx64", size:0x%"PRIx64"] to /memory\n",
					memblock.base, memblock.size);
			num_memory_chunks++;
		}
	}

	/*Following values are expected by Trusty as a SMC*/
	ns_mem_map->magic = TOS_NS_MEM_MAP_MAGIC_VALUE;
	ns_mem_map->ver = TOS_NS_MEM_MAP_CUR_VERSION;
	ns_mem_map->num = num_memory_chunks;

	tegrabl_arch_clean_dcache_range((uintptr_t) ns_mem_map,
			TOS_NS_MEM_MAP_SIZE_MAX);

	/* Currently, trusty only supports 32bit arguments */
	regs.reg[1] = (uint32_t) ((uint64_t)ns_mem_map & ((1ULL << 32) - 1));
	regs.reg[2] = (uint32_t) ((uint64_t)ns_mem_map >> 32);
	regs.reg[3] = TOS_NS_MEM_MAP_SIZE_MAX;

	tegrabl_arm64_send_smc64(&regs);

	if (regs.reg[0] != 0) {
		/*TODO: Halt here if we fail?*/
		pr_error("WARNING: Failed to pass NS DRAM ranges to TOS\n");
	} else {
		pr_info("NS DRAM ranges recorded by TOS\n");
	}

	if (ns_mem_map) {
		memset(ns_mem_map, 0, TOS_NS_MEM_MAP_SIZE_MAX);
		tegrabl_free(ns_mem_map);
		ns_mem_map = NULL;
	}

	/* Update the /memory node of DTB
	 * Limitation: This implementation assumes #address-cells and
	 * #size-cells
	 * to be 2 i.e. only for 64bit addr/size pairs */

	name = "memory";
	err = fdt_setprop(fdt, nodeoffset, "device_type",
						 name, strlen(name)+1);
	if (err < 0) {
		pr_error("Failed to update /memory/%s in DTB (%s)\n",
				 "device_type", fdt_strerror(err));
		err = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		goto fail;
	}

	if (num_memory_chunks) {
		err = fdt_setprop(fdt, nodeoffset, "reg", buf,
						  num_memory_chunks * 2 * sizeof(uint64_t));
		if (err < 0) {
			pr_error("Failed to update /memory/%s in DTB (%s)\n",
					 "reg", fdt_strerror(err));
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
			goto fail;
		}
	}

	pr_info("Updated %s info to DTB\n", "memory");

fail:
	return status;
}

static tegrabl_error_t add_initrd_info(void *fdt, int nodeoffset)
{
	int ret = TEGRABL_NO_ERROR;
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	struct tegrabl_linuxboot_memblock memblock;
	uint32_t buf;

	TEGRABL_ASSERT(fdt);

	if (tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_INITRD,
			NULL, &memblock) != TEGRABL_NO_ERROR) {
		goto fail;
	}

	buf = cpu_to_fdt32((uint32_t)memblock.base);
	ret = fdt_setprop(fdt, nodeoffset, "linux,initrd-start", &buf, sizeof(buf));
	if (ret < 0) {
		pr_error("Unable to set \"%s\" in FDT\n", "linux,initrd-start");
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		goto fail;
	}

	buf = cpu_to_fdt32((uint32_t)(memblock.base + memblock.size));
	ret = fdt_setprop(fdt, nodeoffset, "linux,initrd-end", &buf, sizeof(buf));
	if (ret < 0) {
		pr_error("Unable to set \"%s\" in FDT\n", "linux,initrd-end");
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		goto fail;
	}

	pr_info("Ramdisk: Base: 0x%lx; Size: 0x%lx\n",
			memblock.base, memblock.size);
	pr_info("Updated %s info to DTB\n", "initrd");

fail:
	return status;
}

static tegrabl_error_t add_bpmp_info(void *fdt, int nodeoffset)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	struct tegrabl_linuxboot_memblock memblock;
	enum tegrabl_linuxboot_carveout_type carveout_type =
		TEGRABL_LINUXBOOT_CARVEOUT_BPMPFW;
	int err;
	uint32_t buf;

	TEGRABL_ASSERT(fdt);

	status = tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_CARVEOUT,
											   &carveout_type, &memblock);
	if (status != TEGRABL_NO_ERROR) {
		pr_error("Failed to get bpmp mem layout\n");
		status = tegrabl_err_set_highest_module(status, TEGRABL_ERR_LINUXBOOT);
		goto fail;
	}

	/* Bail-out if carveout not present */
	if ((memblock.size == 0x0) || (memblock.base == 0x0)) {
		pr_info("Skipping BPMP FW node addition\n");
		goto fail;
	}

	buf = cpu_to_fdt32((uint32_t)memblock.base);
	err = fdt_setprop(fdt, nodeoffset, "carveout-start", &buf, sizeof(buf));
	if (err) {
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		goto fail;
	}

	buf = cpu_to_fdt32((uint32_t)memblock.size);
	err = fdt_setprop(fdt, nodeoffset, "carveout-size", &buf, sizeof(buf));
	if (err) {
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		goto fail;
	}

	pr_info("Updated %s info to DTB\n", "bpmp");

fail:
	return status;
}

#define MAX_COMMAND_LINE_SIZE   2048
static char cmdline[MAX_COMMAND_LINE_SIZE];

/* override param value in commandline */
static tegrabl_error_t param_value_override(char *cmdline, int *len,
											char *param, char *value)
{
	char *p = param;
	int cmd_len = *len;
	char *c, *pre_end, *end;
	int new_len, old_len;

	if (!cmdline || !len || !param || !value)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	if (*param == '\0')
		return TEGRABL_NO_ERROR;

	end = cmdline + strlen(cmdline);
	for (; *cmdline != '\0'; cmdline++) {
		if (*cmdline != *p) {
			continue;
		}

		c = cmdline;
		while (*c++ == *p++)

			/* at this point, the param matched is located */
			if (*p == '\0') {

				/* found param's old value */
				cmdline = c;
				while ((*c != ' ') && (*c != '\0'))
					c++;
				old_len = c - cmdline;
				new_len = strlen(value);

				/* if new value is longer than old, move right bytes behind */
				if (new_len > old_len) {
					pre_end = end;
					end = pre_end + (new_len - old_len);
					if (cmd_len + (new_len - old_len) > MAX_COMMAND_LINE_SIZE)
						return TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);

					while (pre_end >= c)
						*end-- = *pre_end--;
				}

				/* update param value */
				cmdline = strncpy(cmdline, value, new_len);
				cmdline += new_len;
				cmd_len += new_len - old_len;

				/* if new value is shorter than old, move right bytes ahead */
				if (new_len <= old_len) {
					while (*c != '\0')
						*cmdline++ = *c++;
					*cmdline = '\0';
				}

				*len = cmd_len;
				return TEGRABL_NO_ERROR;
			}
		p = param;
	}

	return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
}

static tegrabl_error_t add_bootarg_info(void *fdt, int nodeoffset)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	char *dtb_cmdline;
	char *ptr;
	int len = 0;
	int remain = MAX_COMMAND_LINE_SIZE;
	int err;
	enum tegrabl_linuxboot_debug_console console;

	TEGRABL_ASSERT(fdt);

	ptr = cmdline;

	TEGRABL_LTRACE;
	dtb_cmdline = (char *)fdt_getprop(fdt, nodeoffset, "bootargs", &len);
	if (len <= 0) {
		len = 0;
		dtb_cmdline = "";
	}

	err = tegrabl_linuxboot_helper_get_info(
						TEGRABL_LINUXBOOT_INFO_DEBUG_CONSOLE, NULL, &console);
	if (err == TEGRABL_NO_ERROR) {
		if (console == TEGRABL_LINUXBOOT_DEBUG_CONSOLE_NONE)
			param_value_override(dtb_cmdline, &len, "console=", "none");
	} else
		pr_warn("WARN: Fail to override \"console=none\" in commandline\n");

	len = tegrabl_snprintf(ptr, remain, "%s ",
			tegrabl_linuxboot_prepare_cmdline(dtb_cmdline));
	if ((len > 0) && (len <= remain)) {
		ptr += len;
		remain -= len;
	}

	err = fdt_setprop(fdt, nodeoffset, "bootargs", cmdline,
					  MAX_COMMAND_LINE_SIZE - remain + 1);
	if (err < 0) {
		pr_error("Failed to set bootargs in DTB (%s)\n", fdt_strerror(err));
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		goto fail;
	}

	pr_info("Updated %s info to DTB\n", "bootarg");

fail:
	return status;
}

/**
 * @brief Metadata for a MAC Address of a certain interface
 *
 * @param type_of_interface - String representation of the interface type
 * @param chosen_prop		- The corresponding property in chosen node
 */
struct mac_addr_meta_data_type {
	char *type_of_interface;
	char *chosen_prop;
};

struct mac_addr_meta_data_type mac_addr_meta_data[MAC_ADDR_TYPE_MAX] = {
	[MAC_ADDR_TYPE_WIFI] = {
		.type_of_interface = "WIFI",
		.chosen_prop = "nvidia,wifi-mac"
	},
	[MAC_ADDR_TYPE_BT] = {
		.type_of_interface = "Bluetooth",
		.chosen_prop = "nvidia,bluetooth-mac"
	},
	[MAC_ADDR_TYPE_ETHERNET] = {
		.type_of_interface = "Ethernet",
		.chosen_prop = "nvidia,ether-mac"
	},
};

#if defined(CONFIG_ENABLE_EEPROM)
/* Helper function to common out MAC address installation code in DeviceTree */
static tegrabl_error_t install_mac_addr(void *fdt, int nodeoffset,
										enum mac_addr_type type,
										char *mac_addr)
{
	char *chosen_prop = mac_addr_meta_data[type].chosen_prop;
	char *interface = mac_addr_meta_data[type].type_of_interface;
	int err;

	err = fdt_setprop_string(fdt, nodeoffset, chosen_prop, mac_addr);
	if (err < 0) {
		pr_error("Failed to install %s MAC Addr in DT (%s)\n",
				 interface, fdt_strerror(err));
		return TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, type);
	}

	pr_debug("%s: %s MAC Address = %s\n", __func__, interface, mac_addr);
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t add_mac_addr_info(void *fdt, int nodeoffset)
{
	char mac_addr[MAC_ADDR_STRING_LEN];
	uint32_t i;
	tegrabl_error_t status;

	for (i = 0; i < MAC_ADDR_TYPE_MAX; i++) {

		status = tegrabl_get_mac_address(i, (uint8_t *)mac_addr);
		if (status != TEGRABL_NO_ERROR) {
			pr_warn("Failed to get %s MAC address\n",
					 mac_addr_meta_data[i].type_of_interface);
			/* MAC Address failure is not critical enough to stop booting */
			continue;
		}

		status = install_mac_addr(fdt, nodeoffset, i, mac_addr);
		if (status != TEGRABL_NO_ERROR) {
			pr_warn("Failed to update DT for %s MAC address\n",
					 mac_addr_meta_data[i].type_of_interface);
		}
	}

	return TEGRABL_NO_ERROR;
}
#endif

static tegrabl_error_t tegrabl_update_tos_nodes(void *fdt)
{
	tegrabl_error_t err = 0;
	enum tegrabl_tos_type tos_type = 0;
	int node, fdt_err;
	char *tos_names[3] = { NULL, "tlk", "trusty" };

	err = tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_SECUREOS,
			NULL, &tos_type);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* get the secureos node */
	switch (tos_type) {
	case TEGRABL_TOS_TYPE_TLK:
		node = fdt_path_offset(fdt, "/tlk");
		if (node < 0) {
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 0);
			goto fail;
		}
		break;

	case TEGRABL_TOS_TYPE_TRUSTY:
		node = fdt_path_offset(fdt, "/trusty");
		if (node < 0) {
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 0);
			goto fail;
		}
		break;

	default:
		/* no secureos */
		return err;
	}

	/* enable the node */
	pr_info("Enable TOS: %s\n", tos_names[tos_type]);
	fdt_err = fdt_setprop_string(fdt, node, "status", "okay");
	if (fdt_err < 0) {
		err = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
	}

fail:
	return err;
}

static tegrabl_error_t tegrabl_add_serialno(void *fdt)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	char sno[SNO_SIZE + 1];
	int fdt_err;

	err = tegrabl_get_serial_no((uint8_t *)sno);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get serial number\n");
		return err;
	}

	pr_info("Add serial number as DT property\n");
	fdt_err = fdt_setprop_string(fdt, 0, "serial-number", sno);
	if (fdt_err < 0) {
		pr_error("Failed to add serialno in DT\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
	}

	return TEGRABL_NO_ERROR;
}

#if defined(CONFIG_ENABLE_NCT)
static tegrabl_error_t add_tnspec_info(void *fdt, int nodeoffset)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	char id[NCT_MAX_SPEC_LENGTH], config[NCT_MAX_SPEC_LENGTH];
	int node, fdt_err;

	status = tegrabl_nct_get_spec(id, config);
	if (status != TEGRABL_NO_ERROR) {
		/* if read fails, print warning and skip */
		pr_warn("WARNING: Getting spec not successful, skip...\n");
		return TEGRABL_NO_ERROR;
	}

	node = tegrabl_add_subnode_if_absent(fdt, nodeoffset, "plugin-manager");
	if (node < 0) {
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
		goto fail;
	}

	node = tegrabl_add_subnode_if_absent(fdt, node, "tnspec");
	if (node < 0) {
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
		goto fail;
	}

	pr_debug("Adding tnspec/id: %s\n", id);
	fdt_err = fdt_setprop_string(fdt, node, "id", id);
	if (fdt_err < 0) {
		pr_error("Failed to add tnspec/id in DTB\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		goto fail;
	}

	pr_debug("Adding tnspec/config: %s\n", config);
	fdt_err = fdt_setprop_string(fdt, node, "config", config);
	if (fdt_err < 0) {
		pr_error("Failed to add tnspec/config in DTB\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		goto fail;
	}

fail:
	return status;
}
#endif

static struct tegrabl_linuxboot_dtnode_info common_nodes[] = {
	/* keep this sorted by the node_name field */
	{ "bpmp", add_bpmp_info},
	{ "chosen", add_initrd_info},
#if defined(POPULATE_BOARDINFO) /* Only use this for T210 */
	{ "chosen", add_board_info},
#endif
	{ "chosen", add_bootarg_info},
	{ "chosen", add_odmdata_info},
#if defined(CONFIG_ENABLE_NCT)
	{ "chosen", add_tnspec_info},
#endif
#if defined(CONFIG_ENABLE_EEPROM)
	{ "chosen", add_mac_addr_info},
	{ "chosen", tegrabl_add_plugin_manager_ids},
#endif
	{ "memory", add_memory_info},
	{ NULL, NULL},
};

tegrabl_error_t tegrabl_linuxboot_update_dtb(void *fdt)
{
	uint32_t i;
	int node = -1;
	int prev_offset = -1;
	char *prev_name = NULL;
	tegrabl_error_t status;
	struct tegrabl_linuxboot_dtnode_info *extra_nodes = NULL;

	if (fdt == NULL)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	for (i = 0; common_nodes[i].node_name != NULL; i++) {
		if (!prev_name || strcmp(prev_name, common_nodes[i].node_name)) {
			node = tegrabl_add_subnode_if_absent(fdt, 0,
				common_nodes[i].node_name);
		} else {
			node = prev_offset;
		}

		if (node < 0)
			continue;

		prev_offset = node;
		prev_name = common_nodes[i].node_name;

		pr_debug("%d) node_name=%s\n", i, common_nodes[i].node_name);

		if (common_nodes[i].fill_dtnode) {
			status = common_nodes[i].fill_dtnode(fdt, node);
		    if (status != TEGRABL_NO_ERROR) {
				pr_error("%s: %p failed\n", __func__,
						 common_nodes[i].fill_dtnode);
				return status;
			}
		}
	}

	prev_name = NULL;
	if (tegrabl_linuxboot_helper_get_info(TEGRABL_LINUXBOOT_INFO_EXTRA_DT_NODES,
			NULL, &extra_nodes) == TEGRABL_NO_ERROR) {
		pr_debug("%s: extra_nodes: %p\n", __func__, extra_nodes);

		for (i = 0; extra_nodes[i].node_name != NULL; i++) {
			if (!prev_name || strcmp(prev_name, extra_nodes[i].node_name)) {
				node = tegrabl_add_subnode_if_absent(fdt, 0,
					extra_nodes[i].node_name);
			} else {
				node = prev_offset;
			}

			if (node < 0)
				continue;

			prev_offset = node;
			prev_name = extra_nodes[i].node_name;

			pr_debug("%d) node_name=%s\n", i, extra_nodes[i].node_name);

			if (extra_nodes[i].fill_dtnode) {
				status = extra_nodes[i].fill_dtnode(fdt, node);
				if (status != TEGRABL_NO_ERROR) {
					pr_error("%s: %p failed\n", __func__,
							 extra_nodes[i].fill_dtnode);
					return status;
				}
			}
		}
	}

	/* update secureos nodes present in kernel dtb */
	tegrabl_update_tos_nodes(fdt);

	/* add serial number as kernel DT property */
	tegrabl_add_serialno(fdt);

	pr_debug("%s: done\n", __func__);

	return TEGRABL_NO_ERROR;
}

