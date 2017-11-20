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
LOCAL_CFLAGS += -DANDROID \

# Set ESE_DEBUG_UTILS_INCLUDED TRUE to enable debug utils
ESE_DEBUG_UTILS_INCLUDED := TRUE
ifeq ($(ESE_DEBUG_UTILS_INCLUDED), TRUE)
LOCAL_CFLAGS += -Wall -Wextra -DESE_DEBUG_UTILS_INCLUDED
LOCAL_SRC_FILES := /utils/phNxpConfig.cpp
else
LOCAL_CFLAGS += -Wall -Wextra
endif

LOCAL_SRC_FILES += \
    /log/phNxpLog.c \
    /spm/phNxpEse_Spm.c \
    /lib/phNxpEseProto7816_3.c \
    /lib/phNxpEse_Apdu_Api.c \
    /lib/phNxpEseDataMgr.c \
    /lib/phNxpEse_Api.c \
    /pal/phNxpEsePal.c \
    /pal/spi/phNxpEsePal_spi.c

ANDROID_VER := $(subst ., , $(PLATFORM_VERSION))
ANDROID_VER := $(word 1, $(ANDROID_VER))
LOCAL_SHARED_LIBSTL := TRUE
ifeq ($(shell expr $(ANDROID_VER) \>= 6), 1)
LOCAL_SHARED_LIBSTL := FALSE
endif

ifeq ($(LOCAL_SHARED_LIBSTL),TRUE)
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware_legacy libdl libstlport
LOCAL_C_INCLUDES += \
        external/stlport/stlport  bionic/  bionic/libstdc++/include \
        $(LOCAL_PATH)/utils \
	$(LOCAL_PATH)/inc \
	$(LOCAL_PATH)/common \
	$(LOCAL_PATH)/lib \
	$(LOCAL_PATH)/log \
	$(LOCAL_PATH)/pal/spi \
	$(LOCAL_PATH)/pal \
	$(LOCAL_PATH)/../common/include \

else
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware_legacy libdl
LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/utils \
	$(LOCAL_PATH)/inc \
	$(LOCAL_PATH)/common \
	$(LOCAL_PATH)/lib \
	$(LOCAL_PATH)/log \
	$(LOCAL_PATH)/pal/spi \
	$(LOCAL_PATH)/pal \
	$(LOCAL_PATH)/../common/include \

endif

#### Select the JCOP OS Version ####
JCOP_VER_3_1 := 1
JCOP_VER_3_2 := 2
JCOP_VER_3_3 := 3
JCOP_VER_4_0 := 4
JCOP_VER_4_0_1 := 5
LOCAL_CFLAGS += -DJCOP_VER_3_1=$(JCOP_VER_3_1)
LOCAL_CFLAGS += -DJCOP_VER_3_2=$(JCOP_VER_3_2)
LOCAL_CFLAGS += -DJCOP_VER_3_3=$(JCOP_VER_3_3)
LOCAL_CFLAGS += -DJCOP_VER_4_0=$(JCOP_VER_4_0)
LOCAL_CFLAGS += -DJCOP_VER_4_0_1=$(JCOP_VER_4_0_1)
LOCAL_CFLAGS += -DNFC_NXP_ESE_VER=$(JCOP_VER_4_0_1)

#variables for NFC_NXP_CHIP_TYPE
PN65T := 1
PN66T := 2
PN67T := 3
PN80T := 4
PN81A := 5

ifeq ($(PN65T),1)
LOCAL_CFLAGS += -DPN65T=1
endif
ifeq ($(PN66T),2)
LOCAL_CFLAGS += -DPN66T=2
endif
ifeq ($(PN67T),3)
LOCAL_CFLAGS += -DPN67T=3
endif
ifeq ($(PN80T),4)
LOCAL_CFLAGS += -DPN80T=4
endif
ifeq ($(PN81A),5)
LOCAL_CFLAGS += -DPN81A=5
endif
#### Select the CHIP ####
NXP_CHIP_TYPE := $(PN81A)

ifeq ($(NXP_CHIP_TYPE),$(PN65T))
LOCAL_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN65T
else ifeq ($(NXP_CHIP_TYPE),$(PN66T))
LOCAL_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN66T
else ifeq ($(NXP_CHIP_TYPE),$(PN67T))
LOCAL_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN67T
else ifeq ($(NXP_CHIP_TYPE),$(PN80T))
LOCAL_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN80T
else ifeq ($(NXP_CHIP_TYPE),$(PN81A))
LOCAL_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN81A
endif

include $(BUILD_SHARED_LIBRARY)
