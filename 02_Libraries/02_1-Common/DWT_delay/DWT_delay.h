//
// Created by redmiX on 2025/12/2.
//

#ifndef RTOS_PROJECT_DWT_DELAY_H
#define RTOS_PROJECT_DWT_DELAY_H

#include <stdint.h>
#include "stm32f407xx.h"  // STM32F407系列芯片寄存器定义头文件

#define CPU_FREQ_MHZ     168UL   //CPU主频168MHz
#define CPU_FREQ_HZ      (CPU_FREQ_MHZ * 1000000UL)

void DWT_Delay_Init(void);

void DWT_Delay_ms(uint32_t ms);

#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
__forceinline static void DWT_Delay_us(uint32_t us)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline)) static inline void DWT_Delay_us(uint32_t us)
#else
static inline void DWT_Delay_us(uint32_t us)
#endif
{
  if (us > 10000000U) return;

  uint32_t start_cnt = DWT->CYCCNT;
  uint32_t delay_ticks = us * CPU_FREQ_MHZ;

  while ((DWT->CYCCNT - start_cnt) < delay_ticks);
}

// 安全的DWT循环计时宏
#define DWT_TIMEOUT_CHECK(start, timeout_us)              \
(((DWT->CYCCNT) >= (start)) ?                             \
((DWT->CYCCNT) - (start) > (timeout_us) * CPU_FREQ_MHZ) : \
((UINT32_MAX - (start) + (DWT->CYCCNT) + 1) > (timeout_us) * CPU_FREQ_MHZ))


#endif //RTOS_PROJECT_DWT_DELAY_H