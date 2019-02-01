/*
 * Copyright (c) 2015-2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_ODMDATA_LIB_H
#define INCLUDED_TEGRABL_ODMDATA_LIB_H

#include <tegrabl_error.h>

#define TEGRA_BOOTLOADER_LOCK_BIT	13

/**
 * @brief structure to hold the odmdata node properties under
 *		  corresponding mask and val conditions
 *
 * @param mask Mask the fields of current property
 * @param val Expected value to be met by property
 * @param name Name of the property to be added
 */
struct odmdata_params {
	uint32_t mask;
	uint32_t val;
	char *name;
};

/**
 * @brief Get odmdata from br-bct
 *
 * @return odmdata value
 */
uint32_t tegrabl_odmdata_get(void);

/**
 * @brief set odmdata on br-bct
 *
 * @param val Value to write to odmdata
 *
 * @return TEGREABL_NO_ERROR in case of success and
 * TEGRABL_ERR_INVALID if br-bct is not initialized
 */
tegrabl_error_t tegrabl_odmdata_set(uint32_t val);

/*
 * @brief get the odmdata_params structure and its size
 *
 * @param podmdata_list pointer to odmdata_params structure
 * @param odmdata_array_size size of odmdata_list array
 *
 * @return TEGRABL_NO_ERROR incase of success and
 * TEGRABL_ERR_INVALID in case if parameters passed are NULL
 */
tegrabl_error_t tegrabl_odmdata_params_get(
	struct odmdata_params **podmdata_list,
	uint32_t *odmdata_array_size);

/*
 * @brief determine if bootloader is locked or unlocked
 *
 * @return true if unlocked, and false if locked
 */
bool tegrabl_odmdata_is_device_unlocked(void);

#endif /* INCLUDED_TEGRABL_ODMDATA_LIB_H */
