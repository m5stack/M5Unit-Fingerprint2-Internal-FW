#include "myflash.h"

/**
 * @brief  Get flash page number from address
 * @param  addr Memory address
 * @retval Page number
 */
static uint32_t getPage(uint32_t addr)
{
    return (addr - STM32G0xx_FLASH_PAGE0_STARTADDR) / FLASH_PAGE_SIZE;
}

/**
 * @brief  Modify one byte in 8-byte data
 * @param  data Data to modify
 * @param  byte_index Byte index (0-7)
 * @param  new_value New byte value
 * @retval Modified data
 */
static void set_byte_in_uint64(uint64_t *data, uint8_t byte_index, uint8_t new_value)
{
    *data &= ~((uint64_t)(0xFF) << (byte_index * 8));
    // Set new byte value
    *data |= (uint64_t)new_value << (byte_index * 8);
}

/**
 * @brief  Read double word (64-bit) from flash
 * @param  address Memory address
 * @retval Read value
 */
static uint64_t my_flash_read_double_word(uint32_t address)
{
    return *((__IO uint64_t *)(address));
}

/**
 * @brief  Erase flash page
 * @param  page_address Page start address
 * @retval Operation status (true/false)
 */
static bool my_flash_earse_pages(uint32_t page_address)
{
    uint32_t page_error = 0;                     // Will be set to error address if operation fails
    FLASH_EraseInitTypeDef my_flash;             // Flash erase configuration
    my_flash.TypeErase = FLASH_TYPEERASE_PAGES;  // Page erase operation
    my_flash.Page      = getPage(page_address);
    my_flash.NbPages   = 1;  // Number of pages to erase
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&my_flash, &page_error);
    HAL_FLASH_Lock();
    if (status == HAL_OK) {
        return true;
    }
    return false;
}

/**
 * @brief  Write double word (64-bit) to flash
 * @param  address Memory address
 * @param  data Data to write
 * @retval Operation status (true/false)
 */
static bool my_flash_write_double_word(uint32_t address, uint64_t data)
{
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data);
    HAL_FLASH_Lock();
    if (status == HAL_OK) {
        return true;
    }
    return false;
}

/**
 * @brief  Get sleep time
 * @param  None
 * @retval Device address
 */
uint8_t get_sleep_time(void)
{
    return *((__IO uint8_t *)(SLEEP_TIME_ADDR));
}

/**
 * @brief  Set new sleep time in flash
 * @param  sleep_time New value to set
 * @retval Operation status
 */
bool set_sleep_time(uint8_t sleep_time)
{
    __disable_irq();
    uint64_t temp = my_flash_read_double_word(STM32G0xx_FLASH_PAGE29_STARTADDR);
    set_byte_in_uint64(&temp, 0, sleep_time);
    my_flash_earse_pages(STM32G0xx_FLASH_PAGE29_STARTADDR);
    while (my_flash_write_double_word(STM32G0xx_FLASH_PAGE29_STARTADDR, temp) != true) {
    }
    __enable_irq();
    if (sleep_time == get_sleep_time()) {
        return true;
    }
    return false;
}

/**
 * @brief  Get work mode from flash
 * @retval Current work mode value
 */
uint8_t get_work_mode(void)
{
    return *((__IO uint8_t *)(WORK_MODE_ADDR));
}

/**
 * @brief  Set new work mode in flash
 * @param  work_mode New value to set
 * @retval Operation status
 */
bool set_work_mode(uint8_t work_mode)
{
    __disable_irq();
    uint64_t temp = my_flash_read_double_word(STM32G0xx_FLASH_PAGE30_STARTADDR);
    set_byte_in_uint64(&temp, 0, work_mode);
    my_flash_earse_pages(STM32G0xx_FLASH_PAGE30_STARTADDR);
    while (my_flash_write_double_word(STM32G0xx_FLASH_PAGE30_STARTADDR, temp) != true) {
    }
    __enable_irq();
    if (work_mode == get_work_mode()) {
        return true;
    }
    return false;
}