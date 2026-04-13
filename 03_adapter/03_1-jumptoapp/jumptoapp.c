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
static void BootLoader_Clear_optimized(void);
//******************************** 函数声明   *********************************//
//---------------------------------------------------------------------------//
//******************************** Variables ********************************//
unsigned char IV[16]={
    0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32
    };
unsigned char Key[32]={
    0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,
    0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32
    };

uint8_t  g_MemReadbuffer[4096];
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
        AT24C_ReadOneByte(EEPROM_OTA_STATE_ADDR, &ota_state);

        // 3. 根据OTA状态执行不同操作
        switch (ota_state)
        {
        case NO_APP_UPDATE:
            // 1 检查是否有按键按下，若无则直接跳转到应用程序
            if (!KeScan())
            {
                JumpToApp();
            }

            // 2 通过Ymodem协议接收新固件到外部Flash的A区
            new_fl_size = Ymodem_Receive(BLOCK_A_START_ADDR, MAX_APP_SIZE);
            if (new_fl_size <= 0)
            {
                LOG_ERROR("Ymodem receive failed");
                JumpToApp();
            }
            // 3 将A区的加密固件解密并写入B区
            ret = ExA_To_ExB_AES(&new_fl_size);
            if (ret != OTA_OK)
            {
                LOG_ERROR("Boot dowload failed");
                JumpToApp();
            }

            // 4 从EEPROM读取当前应用大小，并将当前应用备份到A区
            AT24C_ReadBytes(EEPROM_CURR_APP_SIZE_ADDR, (uint8_t *)&old_fl_size, 4);
            ret = App_To_ExA(old_fl_size);
            // ret = App_To_ExA(17824);  //因为是第一次下载,当前appsize直接手动写入了
            if (ret != OTA_OK)
            {
                LOG_ERROR("app back up error");
                JumpToApp();
            }

            // 5 将B区的新固件写入内部Flash并跳转到应用
            ExB_To_App(new_fl_size);
            AT24C_WriteBytes(EEPROM_CURR_APP_SIZE_ADDR, (uint8_t *)&new_fl_size, 4);
            JumpToApp();

            ExA_To_App(old_fl_size);
            AT24C_WriteBytes(EEPROM_CURR_APP_SIZE_ADDR, (uint8_t *)&old_fl_size, 4);
            JumpToApp();


            break;

        case APP_DOWNLOADING:
            // 1 重新通过Ymodem协议接收固件
            new_fl_size = Ymodem_Receive(BLOCK_A_START_ADDR, MAX_APP_SIZE);
            if (new_fl_size <= 0)
            {
                LOG_ERROR("Boot dowload failed");
                JumpToApp();
            }

            // 2 解密固件并写入B区
            ret = ExA_To_ExB_AES(&new_fl_size);
            if (ret != OTA_OK)
            {
                LOG_ERROR("Boot dowload failed");
                JumpToApp();
            }

            // 3 将B区的新固件写入内部Flash并跳转到应用
            ExB_To_App(new_fl_size);
            AT24C_WriteBytes(EEPROM_CURR_APP_SIZE_ADDR, (uint8_t *)&new_fl_size, 4);
            JumpToApp();

            break;

        case APP_DOWNLOAD_COMPLETE:
            // 1 从EEPROM读取新固件大小
            AT24C_ReadBytes(EEPROM_NEW_APP_SIZE_ADDR, (uint8_t *)&new_fl_size, 4);
            if (new_fl_size == 0 || new_fl_size > MAX_APP_SIZE)
            {
                LOG_ERROR("Invalid new app size");
                JumpToApp();
            }

            // 2 解密A区的固件并写入B区
            ret = ExA_To_ExB_AES(&new_fl_size);
            if (ret != OTA_OK)
            {
                LOG_ERROR("Boot dowload failed");
                JumpToApp();
            }

            // 3 从EEPROM读取当前应用大小，并将当前应用备份到A区
            AT24C_ReadBytes(EEPROM_CURR_APP_SIZE_ADDR, (uint8_t *)&old_fl_size, 4);
            ret = App_To_ExA(old_fl_size);
            if (ret != OTA_OK)
            {
                LOG_ERROR("app back up error");
                JumpToApp();
            }

            // 4 将B区的新固件写入内部Flash并跳转到应用
            ExB_To_App(new_fl_size);
            AT24C_WriteBytes(EEPROM_CURR_APP_SIZE_ADDR, (uint8_t *)&new_fl_size, 4);
            JumpToApp();

            break;
        }
    }

/**
 * @brief  将外部Flash A区的加密固件解密并写入B区 (适配 .mxxx 格式)
 * @note   协议格式: [4字节自定义Len][自定义Data][4字节固件Len][固件Data] -> AES加密
 * @param  fl_size: 预期的固件大小（用于前置校验）
 * @retval OTA_Status_t: OTA操作状态
 *//**
 * @brief  将外部Flash A区的加密固件解密并写入B区
 * @note   该函数执行以下操作：
 *         1. 从A区读取固件大小并验证
 *         2. 循环读取A区的加密数据
 *         3. 使用AES-256算法解密数据
 *         4. 将解密后的明文写入B区
 *         5. 处理数据填充和扇区擦除
 * @param  fl_size: 固件大小（字节）
 * @retval OTA_Status_t: OTA操作状态
 *         - OTA_OK: 操作成功
 *         - OTA_ERR_SIZE_INVALID: 固件大小无效
 *         - OTA_ERR_READ_FAILED: 读取失败
 *         - OTA_ERR_WRITE_FAILED: 写入失败
 */
OTA_Status_t ExA_To_ExB_AES(uint32_t *fl_size)
{
    // 1. 初始化变量
    uint32_t AppSize = 0;                   // 从A区读取的实际应用大小
    uint32_t total_read_enc = 0;            // 已读取的密文字节数
    uint32_t total_written = 0;             // 已写入B区的明文字节数
    w25q_status_t status;                   // W25Q16操作状态

    // 2. 初始化AES解密参数
    uint8_t *pu8_IV_IN_OUT = IV;            // AES初始化向量
    uint8_t *pu8_key256bit = Key;           // AES-256位密钥

    // 3. 检查固件大小是否有效
    if (*fl_size == 0 || *fl_size >  MAX_APP_SIZE-1)
    {
        LOG_ERROR("fl_size error");
        return OTA_ERR_SIZE_INVALID;
    }

    // 4 从A区读取固件真实大小
    // 4.1 从A区读取16字节的固件加密信息
    status = W25Q16_ReadData(BLOCK_A_START_ADDR,g_MemReadbuffer, 16);
    if (status != W25Q_OK)
    {
        LOG_ERROR("w25q read error");
        return OTA_ERR_READ_FAILED;
    }
    // 4.2 解析固件信息
    Aes_IV_key256bit_Decode(pu8_IV_IN_OUT,g_MemReadbuffer, pu8_key256bit);
    AppSize = *(uint32_t *)(g_MemReadbuffer + 12);
    if (AppSize+16 > *fl_size)
    {
        LOG_ERROR("fl_size error");
        return OTA_ERR_SIZE_INVALID;
    }
    *fl_size = AppSize;

    // 5. 计算需要读取的总字节数（按16字节对齐）
    uint32_t enc_total = (AppSize + 15) / 16 * 16;

    // 6. 循环读取、解密并写入数据
    while (total_read_enc < enc_total)
    {
        // 6.1 计算本次读取的长度（最大4096字节）
        uint32_t remain_enc = enc_total - total_read_enc;
        uint32_t read_len = (remain_enc > 4096) ? 4096 : remain_enc;

        // 6.2 从A区读取加密数据
        status = W25Q16_ReadData(BLOCK_A_START_ADDR+16+total_read_enc,
            g_MemReadbuffer, read_len);
        if (status != W25Q_OK)
        {
            LOG_ERROR("w25q read error");
            return OTA_ERR_READ_FAILED;
        }

        // 6.3 对数据进行AES解密（16字节一块）
        for (uint32_t i = 0; i < read_len; i += 16)
        {
            Aes_IV_key256bit_Decode(pu8_IV_IN_OUT,
                g_MemReadbuffer + i, pu8_key256bit);
        }

        // 6.4 计算本次应写入B区的明文字节数（注意最后一次可能包含填充）
        uint32_t write_len = read_len;
        if (total_read_enc + read_len > AppSize) {
            write_len = AppSize - total_read_enc;   // 去除填充
        }

        // 6.5 扇区擦除（修正优先级）
        uint32_t b_addr = BLOCK_B_START_ADDR + total_written;
        if ((b_addr & 0xFFF) == 0) {   // 4KB对齐
            W25Q16_SectorErase(b_addr);
        }

        // 6.6 写入B区（只写入有效明文）
        status = W25Q16_WriteData(b_addr, g_MemReadbuffer, write_len);
        if (status != W25Q_OK) {
            LOG_ERROR("Write failed at 0x%08X", b_addr);
            return OTA_ERR_WRITE_FAILED;
        }

        // 6.7 更新已读取和已写入的字节数
        total_read_enc += read_len;
        total_written += write_len;
    }
    return OTA_OK;
}

/**
 * @brief  将内部Flash中的应用程序备份到外部Flash的A区
 * @note   该函数将内部Flash中的应用程序复制到外部Flash的A区，
 *         用于固件更新失败时的恢复
 * @param  fl_size: 固件大小（字节）
 * @retval OTA_Status_t: OTA操作状态
 *         - OTA_OK: 操作成功
 *         - OTA_ERR_SIZE_INVALID: 固件大小无效
 *         - OTA_ERR_WRITE_FAILED: 写入失败
 */
OTA_Status_t App_To_ExA(uint32_t fl_size)
{
    w25q_status_t status;  // W25Q16操作状态

    // 1. 检查固件大小是否有效
    if (fl_size == 0 || fl_size >  MAX_APP_SIZE-1)
    {
        LOG_ERROR("fl_size error");
        return OTA_ERR_SIZE_INVALID;
    }

    // 2. 将内部Flash中的应用程序写入外部Flash的A区
    status = W25Q16_WriteData(BLOCK_A_START_ADDR,
        (uint8_t*)APP_START_ADDR, fl_size);
    if (status != W25Q_OK)
    {
        LOG_ERROR("Write failed at 0x%08lX", (unsigned long)BLOCK_A_START_ADDR);
        return OTA_ERR_WRITE_FAILED;
    }
    return OTA_OK;
}

/**
 * @brief  将外部Flash B区的固件写入内部Flash
 * @note   该函数执行以下操作：
 *         1. 擦除内部Flash的A区
 *         2. 从外部Flash B区读取数据
 *         3. 将数据写入内部Flash
 * @param  fl_size: 固件大小（字节）
 * @retval OTA_Status_t: OTA操作状态
 *         - OTA_OK: 操作成功
 *         - OTA_ERR_SIZE_INVALID: 固件大小无效
 *         - OTA_ERR_ERASE_FAILED: 擦除失败
 *         - OTA_ERR_READ_FAILED: 读取失败
 *         - OTA_ERR_WRITE_FAILED: 写入失败
 */
OTA_Status_t ExB_To_App(uint32_t fl_size)
{
    flash_status_t flash_status;  // 内部Flash操作状态
    w25q_status_t w25q_status;    // W25Q16操作状态
    uint32_t total_read_enc = 0;  // 已读取的字节数

    // 1. 检查固件大小是否有效
    if (fl_size == 0 || fl_size >  MAX_APP_SIZE-1)
    {
        LOG_ERROR("fl_size error");
        return OTA_ERR_SIZE_INVALID;
    }

    // 2. 擦除内部Flash的A区
    flash_status = flash_parta_erase();
    if (flash_status != FLASH_OK)
    {
        LOG_ERROR("flash erase failed");
        return OTA_ERR_ERASE_FAILED;
    }

    // 3. 循环读取并写入数据
    while (total_read_enc < fl_size)
    {
        // 4.1 计算本次读取的长度（最大4096字节，且不超过剩余大小）
        uint32_t remain_enc = fl_size - total_read_enc;
        uint32_t read_len = (remain_enc > 4096) ? 4096 : remain_enc;

        // 4.2 从外部Flash的B区读取数据
        w25q_status = W25Q16_ReadData(BLOCK_B_START_ADDR+total_read_enc,
            g_MemReadbuffer, read_len);
        if (w25q_status != W25Q_OK)
        {
            LOG_ERROR("w25q read error");
            return OTA_ERR_READ_FAILED;
        }

        // 4.3 将数据写入内部Flash（按字写入，需要对齐）
        uint32_t write_words = read_len / 4;
        for (uint32_t i = 0; i < write_words; i++)
        {
            flash_status = flash_program_word(APP_START_ADDR+total_read_enc+i*4,
                    (uint32_t*)(g_MemReadbuffer+i*4), 1);
            if (flash_status != FLASH_OK)
            {
                LOG_ERROR("flash progarm error");
                return OTA_ERR_WRITE_FAILED;
            }
        }
        // 处理剩余字节（如果有）
        uint32_t remaining = read_len % 4;
        if (remaining > 0)
        {
            flash_status = flash_program_word(APP_START_ADDR+total_read_enc+write_words*4,
                    (uint32_t*)(g_MemReadbuffer+write_words*4), 1);
            if (flash_status != FLASH_OK)
            {
                LOG_ERROR("flash progarm error");
                return OTA_ERR_WRITE_FAILED;
            }
        }

        // 4.4 更新已读取的字节数
        total_read_enc += read_len;
    }
    return OTA_OK;
}

/**
 * @brief  将外部Flash A区的固件写入内部Flash
 * @note   该函数执行以下操作：
 *         1. 擦除内部Flash的A区
 *         2. 从外部Flash A区读取数据
 *         3. 将数据写入内部Flash
 *         该函数通常用于恢复备份的旧固件
 * @param  fl_size: 固件大小（字节）
 * @retval OTA_Status_t: OTA操作状态
 *         - OTA_OK: 操作成功
 *         - OTA_ERR_SIZE_INVALID: 固件大小无效
 *         - OTA_ERR_ERASE_FAILED: 擦除失败
 *         - OTA_ERR_READ_FAILED: 读取失败
 *         - OTA_ERR_WRITE_FAILED: 写入失败
 */
OTA_Status_t ExA_To_App(uint32_t fl_size)
{
    flash_status_t flash_status;  // 内部Flash操作状态
    w25q_status_t w25q_status;    // W25Q16操作状态
    uint32_t total_read_enc = 0;  // 已读取的字节数

    // 1. 检查固件大小是否有效
    if (fl_size == 0 || fl_size >  MAX_APP_SIZE-1)
    {
        LOG_ERROR("fl_size error");
        return OTA_ERR_SIZE_INVALID;
    }

    // 2. 擦除内部Flash的A区
    flash_status = flash_parta_erase();
    if (flash_status != FLASH_OK)
    {
        LOG_ERROR("flash erase failed");
        return OTA_ERR_ERASE_FAILED;
    }

    // 3. 循环读取并写入数据（只读取有效数据，不读取padding）
    while (total_read_enc < fl_size)
    {
        // 3.1 计算本次读取的长度（最大4096字节，且不超过剩余大小）
        uint32_t remain_enc = fl_size - total_read_enc;
        uint32_t read_len = (remain_enc > 4096) ? 4096 : remain_enc;

        // 3.2 从外部Flash的A区读取数据
        w25q_status = W25Q16_ReadData(BLOCK_A_START_ADDR+total_read_enc,
            g_MemReadbuffer, read_len);
        if (w25q_status != W25Q_OK)
        {
            LOG_ERROR("w25q read error");
            return OTA_ERR_READ_FAILED;
        }

        // 3.3 将数据写入内部Flash（按字写入，需要对齐）
        uint32_t write_words = read_len / 4;
        for (uint32_t i = 0; i < write_words; i++)
        {
            flash_status = flash_program_word(APP_START_ADDR+total_read_enc+i*4,
                    (uint32_t*)(g_MemReadbuffer+i*4), 1);
            if (flash_status != FLASH_OK)
            {
                LOG_ERROR("flash progarm error");
                return OTA_ERR_WRITE_FAILED;
            }
        }
        // 处理剩余字节（如果有）
        uint32_t remaining = read_len % 4;
        if (remaining > 0)
        {
            flash_status = flash_program_word(APP_START_ADDR+total_read_enc+write_words*4,
                    (uint32_t*)(g_MemReadbuffer+write_words*4), 1);
            if (flash_status != FLASH_OK)
            {
                LOG_ERROR("flash progarm error");
                return OTA_ERR_WRITE_FAILED;
            }
        }

        // 3.4 更新已读取的字节数
        total_read_enc += read_len;
    }
    return OTA_OK;
}