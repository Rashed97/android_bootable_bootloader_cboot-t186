/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_LINUXBOOT

#include "build_config.h"
#include <stdint.h>
#include <string.h>
#include <libfdt.h>
#include <inttypes.h>
#include <tegrabl_utils.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_bootimg.h>
#include <tegrabl_linuxboot.h>
#include <tegrabl_sdram_usage.h>
#include <linux_load.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_partition_loader.h>
#include <tegrabl_decompress.h>
#include <tegrabl_malloc.h>
#include <dtb_overlay.h>

static uint64_t ramdisk_load;
static uint64_t ramdisk_size;
static uint64_t vendor_ramdisk_size;
static char bootimg_cmdline[BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE + VENDOR_BOOT_ARGS_SIZE + 1];

/* maximum possible uncompressed kernel image size--60M */
#define MAX_KERNEL_IMAGE_SIZE (1024 * 1024 * 60)

/* device tree overlay size--1M */
#define KERNEL_DTBO_PART_SIZE	(1024 * 1024 * 1)

#define FDT_SIZE_BL_DT_NODES (4048 + 4048)

void tegrabl_get_ramdisk_info(uint64_t *start, uint64_t *size)
{
	if (start) {
		*start = ramdisk_load;
	}
	if (size) {
#if CONFIG_BOOTIMG_HEADER_VERSION >= 3
		*size = ROUND_UP_POW2(ramdisk_size, vndhdr->page_size) + vendor_ramdisk_size;
#else
		*size = ramdisk_size;
#endif
	}
}

char *tegrabl_get_bootimg_cmdline(void)
{
	return bootimg_cmdline;
}

/* Sanity checks the Android boot image */
static tegrabl_error_t validate_boot_image(tegrabl_bootimg_header *hdr,
									   uint32_t *hdr_crc)
{
	uint32_t known_crc = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_info("Checking boot.img header magic...\n");
	/* Check header magic */
	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		pr_error("Invalid boot.img @ %p (header magic mismatch)\n", hdr);
		err = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
		goto fail;
	}
	pr_info("Valid boot.img @ %p\n", hdr);

fail:
	*hdr_crc = known_crc;
	return err;
}

/* Sanity checks the Android vendor_boot image */
static tegrabl_error_t validate_vendor_boot_image(tegrabl_vendor_bootimg_header *vndhdr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_info("Checking vendor_boot.img header magic...\n");
	/* Check header magic */
	if (memcmp(vndhdr->magic, VENDOR_BOOT_MAGIC, VENDOR_BOOT_MAGIC_SIZE)) {
		pr_error("Invalid vendor_boot.img @ %p (header magic mismatch)\n", vndhdr);
		err = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
		goto fail;
	}
	pr_info("Valid vendor_boot.img @ %p\n", vndhdr);

fail:
	return err;
}

/* Extract kernel from an Android boot image, and return the address where it
 * is installed in memory
 */
static tegrabl_error_t extract_kernel(tegrabl_bootimg_header *hdr,
								   tegrabl_vendor_bootimg_header *vndhdr,
								   void **kernel_entry_point)
{
	void *kernel_load = NULL;
	uint64_t kernel_offset = 0; /* Offset of 1st kernel byte in boot.img */
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t hdr_crc = 0;
	bool is_compressed = false;
	decompressor *decomp = NULL;
	uint32_t decomp_size = 0; /* kernel size after decompressing */

	kernel_offset = vndhdr->page_size;

	err = validate_boot_image(hdr, &hdr_crc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u failed to validate boot.img\n", err);
		return err;
	}
	err = validate_vendor_boot_image(vndhdr);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u failed to validate vendor_boot.img\n", err);
		return err;
	}

	if (hdr_crc)
		kernel_load = (char *)0x80000000 + vndhdr->kernel_addr;
	else
		kernel_load = (char *)LINUX_LOAD_ADDRESS;

	is_compressed = is_compressed_content((uint8_t *)hdr + kernel_offset,
										  &decomp);

	if (!is_compressed) {
		pr_info("Copying kernel image (%u bytes) from %p to %p ... ",
				hdr->kernel_size, (char *)hdr + kernel_offset, kernel_load);
		memmove(kernel_load, (char *)hdr + kernel_offset, hdr->kernel_size);
	} else {
		pr_info("Decompressing kernel image (%u bytes) from %p to %p ... ",
				hdr->kernel_size, (char *)hdr + kernel_offset, kernel_load);

		decomp_size = MAX_KERNEL_IMAGE_SIZE;
		err = do_decompress(decomp, (uint8_t *)hdr + kernel_offset,
							hdr->kernel_size, kernel_load, &decomp_size);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("\nError %d decompress kernel\n", err);
			return err;
		}
	}

	pr_info("Done\n");

	*kernel_entry_point = kernel_load;

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t extract_ramdisk(tegrabl_bootimg_header *hdr,
								   tegrabl_vendor_bootimg_header *vndhdr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint64_t ramdisk_offset = (uint64_t)NULL; /* Offset of 1st ramdisk byte in boot.img */

	ramdisk_offset = ROUND_UP_POW2(vndhdr->page_size + hdr->kernel_size,
								   vndhdr->page_size);
	ramdisk_offset = (uintptr_t)hdr + ramdisk_offset;
	ramdisk_load = RAMDISK_ADDRESS;
	ramdisk_size = hdr->ramdisk_size;
	if (ramdisk_offset != ramdisk_load) {
		pr_info("Move boot.img ramdisk (len: %"PRIu64") from 0x%"PRIx64" to 0x%"PRIx64
				"\n", ramdisk_size, ramdisk_offset, ramdisk_load);
		memmove((void *)((uintptr_t)ramdisk_load),
				(void *)((uintptr_t)ramdisk_offset), ramdisk_size);
	}
#if CONFIG_BOOTIMG_HEADER_VERSION >= 3
	uint64_t vendor_ramdisk_offset = (uint64_t)NULL; /* Offset of 1st ramdisk byte in vendor_boot.img */
	/*
	  TODO: Determine where 2112 came from:
	  https://android.googlesource.com/platform/system/tools/mkbootimg/+/master/include/bootimg/bootimg.h#193
	  Google lists the vendor_boot.img header size as 2112, likely to change
	 */
	vendor_ramdisk_offset = ROUND_UP_POW2(2112 + vndhdr->page_size - 1 ,
								   vndhdr->page_size);
	vendor_ramdisk_offset = (uintptr_t)vndhdr + vendor_ramdisk_offset;
	vendor_ramdisk_size = vndhdr->vendor_ramdisk_size;
	uint64_t vendor_ramdisk_start = ROUND_UP_POW2(ramdisk_load + ramdisk_size, vndhdr->page_size);
	/* Move the vendor ramdisk to right after the generic ramdisk */
	pr_info("Move vendor_boot.img ramdisk (len: %"PRIu64") from 0x%"PRIx64" to 0x%"PRIx64"\n",
			vendor_ramdisk_size, vendor_ramdisk_offset, vendor_ramdisk_start);
	memmove((void *)((uintptr_t)vendor_ramdisk_start),
			(void *)((uintptr_t)vendor_ramdisk_offset), vendor_ramdisk_size);
#endif /* CONFIG_BOOTIMG_HEADER_VERSION >= 3 */

#if CONFIG_BOOTIMG_HEADER_VERSION >= 3
	strncpy(bootimg_cmdline, (char *)hdr->cmdline, BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE);
	strncat(bootimg_cmdline, (char *)vndhdr->cmdline, VENDOR_BOOT_ARGS_SIZE);
#else
	strncpy(bootimg_cmdline, (char *)hdr->cmdline, BOOT_ARGS_SIZE);
	// TODO: Maybe append hdr->extra_cmdline too?
#endif
	pr_info("Loaded cmdline from bootimage: %s\n", bootimg_cmdline);

	return err;
}

#if !defined(CONFIG_DT_SUPPORT)
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb, void *kernel_dtbo)
{
	return TEGRABL_NO_ERROR;
}
#else
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb, void *kernel_dtbo)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_dt_create_space(*kernel_dtb, FDT_SIZE_BL_DT_NODES, DTB_MAX_SIZE);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Save dtb handle */
	err = tegrabl_dt_set_fdt_handle(TEGRABL_DT_KERNEL, *kernel_dtb);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_linuxboot_update_dtb(*kernel_dtb);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("kernel-dtbo @ %p\n", kernel_dtbo);
#if defined(CONFIG_ENABLE_DTB_OVERLAY)
	err = tegrabl_dtb_overlay(kernel_dtb, kernel_dtbo);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Booting with default kernel-dtb!!!\n");
		err = TEGRABL_NO_ERROR;
	}
#endif

fail:
	return err;
}
#endif /* end of CONFIG_DT_SUPPORT */

void tegrabl_store_vendor_bootimg_values(tegrabl_vendor_bootimg_header *vndhdr)
{
	bootimg_page_size = vndhdr->page_size;
	pr_info("Kernel page size set to %d\n", bootimg_page_size);
	return;
}

tegrabl_error_t tegrabl_load_kernel_and_dtb(
			struct tegrabl_kernel_bin *kernel,
			void **kernel_entry_point,
			void **kernel_dtb,
			struct tegrabl_kernel_load_callbacks *callbacks,
			void *data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *kernel_dtbo = NULL;
	tegrabl_bootimg_header *hdr = (void *)((uintptr_t)0xDEADDEA0);
	tegrabl_vendor_bootimg_header *vndhdr = (void *)((uintptr_t)0xDEADDEA0);

	if (!kernel_entry_point || !kernel_dtb) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (!kernel->load_from_storage) {
		pr_info("Loading kernel/boot.img from memory ...\n");
		if (!data) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
			pr_error("Found no kernel in memory\n");
			goto fail;
		}
		hdr = data;
		// TODO: Add handling for vendor_boot.img here
		goto load_dtb;
	}

	pr_info("Loading kernel/boot.img/vendor_boot.img from storage ...\n");
#if !defined(CONFIG_BACKDOOR_LOAD)
#if CONFIG_BOOTIMG_HEADER_VERSION >= 3
	/* vendor_boot needs to be loaded before boot so we can use the values from it's
	   header to load the kernel and ramdisk from boot. */
	err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL_VENDOR, (void **)vndhdr, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("vendor_boot.img loading failed\n");
		goto fail;
	}
	/* Store values from the vendor_boot image header in global variables
	   to allow other functions to read them */
	tegrabl_store_vendor_bootimg_values(vndhdr);
#endif /* CONFIG_BOOTIMG_HEADER_VERSION */
	err = tegrabl_load_binary(kernel->bin_type, (void **)&hdr, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("boot.img loading failed\n");
		goto fail;
	}
#if CONFIG_BOOTIMG_HEADER_VERSION < 3
	/* When vendor_boot doesn't exist, set vndhdr equal to hdr after normal
	   boot.img has already loaded. */
	vndhdr = hdr;
#endif /* CONFIG_BOOTIMG_HEADER_VERSION */
#else
	hdr = (void *)((uintptr_t)BOOT_IMAGE_LOAD_ADDRESS);
	vndhdr = (void *)((uintptr_t)VENDOR_BOOT_IMAGE_LOAD_ADDRESS);
#endif

load_dtb:
	/* Load kernel_dtb early since it is used in verified boot */
#if defined(CONFIG_DT_SUPPORT)
#if !defined(CONFIG_BACKDOOR_LOAD)
	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_KERNEL, kernel_dtb);
	/* Load kernel_dtb if not already loaded in memory */
	if ((err != TEGRABL_NO_ERROR) || (*kernel_dtb == NULL)) {
		err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL_DTB, kernel_dtb, NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Kernel-dtb loading failed\n");
			goto fail;
		}
	}
#if defined(CONFIG_ENABLE_DTB_OVERLAY)
	/* kernel_dtbo should also be protected by verified boot */
	kernel_dtbo = tegrabl_malloc(KERNEL_DTBO_PART_SIZE);
	if (!kernel_dtbo) {
		pr_error("Failed to allocate memory\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		return err;
	}

	err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL_DTBO, kernel_dtbo, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading kernel-dtbo\n", err);
		goto fail;
	}
	pr_info("Kernel DTBO @ %p\n", kernel_dtbo);
#endif /* CONFIG_ENABLE_DTB_OVERLAY */
#else
	*kernel_dtb = (void *)((uintptr_t)DTB_LOAD_ADDRESS);
#endif /* CONFIG_BACKDOOR_LOAD */
#else
	*kernel_dtb = NULL;
#endif /* CONFIG_DT_SUPPORT */

	pr_info("Kernel DTB @ %p\n", *kernel_dtb);

	if (callbacks != NULL && callbacks->verify_boot != NULL)
		callbacks->verify_boot(hdr, vndhdr, *kernel_dtb, kernel_dtbo);

	err = extract_kernel(hdr, vndhdr, kernel_entry_point);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the kernel\n", err);
		goto fail;
	}

	err = extract_ramdisk(hdr, vndhdr);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the ramdisk\n", err);
		goto fail;
	}

	err = extract_kernel_dtb(kernel_dtb, kernel_dtbo);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the kernel DTB\n", err);
		goto fail;
	}

	pr_info("%s: Done\n", __func__);

fail:
	tegrabl_free(kernel_dtbo);
	return err;
}
