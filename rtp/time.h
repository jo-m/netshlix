
#pragma once

#include <assert.h>
#include <stdint.h>

static_assert(sizeof(long) == sizeof(int64_t), "Well, it's not");

#define US_PER_SEC (1000000L)
#define NS_PER_US (1000L)

#define SEC_TO_US(sec) ((sec)*US_PER_SEC)
#define NS_TO_US(ns) ((ns) / NS_PER_US)

int64_t micros();
