/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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
#include "crc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32g0xx_hal_flash_ex.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum {
    IAP_WAITING_ENABLE = 0x00,  // 等待使能
    IAP_ENABLE_COMPLETE,        // 使能成功
    IAP_RECEIVING_DATA,         // 接收数据
    IAP_RECEIVE_COMPLETE,       // 接收完成
    IAP_UPGRADE_COMPLETE        // 升级完成
} iap_update_t;                 // IAP升级状态

#define APPLICATION_ADDRESS             (0x08002000)  // APP的起始地
#define FW_LENGTH                       (0xC000)
#define FW_CRC_ADDR                     ((APPLICATION_ADDRESS) + (FW_LENGTH) - 4)
#define UART_RX_LEN                     (2056)
#define STM32G0xx_PAGE_SIZE             (0x800)      // 页的大小
#define STM32G0xx_FLASH_PAGE0_STARTADDR (0x8000000)  // 设备的起始地址
#define CHECK_PASS                      (1)          // CRC检测通过
#define CHECK_ERROR                     (0)          // CRC检测未通过
#define RECEIVE_START                   (0x99)       // 开始接收数据使能标记
#define RECEIVE_ENABLE                  (0x88)       // 告诉主从机准备好接收数据
#define RECEIVE_SUCCESS                 (0x77)       // 接收成功
#define RECEIVE_FAIL                    (0x66)       // 接收失败,重新发送数据包
#define RECEIVE_ACCOMPLISH              (0x55)       // 接收完成,即将跳转APP
#define PACKET_DATA_CONTINUE            (0x01)       // 表示还有后续数据
#define PACKET_DATA_END                 (0x02)       // 表示最后一个包

#define DELAY_TIME (500)

typedef void (*pFunction)(void);  // 函数指针

uint8_t receive_buffer[UART_RX_LEN];  // 接收数据的buffer
FLASH_EraseInitTypeDef my_flash;      // 页擦除结构体，一共2kb
uint16_t indexs = 0;                  // 缓冲区下标标志位
uint32_t jump_address;                // 跳转程序的地址
pFunction jump_to_application;        // 函数指针
static uint8_t data;
static uint8_t iap_update_status = IAP_WAITING_ENABLE;

uint32_t lastTime = 0;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t compute_fw_crc32(void)
{
    uint32_t crcsum, crc_read, len;
    uint8_t *pdata = (uint8_t *)APPLICATION_ADDRESS;
    len            = FW_LENGTH - 4;
    crc_read       = *(uint32_t *)FW_CRC_ADDR;
    crcsum         = HAL_CRC_Calculate(&hcrc, (uint32_t *)pdata, len) ^ 0xFFFFFFFF;
    if (crc_read == crcsum) {
        return 1;
    } else {
        return 0;
    }
}

void uart2_ll_send_char(char ch)
{
    while (!LL_USART_IsActiveFlag_TXE(USART2)) {
    }
    LL_USART_TransmitData8(USART2, ch);
    while (!LL_USART_IsActiveFlag_TC(USART2)) {
    }
}

void reset_all_periph(void)
{
    LL_USART_DeInit(USART2);
    LL_USART_Disable(USART2);
    LL_USART_DisableIT_RXNE(USART2);
    SysTick->CTRL = 0;
    SYSCFG->CFGR1 &= SYSCFG_CFGR1_MEM_MODE;
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;
    /* Set EXTICRx registers to reset value */
    EXTI->EXTICR[0] = 0;
    EXTI->EXTICR[1] = 0;
    EXTI->EXTICR[2] = 0;
    EXTI->EXTICR[3] = 0;
    /* Set CFGR2 register to reset value: clear SRAM parity error flag */
    SYSCFG->CFGR2 |= (uint32_t)SYSCFG_CFGR2_SRAM_PE;
    LL_RCC_DeInit();
    HAL_DeInit();
}

// 跳转目标APP
void jump_app(void)
{
    if (compute_fw_crc32()) {
        /*check the application address context whether avilible*/
        if (((*(__IO uint32_t *)APPLICATION_ADDRESS) & 0x2FFE0000) == 0x20000000) {
            /* Jump to user application */
            reset_all_periph();
            jump_address        = *(__IO uint32_t *)(APPLICATION_ADDRESS + 4);
            jump_to_application = (pFunction)jump_address;
            /* Initialize user application's Stack Pointer */
            __set_MSP(*(__IO uint32_t *)APPLICATION_ADDRESS);
            jump_to_application();
        }
    }
}

// 串口中断
void USART2_IRQHandler(void)
{
    /* USER CODE BEGIN UART2_IRQn 0 */
    if (LL_USART_IsActiveFlag_RXNE(USART2)) {
        data = LL_USART_ReceiveData8(USART2);
        switch (iap_update_status) {
            case IAP_WAITING_ENABLE:  // 等待使能
                if (data == RECEIVE_START) {
                	lastTime = HAL_GetTick() + DELAY_TIME * 1000;
                    iap_update_status = IAP_RECEIVING_DATA;
                    uart2_ll_send_char(RECEIVE_ENABLE);
                } else {
                    uart2_ll_send_char(RECEIVE_FAIL);
                }
                break;
            case IAP_RECEIVING_DATA:
                receive_buffer[indexs++] = data;
                // 数据包错误重新发送数据包
                if (indexs == 1) {
                    if (receive_buffer[0] != PACKET_DATA_CONTINUE && receive_buffer[0] != PACKET_DATA_END) {
                        indexs = 0;
                        uart2_ll_send_char(RECEIVE_FAIL);
                    }
                }
                // 接收到一个完整的数据包
                if (indexs == UART_RX_LEN) {
                    if (receive_buffer[0] == PACKET_DATA_CONTINUE) {
                        iap_update_status = IAP_RECEIVE_COMPLETE;
                    } else if (receive_buffer[0] == PACKET_DATA_END) {
                        iap_update_status = IAP_UPGRADE_COMPLETE;
                    }
                    indexs = 0;
                }
                break;
            default:
                break;
        }
    }
    /* USER CODE END UART2_IRQn 0 */

    /* USER CODE BEGIN UART2_IRQn 1 */

    /* USER CODE END UART2_IRQn 1 */
}

// 根据地址获得内存的页
static uint32_t get_page(uint32_t Addr)
{
    return (Addr - STM32G0xx_FLASH_PAGE0_STARTADDR) / FLASH_PAGE_SIZE;
}

// 写闪存
void write_code(void)
{
    uint16_t Number_Bytes_Transferred = 0;  // 本次传输的数据长度
    uint32_t Add_Flash, end_add_flash;      // 数据传输的首地址，末尾地址
    uint64_t Data       = 0;                // 要写入的数据，一次写8byte
    uint16_t Data_index = 8;                // 数据下标
    uint32_t Page_error = 0;  // 设置Page_error,如果出现错误这个变量会被设置为出错的FLASH地址
    FLASH_EraseInitTypeDef my_flash;  // 页擦除所在的位置
    uint32_t PageNum = 0;             // 地址在的页

    Add_Flash     = receive_buffer[1] << 24 | receive_buffer[2] << 16 | receive_buffer[3] << 8 | receive_buffer[4] << 0;
    end_add_flash = Add_Flash + 2048;

    if (Add_Flash < APPLICATION_ADDRESS) {
          return;
    }

    Number_Bytes_Transferred = (receive_buffer[5] << 8) + receive_buffer[6];

    if (Number_Bytes_Transferred > 0) {
        FLASH_WaitForLastOperation(50);
    step_unlock:
        if (HAL_FLASH_Unlock() != HAL_OK) {
            goto step_unlock;
        }

        PageNum = get_page(Add_Flash);

        my_flash.TypeErase = FLASH_TYPEERASE_PAGES;  // 标明Flash执行页面只做擦除操作
        my_flash.Page      = PageNum;
        my_flash.NbPages   = 1;  // 说明要擦除的页数

    step_erase:
        FLASH_WaitForLastOperation(50);
        if (HAL_FLASHEx_Erase(&my_flash, &Page_error) != HAL_OK) {
            goto step_erase;
        }  // 调用擦除函数擦除

        while (Add_Flash < end_add_flash) {
            Data = receive_buffer[Data_index] | ((uint64_t)receive_buffer[Data_index + 1] << 8) |
                   ((uint64_t)receive_buffer[Data_index + 2] << 16) | ((uint64_t)receive_buffer[Data_index + 3] << 24) |
                   ((uint64_t)receive_buffer[Data_index + 4] << 32) | ((uint64_t)receive_buffer[Data_index + 5] << 40) |
                   ((uint64_t)receive_buffer[Data_index + 6] << 48) | ((uint64_t)receive_buffer[Data_index + 7] << 56);

        step_write:
            FLASH_WaitForLastOperation(50);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, Add_Flash, Data) != HAL_OK) {
                goto step_write;
            }
            Add_Flash  = Add_Flash + 8;
            Data_index = Data_index + 8;
        }
        HAL_FLASH_Lock();
        FLASH_WaitForLastOperation(50);
    }
    // clear buffer
    for (int i = 0; i < sizeof(receive_buffer); i++) {
        receive_buffer[i] = 0;
    }
}

void iap_usart(void)
{

	lastTime = HAL_GetTick() + DELAY_TIME;

    while (1) {

    	if (HAL_GetTick() > lastTime) {
    	            jump_app();
    	}

        switch (iap_update_status) {
            case IAP_RECEIVE_COMPLETE:
            	lastTime = HAL_GetTick() + DELAY_TIME * 1000;
                LL_USART_DisableIT_RXNE(USART2);
                write_code();
                LL_USART_EnableIT_RXNE(USART2);
                uart2_ll_send_char(RECEIVE_SUCCESS);
                iap_update_status = IAP_RECEIVING_DATA;
                break;
            case IAP_UPGRADE_COMPLETE:
            	lastTime = HAL_GetTick() + DELAY_TIME * 1000;
                uart2_ll_send_char(RECEIVE_SUCCESS);
                LL_USART_DisableIT_RXNE(USART2);
                write_code();
                LL_USART_EnableIT_RXNE(USART2);
                uart2_ll_send_char(RECEIVE_ACCOMPLISH);
                LL_mDelay(10);
                jump_app();
                break;
            default:
                break;
        }
    }
}

void iap_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure GPIO pins : PB7 PB3 */
    GPIO_InitStruct.Pin  = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
//  MX_USART2_UART_Init();
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */
  iap_gpio_init();
  LL_mDelay(200);
  if ((!(!!(GPIOA->IDR & GPIO_PIN_2))) && (!(!!(GPIOA->IDR & GPIO_PIN_3)))) {
      MX_USART2_UART_Init();
      LL_USART_EnableIT_RXNE(USART2);  // 使能串口接收中断
      iap_usart();
  } else {
      jump_app();
  }
  MX_USART2_UART_Init();
  LL_USART_EnableIT_RXNE(USART2);  // 使能串口接收中断
  iap_usart();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2)
  {
  }

  /* HSI configuration and activation */
  LL_RCC_HSI_Enable();
  while(LL_RCC_HSI_IsReady() != 1)
  {
  }

  /* Main PLL configuration and activation */
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLR_DIV_2);
  LL_RCC_PLL_Enable();
  LL_RCC_PLL_EnableDomain_SYS();
  while(LL_RCC_PLL_IsReady() != 1)
  {
  }

  /* Set AHB prescaler*/
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

  /* Sysclk activation on the main PLL */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  }

  /* Set APB1 prescaler*/
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  /* Update CMSIS variable (which can be updated also through SystemCoreClockUpdate function) */
  LL_SetSystemCoreClock(64000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
