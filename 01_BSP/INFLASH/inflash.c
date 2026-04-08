//
// Created by 35540 on 2026/4/7.
//
//******************************** Includes *********************************//
#include "inflash.h"
//******************************** Includes *********************************//
//---------------------------------------------------------------------------//
//******************************** Defines **********************************//
#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG       "INFLASH"
#else
#define LOG_TAG       "INFLASH"
#endif
//******************************** Defines **********************************//
//---------------------------------------------------------------------------//
//******************************** Variables *********************************//
//******************************** Variables ********************************//
//---------------------------------------------------------------------------//
//******************************** Macros ***********************************//
#define INFLASH_DEBUG
#if defined(INFLASH_DEBUG) && defined(MYDEBUG)
#define LOG_DEBUG(fmt, ...)  log_d("[%s][%s:%d][DEBUG] " fmt "\r\n", \
LOG_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  log_e("[%s][%s:%d][ERROR] " fmt "\r\n", \
LOG_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define ASSERT_NOT_NULL(ptr)                     do { \
if ((ptr) == NULL) {                                  \
LOG_ERROR("Invalid parameter: %s is NULL", #ptr); \
return 1;                                         \
}                                                     \
} while(0)
#else
#define LOG_DEBUG(fmt, ...)   ((void)0)
#define LOG_ERROR(fmt, ...)   ((void)0)
#define ASSERT_NOT_NULL(ptr)  ((void)0)
#endif
//******************************** Macros ***********************************//
//---------------------------------------------------------------------------//
//******************************** 函数声明   *********************************//
//******************************** 函数声明   *********************************//
//---------------------------------------------------------------------------//
//******************************** Variables ********************************//

//******************************** Variables ********************************//
//---------------------------------------------------------------------------//
//******************************** Functions ********************************//
/**
 * @brief  擦除Flash 指定区域
 * @参  数  p_erase_param  擦除参数结构体指针
 * @note   流程：参数校验→解锁→关中断→擦除→开中断→上锁
 * @retval 0成功 1失败
 */
uint8_t flash_erase(FLASH_EraseInitTypeDef *p_erase_param)
{
  ASSERT_NOT_NULL(&p_erase_param);
  // 1. 参数校验
  // 2. 真正有效的安全校验：防止外部传入非法参数把 Bootloader 擦了
  if (p_erase_param->Sector > 11 ||
      (p_erase_param->Sector + p_erase_param->NbSectors) > 12) {
    LOG_DEBUG("erase param out of range!");
    return 1;
      }
  uint32_t sector_error = 0;
  // 2. 核心原子操作
  HAL_FLASH_Unlock();
  __disable_irq(); // 绝对不能少

  HAL_StatusTypeDef ret = HAL_FLASHEx_Erase(p_erase_param, &sector_error);

  __enable_irq();
  HAL_FLASH_Lock();
  if (HAL_OK != ret || 0xFFFFFFFF != sector_error)
  {
    LOG_DEBUG("erase inflash error");
    return 1;
  }
  LOG_DEBUG("erase inflash success");
  return 0;
}

/**
 * @brief 按双字(64位)写入 Flash - 极速写入版 (F407专属优化)
 * @note  写入速度比 flash_program_word 快一倍，极其适合 Ymodem 接收固件
 * @param addr      起始地址 (必须8字节对齐，即地址末三位为 000)
 * @param data      待写入数据指针 (指针本身也建议8字节对齐)
 * @param dword_cnt 要写入的“双字”个数 (1个双字 = 8字节)
 */
HAL_StatusTypeDef flash_program_dword_fast(uint32_t addr, const uint64_t *data, uint32_t dword_cnt)
{
  // 1. 严苛的参数校验（必须8字节对齐）
  if (data == NULL || dword_cnt == 0) return HAL_ERROR;
  if ((addr & 0x07) != 0) return HAL_ERROR;           // 地址必须 8 字节对齐
  if (((uint32_t)data & 0x07) != 0) return HAL_ERROR;  // 指针也必须 8 字节对齐，否则硬件总线错误
  if (addr < FLASH_PARTA_BASE_ADDR || (addr + dword_cnt * 8) > (FLASH_PARTA_BASE_ADDR + FLASH_PARTA_MAX_SIZE))
    return HAL_ERROR;

  HAL_FLASH_Unlock();
  __disable_irq();

  HAL_StatusTypeDef ret = HAL_OK;
  for (uint32_t i = 0; i < dword_cnt; i++)
  {
    // 注意这里变成了 FLASH_TYPEPROGRAM_DOUBLEWORD，每次塞进去 8 字节
    ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                        addr + i * 8,
                                data[i]);
    if (ret != HAL_OK) {
      break;
    }
  }

  __enable_irq();
  HAL_FLASH_Lock();

  return ret;
}