#
# Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)
MODULE_DEPS += \
	$(LOCAL_DIR)/../dpaux

GLOBAL_INCLUDES += \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/../../include/soc/$(TARGET)

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_i2c.c \
	$(LOCAL_DIR)/tegrabl_i2c_bpmpfw.c

include make/module.mk
