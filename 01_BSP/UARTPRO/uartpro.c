//
// Created by 35540 on 2026/4/8.
//
//******************************** Includes *********************************//
#include "uartpro.h"

#include <stdio.h>
//******************************** Includes *********************************//
//---------------------------------------------------------------------------//
//******************************** Defines **********************************//
#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG       "UARTPRO"
#else
#define LOG_TAG       "UARTPRO"
#endif

//******************************** Defines **********************************//
//******************************** Macros ***********************************//
#if defined(UART_DEBUG) && defined(MYDEBUG)
#define LOG_DEBUG(fmt, ...)  printf("[%s][%s:%d][DEBUG] " fmt "\r\n", \
LOG_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  printf("[%s][%s:%d][ERROR] " fmt "\r\n", \
LOG_TAG, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)   ((void)0)
#define LOG_ERROR(fmt, ...)   ((void)0)
#endif
//******************************** Macros ***********************************//
//---------------------------------------------------------------------------//
//******************************** 函数声明   *********************************//
//******************************** 函数声明   *********************************//
//---------------------------------------------------------------------------//
//******************************** Variables ********************************//
/** 缓冲区A数据存储 */
static uint8_t uart_rx_buf_a[UART_RX_BUF_SIZE];
/** 缓冲区B数据存储 */
static uint8_t uart_rx_buf_b[UART_RX_BUF_SIZE];
//******************************** Variables ********************************//
//---------------------------------------------------------------------------//
//******************************** Functions ********************************//
/**
 * @brief UART1接收控制块
 */
static uart_rx_ctrl_t uart1_rx_ctrl = {
  .buf = {
    [0] = { .buf = uart_rx_buf_a, .len = 0, .state = UART_BUF_IDLE },
    [1] = { .buf = uart_rx_buf_b, .len = 0, .state = UART_BUF_IDLE },
            },
                .buf_size      = UART_RX_BUF_SIZE,
                .active_idx    = 0,
                .new_data_flag = 0
};

void uart1_rx_start(void)
{
  // 1. 重置控制块
  uart1_rx_ctrl.buf[0].state = UART_BUF_IDLE;
  uart1_rx_ctrl.buf[0].len   = 0;
  uart1_rx_ctrl.buf[1].state = UART_BUF_IDLE;
  uart1_rx_ctrl.buf[1].len   = 0;
  uart1_rx_ctrl.active_idx   = 0;
  uart1_rx_ctrl.new_data_flag= 0;
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1,
                               uart1_rx_ctrl.buf[0].buf,
                               uart1_rx_ctrl.buf_size);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == USART1)
  {
    HAL_UART_RxEventTypeTypeDef event_type = HAL_UARTEx_GetRxEventType(&huart1);
    if(event_type == HAL_UART_RXEVENT_IDLE)
    {
      // 1. 获取当前活跃缓冲区，记录接收长度
      uart_buf_t *current_buf = &uart1_rx_ctrl.buf[uart1_rx_ctrl.active_idx];
      current_buf->len = Size; // YMODEM块大小固定，Size应为1024
      current_buf->state = UART_BUF_FULL; // 标记为满，等待处理
      LOG_DEBUG("buf[%d] FULL, len=%d", uart1_rx_ctrl.active_idx, Size);
      // 2. 切换活跃缓冲区（下次DMA写入buf[1]）
      uart1_rx_ctrl.active_idx = 1 - uart1_rx_ctrl.active_idx;
      // 6. 检查另一个缓冲区是否可用
      if (uart1_rx_ctrl.buf[uart1_rx_ctrl.active_idx].state != UART_BUF_IDLE)
      {
        // 两个缓冲区都为FULL（应用层处理过慢），无法切换
        LOG_ERROR("Both buffers FULL, data lost");
        return;
      }
      // 3. 启动下一次DMA接收（指向新的活跃缓冲区）
      HAL_UARTEx_ReceiveToIdle_DMA(&huart1,
                                   uart1_rx_ctrl.buf[uart1_rx_ctrl.active_idx].buf,
                                   uart1_rx_ctrl.buf_size);
    }

    uart1_rx_ctrl.new_data_flag = 1;
  }

}

/**
 * @brief 获取一个已接收完成的缓冲区
 * 返回第一个状态为FULL的缓冲区指针，若没有则返回NULL。
 * @return uart_buf_t* 指向已满缓冲区的指针，若无数据则返回NULL
 */
uart_buf_t *uart1_rx_get_read_buf(void)
{
  if (uart1_rx_ctrl.buf[0].state == UART_BUF_FULL)
  {
    return &uart1_rx_ctrl.buf[0];
  }
  if (uart1_rx_ctrl.buf[1].state == UART_BUF_FULL)
  {
    return &uart1_rx_ctrl.buf[1];
  }
  return NULL;
}

/**
 * @brief 释放当前已读取的缓冲区
 * 将状态为FULL的缓冲区重置为IDLE并清零长度，使其可被DMA重新使用。
 */
void uart1_rx_release_read_buf(void)
{
  if (uart1_rx_ctrl.buf[0].state == UART_BUF_FULL)
  {
    uart1_rx_ctrl.buf[0].state = UART_BUF_IDLE;
    uart1_rx_ctrl.buf[0].len   = 0;
  }
  else if (uart1_rx_ctrl.buf[1].state == UART_BUF_FULL)
  {
    uart1_rx_ctrl.buf[1].state = UART_BUF_IDLE;
    uart1_rx_ctrl.buf[1].len   = 0;
  }
}