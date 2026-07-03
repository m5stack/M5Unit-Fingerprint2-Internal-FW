#include "fingerprint.h"
#include "string.h"
#include "stdio.h"
#include "myflash.h"
#include "tim.h"

extern __IO uint8_t finger_uart_buffer[2][BUFFER_SIZE];
extern __IO uint8_t stm32_uart_buffer[2][BUFFER_SIZE];
extern __IO uint8_t finger_uart_buffer_index;
extern __IO uint8_t stm32_uart_buffer_index;

extern __IO uint8_t sleep_time;

extern __IO uint8_t fingerprint_mcu_power_status;
extern __IO uint8_t fingerprint_mcu_init_status;
extern __IO uint8_t fingerprint_sensor_sleep_status;
extern __IO uint8_t fingerprint_data_forward_flag_status;
extern __IO uint8_t int_reset_flag;
extern __IO uint8_t fingerprint_wait_event_status;
extern __IO uint8_t fingerprint_irq_trigger_status;

extern __IO uint8_t rising_edge_irq_flag;
extern __IO uint32_t rising_edge_irq_timeout;

extern __IO uint8_t falling_edge_irq_flag;
extern __IO uint32_t falling_edge_irq_timeout;

extern __IO uint8_t fingerprint_addr_buffer[4];

extern __IO uint8_t timer_timeout_status;

extern __IO uint8_t work_mode_status;

extern __IO uint8_t finger_check_in_progress;
extern __IO uint8_t finger_check_success;
extern __IO uint8_t finger_check_failed;
extern __IO uint8_t fingerprint_module_busy_return_status;

static uint8_t passive_wakeup_response[12] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 0x03, 0xFF, 0x01, 0x09};

static uint8_t ps_sleep[12] = {0xEF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x33, 0x00, 0x37};

static uint8_t ps_get_image[12] = {0xEF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x01, 0x00, 0x05};

static uint8_t return_packet_buf[32] = {0};

void usart1_transmit_dma(uint8_t *buffer, uint16_t size)
{
    // Check if UART2 is ready to transmit (TXE = 1 indicates data register is empty)
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE)) {
        // If UART is ready, transmit data via DMA
        HAL_UART_Transmit_DMA(&huart1, buffer, size);  // Use DMA to transmit data
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, finger_uart_buffer[1 - finger_uart_buffer_index], BUFFER_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    }
}

void usart2_transmit_dma(uint8_t *buffer, uint16_t size)
{
    // Check if UART2 is ready to transmit (TXE = 1 indicates data register is empty)
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE)) {
        // If UART is ready, transmit data via DMA
        HAL_UART_Transmit_DMA(&huart2, buffer, size);  // Use DMA to transmit data
    }
}

void fingerprint_init(void)
{
    // Disable external interrupt trigger
    set_exit_trigger_mode(INTERRUPT_TRIGGER_NONE);

    // Power on fingerprint MCU
    open_fingerprint_mcu();

    // Wait until initialization is complete (0x55 signal received)
    while (fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_INCOMPLETE) {
        // Busy wait
    }

    if (work_mode_status == WORK_MODE_ALWAYS_ON) {
        return;
    } else {
        // Enter wait mode for sleep command acknowledgment
        fingerprint_enter_wait_mode(WAIT_EVENT_SLEEP_RETURN);

        // Set external interrupt mode for reset trigger
        set_exit_trigger_mode(INTERRUPT_TRIGGER_RESET);

        // Send fingerprint sensor sleep command
        set_fingerprint_sleep();

        // Wait for both:
        // 1. Reset interrupt signal (int_reset_flag == 1)
        // 2. Sleep response from fingerprint sensor
        while (int_reset_flag == 0 || fingerprint_sensor_sleep_status == FINGERPRINT_SENSOR_AWAKE) {
            // Busy wait
        }

        // Exit wait mode
        fingerprint_exit_wait_mode();

        // Power off fingerprint MCU
        close_fingerprint_mcu();

        // Set external interrupt mode for wake-up trigger
        set_exit_trigger_mode(INTERRUPT_TRIGGER_WAKEUP);
    }
}

void start_finger_check(void)
{
    if (finger_check_in_progress) {
        return;
    }

    if (fingerprint_module_busy_return_status == FINGERPRINT_BUSY_RETURNED) {
        fingerprint_exit_wait_mode();

        start_timer_handle();

        timer_timeout_status = TIMER_TIMEOUT_NOT_OCCURRED;

        fingerprint_module_busy_return_status = FINGERPRINT_BUSY_NOT_RETURNED;

        return;
    }

    if (finger_check_failed == 0) {
        fingerprint_enter_wait_mode(WAIT_EVENT_IMAGE_RETURN);
    }

    if (finger_check_success) {
        fingerprint_exit_wait_mode();

        start_timer_handle();

        timer_timeout_status = TIMER_TIMEOUT_NOT_OCCURRED;

        return;
    }

    if (finger_check_failed == MAX_NO_FINGER_COUNT) {
        // Enter wait mode for sleep command acknowledgment
        fingerprint_enter_wait_mode(WAIT_EVENT_SLEEP_RETURN);

        // Set external interrupt mode for reset trigger
        set_exit_trigger_mode(INTERRUPT_TRIGGER_RESET);

        // Send fingerprint sensor sleep command
        set_fingerprint_sleep();

        // Wait for both:
        // 1. Reset interrupt signal (int_reset_flag == 1)
        // 2. Sleep response from fingerprint sensor
        while (int_reset_flag == 0 || fingerprint_sensor_sleep_status == FINGERPRINT_SENSOR_AWAKE) {
            // Busy wait
        }

        // Exit wait mode
        fingerprint_exit_wait_mode();

        // Power off fingerprint MCU
        close_fingerprint_mcu();

        // Set external interrupt mode for wake-up trigger
        set_exit_trigger_mode(INTERRUPT_TRIGGER_WAKEUP);

        timer_timeout_status = TIMER_TIMEOUT_NOT_OCCURRED;

        return;
    }

    finger_check_in_progress = 1;

    send_get_image_packet();
}

void open_fingerprint_mcu(void)
{
    fingerprint_mcu_power_status = FINGERPRINT_MCU_POWER_ON;
    fingerprint_mcu_init_status  = FINGERPRINT_MCU_INIT_INCOMPLETE;
    HAL_GPIO_WritePin(MCU_EN_GPIO_Port, MCU_EN_Pin, GPIO_PIN_SET);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, finger_uart_buffer[1 - finger_uart_buffer_index], BUFFER_SIZE);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

void close_fingerprint_mcu(void)
{
    fingerprint_mcu_power_status = FINGERPRINT_MCU_POWER_OFF;
    fingerprint_mcu_init_status  = FINGERPRINT_MCU_INIT_INCOMPLETE;
    HAL_GPIO_WritePin(MCU_EN_GPIO_Port, MCU_EN_Pin, GPIO_PIN_RESET);
}

void set_fingerprint_sleep(void)
{
    fingerprint_sensor_sleep_status = FINGERPRINT_SENSOR_AWAKE;
    int_reset_flag                  = 0;
    send_sleep_packet();
}

void fingerprint_enter_wait_mode(fingerprint_wait_event_t event)
{
    fingerprint_data_forward_flag_status = FINGERPRINT_DATA_FORWARD_DISABLED;
    fingerprint_wait_event_status        = event;
}

void fingerprint_exit_wait_mode(void)
{
    fingerprint_data_forward_flag_status = FINGERPRINT_DATA_FORWARD_ENABLED;
    fingerprint_wait_event_status        = WAIT_EVENT_NONE;
}

void set_exit_trigger_mode(fingerprint_irq_trigger_t status)
{
    if (status == INTERRUPT_TRIGGER_NONE) {
        __HAL_GPIO_EXTI_CLEAR_IT(TOUCH_DETECTION_Pin);
        HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
    } else {
        __HAL_GPIO_EXTI_CLEAR_IT(TOUCH_DETECTION_Pin);
        HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
    }
    falling_edge_irq_flag          = 0;
    rising_edge_irq_flag           = 0;
    fingerprint_irq_trigger_status = status;
}

void send_sleep_packet(void)
{
    // Copy fingerprint module address and send passive wake-up response
    memcpy(&ps_sleep[2], fingerprint_addr_buffer, 4);
    usart1_transmit_dma(ps_sleep, 12);
}

void send_get_image_packet(void)
{
    // Copy fingerprint module address and send passive wake-up response
    memcpy(&ps_get_image[2], fingerprint_addr_buffer, 4);
    usart1_transmit_dma(ps_get_image, sizeof(ps_get_image));
}

void send_passive_wakeup_packet(void)
{
    // Copy fingerprint module address and send passive wake-up response
    memcpy(&passive_wakeup_response[2], fingerprint_addr_buffer, 4);
    usart2_transmit_dma(passive_wakeup_response, sizeof(passive_wakeup_response));
}

uint16_t crc_sum(uint8_t *buffer, uint16_t size)
{
    uint16_t data = 0;
    for (uint8_t i = 6; i < size - 2; i++) {
        data += buffer[i];
    }
    return data;
}

packet_check_status_t packet_check(uint8_t *buffer, uint16_t size)
{
    // Check packet header
    if (buffer[0] != PACKET_HEADER_HIGH_BYTE || buffer[1] != PACKET_HEADER_LOW_BYTE) {
        return PACKET_CHECK_HEADER_FAIL;
    }
    // Verify fingerprint module address
    if (memcmp(&buffer[2], fingerprint_addr_buffer, 4) != 0) {
        return PACKET_CHECK_ADDRESS_FAIL;
    }
    // Validate packet length
    if ((buffer[7] << 8 | buffer[8]) != (size - 9)) {
        return PACKET_CHECK_LENGTH_FAIL;
    }
    // Verify CRC checksum
    return (crc_sum(buffer, size) == (buffer[size - 2] << 8 | buffer[size - 1])) ? PACKET_CHECK_SUCCESS
                                                                                 : PACKET_CHECK_CRC_FAIL;
}

static void send_standard_response(uint8_t data)
{
    // Prepare and send a standard response packet
    memcpy(&return_packet_buf[2], fingerprint_addr_buffer, 4);
    return_packet_buf[0]  = 0xEF;
    return_packet_buf[1]  = 0x01;
    return_packet_buf[6]  = 0x07;
    return_packet_buf[7]  = 0x00;
    return_packet_buf[8]  = 0x04;
    return_packet_buf[9]  = 0x00;
    return_packet_buf[10] = data;
    uint16_t crc          = crc_sum(return_packet_buf, 13);
    return_packet_buf[11] = (uint8_t)(crc >> 8);
    return_packet_buf[12] = (uint8_t)(crc & 0xFF);
    usart2_transmit_dma(return_packet_buf, 13);
}

void set_sleep_time_handle(uint8_t time)
{
    // Validate and set active sleep time
    if (sleep_time >= 10 && sleep_time <= 254) {
        if (sleep_time != time) {
            sleep_time = time;
            if (work_mode_status == WORK_MODE_SLEEP_PERIODIC &&
                fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_COMPLETE) {
                start_timer_handle();
            }
        }
        return_data_packet_handle(OPERATION_SUCCESS);
    } else {
        return_data_packet_handle(ERROR_INVALID_PARAM);
    }
}

void get_sleep_time_handle(void)
{
    send_standard_response(sleep_time);
}

void set_work_mode_handle(uint8_t mode)
{
    if (mode >= WORK_MODE_SLEEP_PERIODIC && mode <= WORK_MODE_ALWAYS_ON) {
        if (work_mode_status != mode) {
            work_mode_status = mode;
            switch (work_mode_status) {
                case WORK_MODE_SLEEP_PERIODIC:
                    // Enter wait mode for sleep command acknowledgment
                    fingerprint_enter_wait_mode(WAIT_EVENT_SLEEP_RETURN);

                    // Set external interrupt mode for reset trigger
                    set_exit_trigger_mode(INTERRUPT_TRIGGER_RESET);

                    // Send fingerprint sensor sleep command
                    set_fingerprint_sleep();

                    // Wait for both:
                    // 1. Reset interrupt signal (int_reset_flag == 1)
                    // 2. Sleep response from fingerprint sensor
                    while (int_reset_flag == 0 || fingerprint_sensor_sleep_status == FINGERPRINT_SENSOR_AWAKE) {
                        // Busy wait
                    }

                    // Exit wait mode
                    fingerprint_exit_wait_mode();

                    // Power off fingerprint MCU
                    close_fingerprint_mcu();

                    // Set external interrupt mode for wake-up trigger
                    set_exit_trigger_mode(INTERRUPT_TRIGGER_WAKEUP);
                    break;
                case WORK_MODE_ALWAYS_ON:
                    if (fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_COMPLETE) {
                        HAL_TIM_Base_Stop_IT(&htim2);
                        __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
                        timer_timeout_status = TIMER_TIMEOUT_NOT_OCCURRED;
                        // Disable external interrupt trigger
                        set_exit_trigger_mode(INTERRUPT_TRIGGER_NONE);

                    } else {
                        // Disable external interrupt trigger
                        set_exit_trigger_mode(INTERRUPT_TRIGGER_NONE);

                        // Power on fingerprint MCU
                        open_fingerprint_mcu();

                        uint32_t start_time = HAL_GetTick();
                        while ((HAL_GetTick() - start_time) < 40) {
                            if (fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_COMPLETE) break;
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        return_data_packet_handle(OPERATION_SUCCESS);
    } else {
        return_data_packet_handle(ERROR_INVALID_PARAM);
    }
}

void get_work_mode_handle(void)
{
    send_standard_response(work_mode_status);
}

void activate_fingerprint_module_handle(void)
{
    if (fingerprint_mcu_power_status != FINGERPRINT_MCU_POWER_ON && work_mode_status == WORK_MODE_SLEEP_PERIODIC) {
        set_exit_trigger_mode(INTERRUPT_TRIGGER_NONE);
        open_fingerprint_mcu();
        uint32_t start_time = HAL_GetTick();
        while ((HAL_GetTick() - start_time) < 300) {
            if (fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_COMPLETE) break;
        }
        start_timer_handle();
    }

    reset_timer_handle();

    return_data_packet_handle(fingerprint_mcu_init_status == FINGERPRINT_MCU_INIT_COMPLETE ? OPERATION_SUCCESS
                                                                                           : RECEIVE_PACKET_ERROR);
}

void get_fingerprint_module_status_handle(void)
{
    send_standard_response(fingerprint_mcu_init_status);
}

void save_configuration_status_handle(uint8_t status)
{
    if (status == 0) {
        // Save configuration if changes exist
        if (sleep_time != get_sleep_time()) {
            return_data_packet_handle(set_sleep_time(sleep_time) ? OPERATION_SUCCESS : RECEIVE_PACKET_ERROR);
        } else {
            return_data_packet_handle(OPERATION_SUCCESS);
        }

    } else if (status == 1) {
        // Save configuration if changes exist
        if (work_mode_status != get_work_mode()) {
            return_data_packet_handle(set_work_mode(work_mode_status) ? OPERATION_SUCCESS : RECEIVE_PACKET_ERROR);
        } else {
            return_data_packet_handle(OPERATION_SUCCESS);
        }

    } else {
        return_data_packet_handle(ERROR_INVALID_PARAM);
    }
}

void get_stm32_fw_ver(void)
{
    send_standard_response(FIRMWARE_VERSION);
}

void return_data_packet_handle(uint8_t type)
{
    // Copy fingerprint module address into the packet
    memcpy(&return_packet_buf[2], fingerprint_addr_buffer, 4);

    // Set fixed packet header
    return_packet_buf[0] = 0xEF;
    return_packet_buf[1] = 0x01;

    // Set packet command and data length
    return_packet_buf[6] = 0x07;  // Command identifier
    return_packet_buf[7] = 0x00;
    return_packet_buf[8] = 0x03;  // Data length (3 bytes)

    // Set response type
    return_packet_buf[9] = type;

    // Compute CRC checksum
    uint16_t crc          = crc_sum(return_packet_buf, 12);
    return_packet_buf[10] = (uint8_t)(crc >> 8);
    return_packet_buf[11] = (uint8_t)(crc & 0xFF);

    // Transmit packet using DMA
    usart2_transmit_dma(return_packet_buf, 12);
}

void iap_reset_stm32_mcu_handle(void)
{
    // Reset STM32 MCU via IAP (In-Application Programming)
    uint32_t reset_wait_start = HAL_GetTick();
    close_fingerprint_mcu();
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_UART_MspDeInit(&huart2);

    // Configure GPIO pins as input with pull-up
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin  = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Wait for reset condition or timeout
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) || HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3)) {
        if (HAL_GetTick() - reset_wait_start > 3000) break;
    }

    NVIC_SystemReset();
}
