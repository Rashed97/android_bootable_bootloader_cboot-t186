/*
 * Copyright (c) 2014-2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_BOOTIMAGE_H
#define INCLUDED_TEGRABL_BOOTIMAGE_H

#include <stdint.h>
#include <tegrabl_error.h>

#include "android_bootimg.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define ANDROID_HEADER_SIZE 2048

/**
 * Holds the boot.img (kernel + ramdisk) header.
 *
 * @param magic Holds the magic value used as a signature.
 * @param kernel_size Holds the size of kernel image.
 * @param kernel_addr Holds the load address of kernel.
 * @param ramdisk_size Holds the RAM disk (initrd) image size.
 * @param ramdisk_addr Holds the RAM disk (initrd) load address.
 * @param second_size Holds the secondary kernel image size.
 * @param second_addr Holds the secondary image address
 * @param tags_addr Holds the address for ATAGS.
 * @param page_size Holds the page size of the storage medium.
 * @param unused Unused field.
 * @param name Holds the project name, currently unused,
 * @param cmdline Holds the kernel command line to be appended to default
 *                command line.
 * @param id Holds the identifier, currently unused.
 * @param compression_algo Holds the decompression algorithm:
 * <pre>
 *                          0 = disable decompression
 *                          1 = ZLIB
 *                          2 = LZF
 * </pre>
 * @param crc_kernel Holds the store kernel checksum.
 * @param crc_ramdisk Holds the store RAM disk checksum.
 */

struct tegrabl_bootimg_header {
	uint8_t  magic[BOOT_MAGIC_SIZE];
	uint32_t kernel_size;
	uint32_t kernel_addr;

	uint32_t ramdisk_size;
	uint32_t ramdisk_addr;

	uint32_t second_size;
	uint32_t second_addr;

	uint32_t tags_addr;
	uint32_t page_size;

	uint32_t header_version;

	uint32_t os_version;

	uint8_t name[BOOT_NAME_SIZE];
	uint8_t cmdline[BOOT_ARGS_SIZE];

	uint32_t id[8];

	uint8_t extra_cmdline[BOOT_EXTRA_ARGS_SIZE];
};

#define CRC32_SIZE  (sizeof(uint32_t))

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_TEGRABL_BOOTIMAGE_H */

