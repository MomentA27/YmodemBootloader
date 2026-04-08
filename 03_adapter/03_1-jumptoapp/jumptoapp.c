//
// Created by 35540 on 2026/4/8.
//
//******************************** Includes *********************************//
#include "jumptoapp.h"
//******************************** Includes *********************************//
//---------------------------------------------------------------------------//
//******************************** Defines **********************************//
#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG       "JUMPAPP"
#else
#define LOG_TAG       "JUMPAPP"
#endif


/* LED端口操作定义 */
#define LED0(x)   do{ x ? \
  HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin, GPIO_PIN_SET) : \
  HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin, GPIO_PIN_RESET);\
}while(0)       /* LED0翻转 */
//******************************** Defines **********************************//
//---------------------------------------------------------------------------//
//******************************** Variables *********************************//
//******************************** Variables ********************************//
//---------------------------------------------------------------------------//
//******************************** Macros ***********************************//
#if defined(JUMPAPP) && defined(MYDEBUG)
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
uint8_t KeyScan(void)
{
  if(HAL_GPIO_ReadPin(KEY_GPIO_Port,KEY_Pin) == 0)
  {
    DWT_Delay_ms(5000);
    if(HAL_GPIO_ReadPin(KEY_GPIO_Port,KEY_Pin) == 0)
    {
      return 1;
    }
  }
  return 0;
}
/**
 * @brief  清理Bootloader相关资源，为跳转到应用程序做准备
 * @note   该函数会禁用所有Bootloader使用的外设，包括UART、I2C、SPI、DMA等，
 *         并复位所有GPIO配置，关闭GPIO时钟，确保跳转到应用程序后不会发生冲突
 * @param  无
 * @retval 无
 */
static void BootLoader_Clear_optimized(void) {

}

/**
 * @brief  跳转到应用程序执行
 * @note   该函数会检查应用程序栈顶指针的有效性，然后执行以下操作：
 *         1. 关闭中断
 *         2. 清理Bootloader相关资源
 *         3. 切换主堆栈指针(MSP)到应用程序栈顶
 *         4. 重定向中断向量表到应用程序区域
 *         5. 再次清理资源
 *         6. 执行跳转到应用程序
 * @param  无
 * @retval 无
 */
void JumpToApp(void)
{
  // 1. 获取应用程序栈顶指针（地址偏移0处）
  uint32_t app_stack_top = *(uint32_t *)(APP_START_ADDR);

  // 2. 打印调试信息，验证地址和栈顶值
  LOG_DEBUG("read app stack top：%lx\r\n", app_stack_top);
  // 3. 校验栈顶指针是否在合法的RAM范围内
  if (app_stack_top >= 0x20000000 && app_stack_top <= 0x20020000)
  {
    // 定义函数指针类型
    typedef void (*pFunction)(void);
    // 4. 获取应用程序复位中断服务程序地址（地址偏移4处）
    pFunction JumpToApplication = (pFunction)*(uint32_t *)(APP_START_ADDR+4);
    // 5. 关闭中断
    __disable_irq();
    // 6. 清理Bootloader相关资源
    BootLoader_Clear_optimized();
    // 7. 切换主堆栈指针(MSP)到应用程序栈顶
    __set_MSP(app_stack_top);
    // 8. 重定向中断向量表到应用程序区域
    SCB->VTOR = APP_START_ADDR;
    // 9. 再次清理资源（此时已使用App栈，安全）
    BootLoader_Clear_optimized();
    // 10. 执行跳转，进入应用程序
    JumpToApplication();
  }
  else
  {
    LOG_ERROR("read app stack top error no jump");
  }
}

/**
 * @brief  OTA状态管理器，根据当前OTA状态执行相应操作
 * @note   该函数从EEPROM读取OTA状态，并根据状态执行不同操作：
 *         1. NO_APP_UPDATE: 无更新状态，检查按键，接收新固件，备份旧固件，更新固件
 *         2. APP_DOWNLOADING: 下载中状态，处理下载失败情况，重新下载固件
 *         3. APP_DOWNLOAD_COMPLETE: 下载完成状态，处理固件解密、备份和更新
 * @param  无
 * @retval 无
 */
void OTA_StateManager(void)
{
  // 1. 初始化OTA状态和相关变量
  uint8_t ota_state = NO_APP_UPDATE;      // OTA状态：无更新、下载中、下载完成
  uint32_t new_fl_size = 0;               // 新固件大小
  uint32_t old_fl_size = 0;               // 旧固件大小
  OTA_Status_t ret;                       // OTA操作返回状态

  // 2. 从EEPROM读取OTA状态
  ota_state = AT24C02_ReadOneByte(&at24c02_iic,EEPROM_OTA_STATE_ADDR);
  LOG_DEBUG("read ota state：%d\r\n", ota_state);
  switch (ota_state)
  {
    case NO_APP_UPDATE:
      if (!KeyScan())
      {
        JumpToApp();
      }
      // 2 通过Ymodem协议接收新固件到外部Flash的A区
      ret = Ymodem_Receive_App(APP_START_ADDR);
  }
}