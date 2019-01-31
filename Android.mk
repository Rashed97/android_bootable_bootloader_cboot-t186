LOCAL_PATH := $(call my-dir)

NVIDIA_DEFAULTS := $(CLEAR_VARS)

CBOOT_DIR := $(LOCAL_PATH)/bootloader/partner/t18x/cboot
_cboot_project := $(TARGET_TEGRA_VERSION)

include $(CBOOT_DIR)/cboot.mk
