/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __MYFLASH_H
#define __MYFLASH_H

#ifdef __cplusplus

extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdbool.h>
#include "stm32g0xx_hal_flash_ex.h"

#define STM32G0xx_PAGE_SIZE              (0x800)
#define STM32G0xx_FLASH_PAGE0_STARTADDR  (0x8000000)
#define STM32G0xx_FLASH_PAGE29_STARTADDR (STM32G0xx_FLASH_PAGE0_STARTADDR + 29 * STM32G0xx_PAGE_SIZE)
#define STM32G0xx_FLASH_PAGE30_STARTADDR (STM32G0xx_FLASH_PAGE0_STARTADDR + 30 * STM32G0xx_PAGE_SIZE)
#define SLEEP_TIME_ADDR                  (STM32G0xx_FLASH_PAGE29_STARTADDR + 0)
#define WORK_MODE_ADDR                   (STM32G0xx_FLASH_PAGE30_STARTADDR + 0)

uint8_t get_sleep_time(void);
bool set_sleep_time(uint8_t sleep_time);

uint8_t get_work_mode(void);
bool set_work_mode(uint8_t work_mode);

#ifdef __cplusplus
}
#endif

#endif /* __MYFLASH_H */
