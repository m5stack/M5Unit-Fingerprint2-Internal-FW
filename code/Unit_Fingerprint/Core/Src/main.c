/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdbool.h"
#include "stdio.h"
#include "myflash.h"
#include "fingerprint.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APPLICATION_ADDRESS (0x08002000)
#define SLEEP_TIME_BASE     (10)
#define WORK_MODE_BASE      (0)
#define INVALID_VALUE       (0xFF)

#define PACKET_MAX_SIZE 16  // Maximum packet size

typedef struct {
    uint8_t data[PACKET_MAX_SIZE];  // Data packet buffer
    uint16_t size;                  // Data packet size
    uint8_t cmd;                    // Command byte
    uint8_t valid;                  // Valid flag (1: valid, 0: invalid)
} buffer_cache_cmd_t;

buffer_cache_cmd_t buffer_cache_cmd = {0};  // Single buffer cache instance

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
__IO uint8_t finger_uart_buffer[2][BUFFER_SIZE];  // Buffer for finger UART data, double buffer for receiving data
__IO uint8_t stm32_uart_buffer[2][BUFFER_SIZE];   // Buffer for STM32 UART data, double buffer for receiving data
__IO uint8_t finger_uart_buffer_index = 0;        // Index to track the current position in the finger UART buffer
__IO uint8_t stm32_uart_buffer_index  = 0;        // Index to track the current position in the STM32 UART buffer

__IO uint8_t sleep_time = 0;  // sleep timeout

__IO uint8_t fingerprint_addr_buffer[4] = {0};  // Buffer to store the fingerprint module address (Big-Endian format)

__IO uint8_t rising_edge_irq_flag     = 0;  // Flag to indicate a rising edge interrupt occurred
__IO uint32_t rising_edge_irq_timeout = 0;  // Timestamp of the rising edge interrupt

__IO uint8_t falling_edge_irq_flag     = 0;  // Flag to indicate a falling edge interrupt occurred
__IO uint32_t falling_edge_irq_timeout = 0;  // Timestamp of the falling edge interrupt

__IO uint8_t fingerprint_mcu_power_status         = FINGERPRINT_MCU_POWER_OFF;
__IO uint8_t fingerprint_mcu_init_status          = FINGERPRINT_MCU_INIT_INCOMPLETE;
__IO uint8_t fingerprint_sensor_sleep_status      = FINGERPRINT_SENSOR_AWAKE;
__IO uint8_t fingerprint_data_forward_flag_status = FINGERPRINT_DATA_FORWARD_ENABLED;
__IO uint8_t fingerprint_wait_event_status        = WAIT_EVENT_NONE;
__IO uint8_t fingerprint_irq_trigger_status       = INTERRUPT_TRIGGER_NONE;
__IO uint8_t int_reset_flag                       = 0;

__IO uint8_t timer_timeout_status = TIMER_TIMEOUT_NOT_OCCURRED;

__IO uint8_t work_mode_status = WORK_MODE_SLEEP_PERIODIC;

__IO uint8_t fingerprint_module_busy_return_status = FINGERPRINT_BUSY_NOT_RETURNED;

__IO uint8_t finger_check_in_progress = 0;
__IO uint8_t finger_check_success     = 0;
__IO uint8_t finger_check_failed      = 0;

const uint8_t blocked_cmd[256] = {
    [0x08] = 1,
    [0x09] = 1,
    [0x0B] = 1,
    [0x12] = 1,
    [0x13] = 1,
    [0x15] = 1,
    [0x1A] = 1,
    [0x3B] = 1,
    [0xE0] = 1,
    [0xE1] = 1,
    [0xE2] = 1,
    [0xE3] = 1,
    [0xE4] = 1
};


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void iap_set()
{
    uint8_t i;
    uint32_t *pVecTab = (uint32_t *)(0x20000000);
    for (i = 0; i < 48; i++) {
        *(pVecTab++) = *(__IO uint32_t *)(APPLICATION_ADDRESS + (i << 2));
    }
    /* Enable the SYSCFG peripheral clock*/
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_SYSCFG_REMAPMEMORY_SRAM();
}

void unit_fingerprint_init(void)
{
    uint8_t flag = 0;  // Flag to track if default values are set

    // Retrieve active sleep timeout
    uint8_t temp = get_sleep_time();
    if (temp == INVALID_VALUE) {
        sleep_time = SLEEP_TIME_BASE;  // Set to default timeout
        flag++;                        // Indicate a default value was set
    } else {
        sleep_time = temp;  // Use stored value
    }

    // If any default values were set, save them to flash memory
    if (flag != 0) {
        set_sleep_time(sleep_time);
    }

    flag = 0;  // Reset flag for work mode status

    // Retrieve work mode status
    temp = get_work_mode();
    if (temp == INVALID_VALUE) {
        work_mode_status = WORK_MODE_BASE;  // Set to default work mode
        flag++;                             // Indicate a default value was set
    } else {
        work_mode_status = temp;  // Use stored value
    }

    // If any default values were set, save them to flash memory
    if (flag != 0) {
        set_work_mode(work_mode_status);
    }

    // Store the address in a buffer (Big-Endian format)
    for(uint8_t i=0;i<4;i++){
        fingerprint_addr_buffer[i] = 0xFF;
    }
}

void process_buffer_cache_cmd()
{
    if (!packet_check(buffer_cache_cmd.data, buffer_cache_cmd.size)) {
        switch (buffer_cache_cmd.cmd) {
            case CMD_SET_SLEEP_TIME:
                set_sleep_time_handle(buffer_cache_cmd.data[10]);
                break;
            case CMD_GET_SLEEP_TIME:
                get_sleep_time_handle();
                break;
            case CMD_SET_WORK_MODE:
                set_work_mode_handle(buffer_cache_cmd.data[10]);
                break;
            case CMD_GET_WORK_MODE:
                get_work_mode_handle();
                break;
            case CMD_ACTIVATE_FINGERPRINT_MODULE:
                activate_fingerprint_module_handle();
                break;
            case CMD_GET_FINGERPRINT_MODULE_STATUS:
                get_fingerprint_module_status_handle();
                break;
            case CMD_SAVE_CONFIGURATION_STATUS:
                save_configuration_status_handle(buffer_cache_cmd.data[10]);
                break;
            case CMD_GET_FIRMWARE_VERSION:
                get_stm32_fw_ver();
                break;
            case CMD_IAP_RESET_STM32_MCU:
                iap_reset_stm32_mcu_handle();
                break;
            default:
                break;
        }
    } else {
        return_data_packet_handle(RECEIVE_PACKET_ERROR);
    }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* USER CODE BEGIN 1 */
    iap_set();
    unit_fingerprint_init();
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_IWDG_Init();
    MX_TIM2_Init();
    /* USER CODE BEGIN 2 */

    HAL_TIM_Base_Stop_IT(&htim2);
    __HAL_GPIO_EXTI_CLEAR_IT(TOUCH_DETECTION_Pin);
    HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);

    // Initialize the fingerprint module
#if DEBUG
    printf("MCU Start\r\n");
#endif

    // Enable UART1 DMA idle-line reception interrupt
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, finger_uart_buffer[finger_uart_buffer_index], BUFFER_SIZE);
    finger_uart_buffer_index ^= 1;
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    fingerprint_init();

    // Enable UART1 DMA idle-line reception interrupt
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, stm32_uart_buffer[stm32_uart_buffer_index], BUFFER_SIZE);
    stm32_uart_buffer_index ^= 1;
    __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        if (buffer_cache_cmd.valid) {
            // Process the buffer cache command
            process_buffer_cache_cmd();
            // Reset the buffer cache command after processing
            buffer_cache_cmd.valid = 0;
        } else {
            HAL_UARTEx_ReceiveToIdle_DMA(&huart2, stm32_uart_buffer[1 - stm32_uart_buffer_index], BUFFER_SIZE);
            __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
        }
        // If a timeout occurs, attempt to shut down the fingerprint module.
        if (timer_timeout_status == TIMER_TIMEOUT_OCCURRED) {
            start_finger_check();
        }

        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        HAL_IWDG_Refresh(&hiwdg);
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv              = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState            = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN            = 8;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TOUCH_DETECTION_Pin) {
        rising_edge_irq_timeout = HAL_GetTick();
        rising_edge_irq_flag    = 1;
        if (fingerprint_irq_trigger_status == INTERRUPT_TRIGGER_WAKEUP) {
            set_exit_trigger_mode(INTERRUPT_TRIGGER_NONE);
            open_fingerprint_mcu();
            send_passive_wakeup_packet();
            start_timer_handle();
#if DEBUG
            printf("Exit Awake Fingerprint\r\n");
#endif
        }
    }
}
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TOUCH_DETECTION_Pin) {
        falling_edge_irq_timeout = HAL_GetTick();
        falling_edge_irq_flag    = 1;
        if (fingerprint_irq_trigger_status == INTERRUPT_TRIGGER_RESET) {
            uint32_t time_diff = falling_edge_irq_timeout - rising_edge_irq_timeout;
            if (time_diff >= 5 && time_diff <= 15) {
                int_reset_flag = 1;
#if DEBUG
                printf("Sleep Reset Init Success\r\n");
#endif
            }
        }
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == (&htim2)) {
        if (timer_timeout_status == TIMER_TIMEOUT_NOT_OCCURRED) {
            timer_timeout_status     = TIMER_TIMEOUT_OCCURRED;
            finger_check_in_progress = 0;
            finger_check_success     = 0;
            finger_check_failed      = 0;
            HAL_TIM_Base_Stop_IT(&htim2);
#if DEBUG
            printf("Timer Timeout\r\n");
#endif
        }
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == &huart1) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, finger_uart_buffer[finger_uart_buffer_index], BUFFER_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
        finger_uart_buffer_index ^= 1;
#if DEBUG
        printf("rx_data_len: %d ", Size);
        printf("rx_data:");
        for (uint8_t i = 0; i < Size; i++) {
            printf(" 0x%02x", finger_uart_buffer[finger_uart_buffer_index][i]);
        }
        printf("\r\n");
#endif
        if (Size > 2) {
            if (fingerprint_data_forward_flag_status == FINGERPRINT_DATA_FORWARD_ENABLED) {
                usart2_transmit_dma(finger_uart_buffer[finger_uart_buffer_index], Size);
            } else {
                if (Size == 12) {
                    uint8_t code   = finger_uart_buffer[finger_uart_buffer_index][9];
                    uint8_t suffix = finger_uart_buffer[finger_uart_buffer_index][11];
                    if (fingerprint_wait_event_status == WAIT_EVENT_SLEEP_RETURN) {
                        if (code == 0x00 && suffix == 0x0A) {
                            fingerprint_sensor_sleep_status = FINGERPRINT_SENSOR_SLEEP;
                        } else if (code == 0x01 && suffix == 0x0B) {
                            fingerprint_sensor_sleep_status = FINGERPRINT_SENSOR_AWAKE;
                        }
                    } else if (fingerprint_wait_event_status == WAIT_EVENT_IMAGE_RETURN) {
                        finger_check_in_progress = 0;
                        if (code == 0x00 && suffix == 0x0A) {
                            finger_check_success++;
                        } else if (code == 0x02 && suffix == 0x0C) {
                            finger_check_failed++;
                        }
                    }
                }
            }

        } else if (Size == 1 && finger_uart_buffer[finger_uart_buffer_index][0] == 0x55) {
            if (fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_INCOMPLETE) {
                fingerprint_mcu_init_status = FINGERPRINT_MCU_INIT_COMPLETE;
            }
        }
        reset_timer_handle();
    } else if (huart == &huart2) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
        __HAL_UART_CLEAR_OREFLAG(&huart2);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart2, stm32_uart_buffer[stm32_uart_buffer_index], BUFFER_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
        __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
        stm32_uart_buffer_index ^= 1;
        if (Size >= 11) {
            // Cache package ID and command byte to avoid repeated access
            uint8_t package_id = stm32_uart_buffer[stm32_uart_buffer_index][6];  // Package ID
            uint8_t cmd_byte   = stm32_uart_buffer[stm32_uart_buffer_index][9];  // Command byte

            // Check if the package ID is 0x01
            if (package_id == 0x01) {
                // Command byte in range 0xD0-0xD8 or 0xFD
                if ((cmd_byte >= 0xD0 && cmd_byte <= 0xD7) || cmd_byte == 0xFD) {
                    // Cache data if not already valid
                    if (!buffer_cache_cmd.valid) {
                        memcpy(buffer_cache_cmd.data, stm32_uart_buffer[stm32_uart_buffer_index], Size);
                        buffer_cache_cmd.cmd   = cmd_byte;
                        buffer_cache_cmd.size  = Size;
                        buffer_cache_cmd.valid = 1;
                    }
                }
                // Check if the command is blocked
                else if (blocked_cmd[cmd_byte] || (cmd_byte == 0x0E && stm32_uart_buffer[stm32_uart_buffer_index][10] == 0x04)) {
                     return_data_packet_handle(OPERATION_BLOCKED);
                }
                // Default case for other valid command bytes
                else {
                    if (fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_COMPLETE) {
                        if (fingerprint_data_forward_flag_status == FINGERPRINT_DATA_FORWARD_ENABLED) {
                            usart1_transmit_dma(stm32_uart_buffer[stm32_uart_buffer_index], Size);
                        } else if (fingerprint_data_forward_flag_status == FINGERPRINT_DATA_FORWARD_DISABLED) {
                            fingerprint_module_busy_return_status = FINGERPRINT_BUSY_RETURNED;
                            return_data_packet_handle(FINGERPRINT_MODULE_BUSY);
                        }
                    } else {
                        return_data_packet_handle(FINGERPRINT_MODULE_NOT_OPEN);
                    }
                }
            }
            // Handle cases where the package ID is not 0x01
            else {
                if (fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_COMPLETE) {
                    if (fingerprint_data_forward_flag_status == FINGERPRINT_DATA_FORWARD_ENABLED) {
                        usart1_transmit_dma(stm32_uart_buffer[stm32_uart_buffer_index], Size);
                    } else if (fingerprint_data_forward_flag_status == FINGERPRINT_DATA_FORWARD_DISABLED) {
                        fingerprint_module_busy_return_status = FINGERPRINT_BUSY_RETURNED;
                        return_data_packet_handle(FINGERPRINT_MODULE_BUSY);
                    }
                } else {
                    return_data_packet_handle(FINGERPRINT_MODULE_NOT_OPEN);
                }
            }

        } else {
            return_data_packet_handle(RECEIVE_PACKET_ERROR);
        }
        reset_timer_handle();
    }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
