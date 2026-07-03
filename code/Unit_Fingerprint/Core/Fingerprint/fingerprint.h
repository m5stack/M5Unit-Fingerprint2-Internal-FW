/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __FINGERPRINT_H
#define __FINGERPRINT_H

#ifdef __cplusplus

extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stdio.h"
#include "dma.h"
#include "usart.h"

#define FIRMWARE_VERSION            (0x01)
#define MAX_NO_FINGER_COUNT         (0x03)
#define OPERATION_SUCCESS           (0x00)
#define RECEIVE_PACKET_ERROR        (0x01)
#define FINGERPRINT_MODULE_BUSY     (0xFB)
#define OPERATION_BLOCKED           (0xFC)
#define ERROR_INVALID_PARAM         (0xFD)
#define FINGERPRINT_MODULE_NOT_OPEN (0xFE)
#define PACKET_HEADER_HIGH_BYTE     (0xEF)
#define PACKET_HEADER_LOW_BYTE      (0x01)
#define PACKET_MIN_LENGTH           (0x0C)

/**
 * @brief Enumeration for packet validation status.
 *
 * This enum represents different types of checks performed on a data packet.
 */
typedef enum {
    PACKET_CHECK_SUCCESS      = 0,  /**< Packet check passed successfully */
    PACKET_CHECK_CRC_FAIL     = 1,  /**< CRC check failed */
    PACKET_CHECK_LENGTH_FAIL  = 2,  /**< Packet length check failed */
    PACKET_CHECK_HEADER_FAIL  = 3,  /**< Packet header check failed */
    PACKET_CHECK_ADDRESS_FAIL = 4,  /**< Address validation failed */
    PACKET_CHECK_UNKNOWN_FAIL = 255 /**< Unknown packet check error */
} packet_check_status_t;            /**< @enum Packet validation status type */

/**
 * @brief Enumeration of command IDs for fingerprint MCU operations.
 */
typedef enum {
    CMD_SET_SLEEP_TIME                = 0xD0, /**< Command to set active sleep time */
    CMD_GET_SLEEP_TIME                = 0xD1, /**< Command to get active sleep time */
    CMD_SET_WORK_MODE                 = 0xD2, /**< Command to set work mode */
    CMD_GET_WORK_MODE                 = 0xD3, /**< Command to get work mode */
    CMD_ACTIVATE_FINGERPRINT_MODULE   = 0xD4, /**< Command to activate fingerprint module */
    CMD_GET_FINGERPRINT_MODULE_STATUS = 0xD5, /**< Command to get fingerprint module status */
    CMD_SAVE_CONFIGURATION_STATUS     = 0xD6, /**< Command to save configuration status */
    CMD_GET_FIRMWARE_VERSION          = 0xD7, /**< Command to get firmware version */
    CMD_IAP_RESET_STM32_MCU           = 0xFD  /**< Command to reset STM32 MCU in IAP mode */
} fingerprint_cmd_t;

uint16_t crc_sum(uint8_t *buffer, uint16_t size);
packet_check_status_t packet_check(uint8_t *buffer, uint16_t size);
void fingerprint_init(void);
void start_finger_check(void);
void open_fingerprint_mcu(void);
void close_fingerprint_mcu(void);
void set_fingerprint_sleep(void);
void fingerprint_enter_wait_mode(fingerprint_wait_event_t event);
void fingerprint_exit_wait_mode(void);
void set_exit_trigger_mode(fingerprint_irq_trigger_t status);
void send_sleep_packet(void);
void send_get_image_packet(void);
void send_passive_wakeup_packet(void);
void set_sleep_time_handle(uint8_t time);
void get_sleep_time_handle(void);
void set_work_mode_handle(uint8_t mode);
void get_work_mode_handle(void);
void activate_fingerprint_module_handle(void);
void get_fingerprint_module_status_handle(void);
void save_configuration_status_handle(uint8_t status);
void get_stm32_fw_ver(void);
void return_data_packet_handle(uint8_t type);
void iap_reset_stm32_mcu_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* __FINGERPRINT_H */
