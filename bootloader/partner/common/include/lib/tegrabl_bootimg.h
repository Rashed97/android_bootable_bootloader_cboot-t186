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

#if CONFIG_BOOTIMG_HEADER_VERSION >= 3
// Starting from bootimg header v3, pagesize is forced to be 4096 bytes
#define ANDROID_HEADER_SIZE 4096
#else
#define ANDROID_HEADER_SIZE 2048
#endif

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

#if CONFIG_BOOTIMG_HEADER_VERSION == 3
typedef struct boot_img_hdr_v3 tegrabl_bootimg_header;
#elif CONFIG_BOOTIMG_HEADER_VERSION == 2
typedef struct boot_img_hdr_v2 tegrabl_bootimg_header;
#elif CONFIG_BOOTIMG_HEADER_VERSION == 1
typedef struct boot_img_hdr_v1 tegrabl_bootimg_header;
#else // CONFIG_BOOTIMG_HEADER_VERSION=0 will fall back to this
typedef struct boot_img_hdr_v0 tegrabl_bootimg_header;
#endif

#if CONFIG_BOOTIMG_HEADER_VERSION == 3
// Set the vendor bootimg struct only if header version == 3
// otherwise fall back to bootimg structs
typedef struct vendor_boot_img_hdr_v3 tegrabl_vendor_bootimg_header;
#elif CONFIG_BOOTIMG_HEADER_VERSION == 2
typedef struct boot_img_hdr_v2 tegrabl_vendor_bootimg_header;
#elif CONFIG_BOOTIMG_HEADER_VERSION == 1
typedef struct boot_img_hdr_v1 tegrabl_vendor_bootimg_header;
#else // CONFIG_BOOTIMG_HEADER_VERSION=0 will fall back to this
typedef struct boot_img_hdr_v0 tegrabl_vendor_bootimg_header;
#endif

// Set this to the default 4096 page size, override in linux_load if different
extern uint32_t bootimg_page_size;

#define CRC32_SIZE  (sizeof(uint32_t))

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_TEGRABL_BOOTIMAGE_H */

