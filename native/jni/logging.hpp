#pragma once
#include <errno.h>
#define LOG_TAG "OverlayFS"

extern int log_fd;

#define write_log(PRIO, TAG, ...) \
    { \
      char logbuffer[4098]; \
      snprintf(logbuffer, sizeof(logbuffer)-1, __VA_ARGS__); \
      log_to_file(log_fd, PRIO, logbuffer); \
    }

#define LOGD(...) write_log(1, LOG_TAG, __VA_ARGS__)
#define LOGI(...) write_log(0, LOG_TAG, __VA_ARGS__)
#define LOGW(...) write_log(2, LOG_TAG, __VA_ARGS__)
#define LOGE(...) write_log(3, LOG_TAG, __VA_ARGS__)
#define PLOGE(fmt, args...) LOGE(fmt " failed with %d: %s\n", ##args, errno, std::strerror(errno))

void log_to_file(int fd, int prio, const char *log);
