/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MCU_EN_Pin                GPIO_PIN_15
#define MCU_EN_GPIO_Port          GPIOC
#define TOUCH_DETECTION_Pin       GPIO_PIN_8
#define TOUCH_DETECTION_GPIO_Port GPIOA
#define TOUCH_DETECTION_EXTI_IRQn EXTI4_15_IRQn

/* USER CODE BEGIN Private defines */
//#define DEBUG       (1)    // Enable or disable debug output
//#define BUFFER_SIZE (512)  // Buffer size for data storage, 512 bytes

#define DEBUG       (0)    // Enable or disable debug output
#define BUFFER_SIZE (1024)  // Buffer size for data storage, 1024 bytes

// Define the GPIO level states indicating fingerprint MCU power status
typedef enum {
    FINGERPRINT_MCU_POWER_OFF = 0,  // GPIO is low, fingerprint MCU power is off
    FINGERPRINT_MCU_POWER_ON  = 1   // GPIO is high, fingerprint MCU power is on
} fingerprint_mcu_power_state_t;

// Define the initialization status of the fingerprint MCU
typedef enum {
    FINGERPRINT_MCU_INIT_INCOMPLETE = 0x00,  // MCU initialization is not yet complete
    FINGERPRINT_MCU_INIT_COMPLETE   = 0x01   // MCU initialization has been completed
} fingerprint_mcu_init_t;

// Define the possible states for update_addr_flag
typedef enum {
    UPDATE_ADDR_NONE   = 0,  // 0: Do not update
    UPDATE_ADDR_UPDATE = 1,  // 1: Update
    UPDATE_ADDR_CHECK  = 2   // 2: Check if memory address matches, if not, save it to memory
} update_addr_flag_t;

// Enumeration of fingerprint sensor sleep status
typedef enum {
    FINGERPRINT_SENSOR_AWAKE = 0,  // Fingerprint sensor is active (not in sleep mode)
    FINGERPRINT_SENSOR_SLEEP = 1   // Fingerprint sensor is in sleep mode
} fingerprint_sensor_sleep_t;

// Define the data forwarding status for fingerprint module
typedef enum {
    FINGERPRINT_DATA_FORWARD_DISABLED = 0,  // Data forwarding is disabled
    FINGERPRINT_DATA_FORWARD_ENABLED  = 1   // Data forwarding is enabled
} fingerprint_data_forward_flag_t;

// Enumeration of fingerprint module wait events
typedef enum {
    WAIT_EVENT_NONE         = 0,  // No event is being waited on
    WAIT_EVENT_SLEEP_RETURN = 1,  // Waiting for sleep event to return
    WAIT_EVENT_IMAGE_RETURN = 2   // Waiting for image capture event to return
} fingerprint_wait_event_t;

// Define the interrupt trigger types for the fingerprint module
typedef enum {
    INTERRUPT_TRIGGER_NONE   = 0,  // No trigger detected
    INTERRUPT_TRIGGER_RESET  = 1,  // Triggered by reset event
    INTERRUPT_TRIGGER_WAKEUP = 2   // Triggered by wake-up event
} fingerprint_irq_trigger_t;

// Define the timeout status of a timer
typedef enum {
    TIMER_TIMEOUT_NOT_OCCURRED = 0,  // Timer has not timed out
    TIMER_TIMEOUT_OCCURRED     = 1   // Timer has timed out
} timer_timeout_status_t;

// Defines the working mode of the device.
typedef enum {
    WORK_MODE_SLEEP_PERIODIC = 0x00, /**< Periodic sleep mode: the device enters sleep after intervals */
    WORK_MODE_ALWAYS_ON      = 0x01, /**< Always-on mode: the device remains active continuously */
} work_mode_t;

// Indicates whether the fingerprint module has returned from busy state.
typedef enum {
    FINGERPRINT_BUSY_NOT_RETURNED = 0, /**< Module has not returned from busy */
    FINGERPRINT_BUSY_RETURNED     = 1, /**< Module has returned from busy */
} fingerprint_busy_return_status_t;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
