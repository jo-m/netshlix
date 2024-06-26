#pragma once

#ifdef ESP_PLATFORM
#include <esp_err.h>
#include <esp_log.h>
#else

#include <stdio.h>

/**
 * Minimal dummy header to run code using some ESP-IDF features on Linux.
 */

typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE,
} esp_log_level_t;

#ifdef NDEBUG
#define ESP_LOGE(tag, format, ...)
#define ESP_LOGW(tag, format, ...)
#define ESP_LOGI(tag, format, ...)
#define ESP_LOGD(tag, format, ...)
#define ESP_LOGV(tag, format, ...)
#else
#define ESP_LOGE(tag, format, ...) printf("E[%s]\t" format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("W[%s]\t" format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) printf("I[%s]\t" format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) printf("D[%s]\t" format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) printf("V[%s]\t" format "\n", tag, ##__VA_ARGS__)
#endif

#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_INFO

typedef int esp_err_t;

/* Definitions for error constants. */
#define ESP_OK 0    /*!< esp_err_t value indicating success (no error) */
#define ESP_FAIL -1 /*!< Generic esp_err_t code indicating failure */

#define ESP_ERR_NO_MEM 0x101           /*!< Out of memory */
#define ESP_ERR_INVALID_ARG 0x102      /*!< Invalid argument */
#define ESP_ERR_INVALID_STATE 0x103    /*!< Invalid state */
#define ESP_ERR_INVALID_SIZE 0x104     /*!< Invalid size */
#define ESP_ERR_NOT_FOUND 0x105        /*!< Requested resource not found */
#define ESP_ERR_NOT_SUPPORTED 0x106    /*!< Operation or feature not supported */
#define ESP_ERR_TIMEOUT 0x107          /*!< Operation timed out */
#define ESP_ERR_INVALID_RESPONSE 0x108 /*!< Received response was invalid */
#define ESP_ERR_INVALID_CRC 0x109      /*!< CRC or checksum was invalid */
#define ESP_ERR_INVALID_VERSION 0x10A  /*!< Version was invalid */
#define ESP_ERR_INVALID_MAC 0x10B      /*!< MAC address was invalid */
#define ESP_ERR_NOT_FINISHED 0x10C     /*!< Operation has not fully completed */
#define ESP_ERR_NOT_ALLOWED 0x10D      /*!< Operation is not allowed */

#endif
