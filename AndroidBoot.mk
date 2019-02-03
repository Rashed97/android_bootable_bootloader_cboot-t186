#
# Copyright (C) 2019 Team Codefire
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
# Android makefile to build bootloader as a part of Android Build
#
# Configuration
# =============
#
# These config vars are usually set in BoardConfig.mk:
#   TARGET_BOOTLOADER_PLATFORM             = Bootloader platform
#   TARGET_BOOTLOADER_PROJECT              = Bootloader project
#

BOOTLOADER_PLATFORM := $(TARGET_BOOTLOADER_PLATFORM)
BOOTLOADER_PROJECT := $(TARGET_BOOTLOADER_PROJECT)
BOOTLOADER_SUPERPROJECT := $(shell echo $(BOOTLOADER_PROJECT) | sed -e "s/.$$/x/")

CBOOT_SRC := bootable/bootloader/cboot-t186
CBOOT_OUT := $(TARGET_OUT_INTERMEDIATES)/CBOOT_OBJ

BOOTLOADER_TOOLCHAIN_ARCH := aarch64
BOOTLOADER_TOOLCHAIN_PREFIX := aarch64-linux-android-
BOOTLOADER_TOOLCHAIN_VERSION := 4.9

BOOTLOADER_TOOLCHAIN_PATH := $(shell pwd)/prebuilts/gcc/$(HOST_OS)-x86/$(BOOTLOADER_TOOLCHAIN_ARCH)/$(BOOTLOADER_TOOLCHAIN_PREFIX)$(BOOTLOADER_TOOLCHAIN_VERSION)/bin/$(BOOTLOADER_TOOLCHAIN_PREFIX)

ifneq ($(USE_CCACHE),)
    # Detect if the system already has ccache installed to use instead of the prebuilt
    ccache := $(shell which ccache)

    ifeq ($(ccache),)
        ccache := $(shell pwd)/prebuilts/misc/$(HOST_PREBUILT_TAG)/ccache/ccache
       	# Check that the executable is here.
        ccache := $(strip $(wildcard $(ccache)))
    endif
endif

BOOTLOADER_CROSS_COMPILE := CROSS_COMPILE="$(ccache) $(BOOTLOADER_TOOLCHAIN_PATH)"
ccache =

CBOOT_CLEAN:
	$(hide) rm -f $(TARGET_CBOOT_BIN)
	$(hide) rm -rf $(CBOOT_OUT)

$(CBOOT_OUT):
	mkdir -p $(CBOOT_OUT)

TARGET_CBOOT_BIN := $(CBOOT_OUT)/build-$(BOOTLOADER_PROJECT)/lk.bin
$(TARGET_CBOOT_BIN):
	$(MAKE) -C $(CBOOT_SRC)/bootloader/partner/$(BOOTLOADER_SUPERPROJECT)/cboot PROJECT=$(BOOTLOADER_PROJECT) \
	    TOOLCHAIN_PREFIX=$(BOOTLOADER_CROSS_COMPILE) DEBUG=2 BUILDROOT=$(CBOOT_OUT) NV_BUILD_SYSTEM_TYPE=l4t NOECHO=@

INSTALLED_CBOOT_TARGET := $(PRODUCT_OUT)/cboot.bin
$(INSTALLED_CBOOT_TARGET) : $(TARGET_CBOOT_BIN)
	$(transform-prebuilt-to-target)

.PHONY: cbootimage
cbootimage: $(INSTALLED_CBOOT_TARGET)
