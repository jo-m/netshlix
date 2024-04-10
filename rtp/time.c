#define _POSIX_C_SOURCE 200112L
#include "time.h"

#include <stdint.h>
#include <time.h>

uint64_t micros() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
    return us;
}
