#pragma once
#include <android/log.h>
#include <errno.h>
#define LOG_TAG "OverlayFS"

extern int log_fd;

#define write_log(PRIO, TAG, ...) \
    { \
      char logbuffer[4098]; \
      __android_log_print(PRIO, TAG, __VA_ARGS__); \
      snprintf(logbuffer, sizeof(logbuffer)-1, __VA_ARGS__); \
      log_to_file(log_fd, PRIO, logbuffer); \
    }

#define LOGD(...) write_log(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGI(...) write_log(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) write_log(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) write_log(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define PLOGE(fmt, args...) LOGE(fmt " failed with %d: %s\n", ##args, errno, std::strerror(errno))

void log_to_file(int fd, int prio, const char *log);
