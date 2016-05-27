# Copyright (C) 2011 The Android Open Source Project
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


# function to find all *.cpp files under a directory
define all-cpp-files-under
$(patsubst ./%,%, \
  $(shell cd $(LOCAL_PATH) ; \
          find $(1) -name "*.cpp" -and -not -name ".*") \
 )
endef


HAL_SUFFIX := $(TARGET_DEVICE)
ifeq ($(TARGET_DEVICE),crespo)
	HAL_SUFFIX := herring
endif

LOCAL_ARM_MODE := arm
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libese-spi
LOCAL_ARM_MODE := ar
#LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := both
LOCAL_SRC_FILES := -Wall $(call all-c-files-under, .)  $(call all-cpp-files-under, .)

LOCAL_ANDROID_M := TRUE
ifeq ($(LOCAL_ANDROID_M),FALSE)
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware_legacy libdl libstlport
LOCAL_C_INCLUDES += \
        external/stlport/stlport  bionic/  bionic/libstdc++/include \
        $(LOCAL_PATH)/utils \
        $(LOCAL_PATH)/inc \
        $(LOCAL_PATH)/common \
        $(LOCAL_PATH)/hal \
        $(LOCAL_PATH)/log \
        $(LOCAL_PATH)/tml \
        $(LOCAL_PATH)/spm \

else
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware_legacy libdl
LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/utils \
        $(LOCAL_PATH)/inc \
        $(LOCAL_PATH)/common \
        $(LOCAL_PATH)/hal \
        $(LOCAL_PATH)/log \
        $(LOCAL_PATH)/tml \
        $(LOCAL_PATH)/spm \

endif

LOCAL_CFLAGS += -Wall -Wextra
LOCAL_CFLAGS += -DANDROID \

#### Select the JCOP OS Version ####
JCOP_VER_3_1 := 1
JCOP_VER_3_2 := 2
JCOP_VER_3_3 := 3
LOCAL_CFLAGS += -DJCOP_VER_3_1=$(JCOP_VER_3_1)
LOCAL_CFLAGS += -DJCOP_VER_3_2=$(JCOP_VER_3_2)
LOCAL_CFLAGS += -DJCOP_VER_3_3=$(JCOP_VER_3_3)
LOCAL_CFLAGS += -DNFC_NXP_ESE_VER=$(JCOP_VER_3_3)
include $(BUILD_SHARED_LIBRARY)
