# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	rilproxy.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libsocketproxy \
#	librilutils \

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB
#LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_MODULE:= rilproxyd
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	socketproxy.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
#	librilutils

LOCAL_MODULE:= libsocketproxy
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)



include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	riloemproxy.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libsocketproxy \
#	librilutils

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB
#LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_MODULE:= riloemproxyd
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

################################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	rilimsproxy.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libsocketproxy \
#	librilutils

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB
#LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_MODULE:= rilimsproxyd
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

#############################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	rilqrcproxy.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libsocketproxy \
#	librilutils

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB
#LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_MODULE:= rilqrcproxyd
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

###########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	rilulproxy.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libsocketproxy \
#	librilutils

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB
#LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_MODULE:= rilulproxyd
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
###########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	rilurcproxy.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libsocketproxy \
#	librilutils

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB
#LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_MODULE:= rilurcproxyd
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
###########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	rilursproxy.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libsocketproxy \
#	librilutils

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB
#LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_MODULE:= rilursproxyd
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
###########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	ril_debug.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libsocketproxy \
#	librilutils

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB
#LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_MODULE:= ril_debug
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)