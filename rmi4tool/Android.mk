LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := rmi4tool
LOCAL_C_INCLUDES := rmidevice
LOCAL_SRC_FILES := main.cpp
LOCAL_CPPFLAGS := -Wall
LOCAL_STATIC_LIBRARIES := rmidevice

include $(BUILD_EXECUTABLE)
