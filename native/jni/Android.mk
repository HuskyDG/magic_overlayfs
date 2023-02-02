LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := overlayfs_system
LOCAL_SRC_FILES := main.cpp
LOCAL_STATIC_LIBRARIES := libcxx libselinux
LOCAL_LDLIBS := -llog
include $(BUILD_EXECUTABLE)

include jni/external/Android.mk
include jni/libcxx/Android.mk
