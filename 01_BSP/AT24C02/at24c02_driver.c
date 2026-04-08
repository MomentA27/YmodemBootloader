//
// Created by 35540 on 2026/4/5.
//
//******************************** Includes *********************************//
#include "at24c02_driver.h"

//******************************** Includes *********************************//
//---------------------------------------------------------------------------//
//******************************** Defines **********************************//
#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG       "AT24C02"
#else
#define LOG_TAG       "AT24C02"
#endif
//******************************** Defines **********************************//
//---------------------------------------------------------------------------//
//******************************** Variables *********************************//
//******************************** Variables ********************************//
//---------------------------------------------------------------------------//
//******************************** Macros ***********************************//
#define AT24C_DEBUG
#if defined(AT24C_DEBUG) && defined(MYDEBUG)
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
i2c_bus_t at24c02_iic = {
  .I2C_SDA_PORT = GPIOB,
  .I2C_SDA_PIN = GPIO_PIN_9,
  .I2C_SCL_PORT = GPIOB,
  .I2C_SCL_PIN = GPIO_PIN_8
};
/**
 * @brief 等待AT24C02内部写周期完成（轮询ACK）
 * @param bus IIC总线句柄
 * @return 0:成功等待（器件就绪） 1:超时错误（可能总线异常或器件损坏）
 */
static uint8_t AT24C02_WaitIdle(const i2c_bus_t *bus)
{
  ASSERT_NOT_NULL(bus);
  uint16_t timeout = 1000; // 最大等待时间，防止死循环

  while (timeout--) {
    I2CStart(bus);
    I2CSendByte(bus, AT24C02_DEV_ADDR << 1); // 发送写地址

    // 如果返回 SUCCESS(0)，说明器件回应了ACK，内部写入完成
    if (I2CWaitAck(bus) == SUCCESS) {
      I2CStop(bus);
      return 0;
    }
    // 没回应ACK，说明还在写，稍微延时后继续发Start轮询
    DWT_Delay_us(100); // 注意：这里直接用你 i2c_bus.c 里定义的 delay_us 指针
  }
  I2CStop(bus);
  // 【调试日志】记录等待超时错误
  LOG_ERROR("Wait Idle Timeout! Device may be busy or disconnected.");
  return 1;
}

/**
 * @brief 初始化AT24C02（实际上就是初始化它挂载的IIC总线）
 */
uint8_t AT24C02_Init(const i2c_bus_t *bus) {
  ASSERT_NOT_NULL(bus);
  LOG_DEBUG("Initializing I2C bus for AT24C02...");
  I2CInit(bus);
  DWT_Delay_Init();
  return 0;
}

/**
 * @brief 读取单个字节
 */
uint8_t AT24C02_ReadOneByte(const i2c_bus_t *bus, uint8_t addr) {
  ASSERT_NOT_NULL(bus);
  // 你原来的 I2C_Read_One_Byte 已经完美实现了 随机读 时序，直接复用
  uint8_t data = I2C_Read_One_Byte(bus, AT24C02_DEV_ADDR, addr & AT24C02_ADDR_MASK);

  // 【调试日志】打印读取动作和结果 (注意：频繁读取时日志会很多，调试完建议关闭宏)
  LOG_DEBUG("Read Addr:0x%02X, Data:0x%02X", addr, data);
  return data;
}

/**
 * @brief 写入单个字节
 */
uint8_t AT24C02_WriteByte(const i2c_bus_t *bus, uint8_t addr, uint8_t data)
{
  ASSERT_NOT_NULL(bus);

  LOG_DEBUG("Write Addr:0x%02X, Data:0x%02X", addr, data);

  uint8_t res = I2C_Write_One_Byte(bus, AT24C02_DEV_ADDR, addr & AT24C02_ADDR_MASK, data);
  if (res == 0)
  {
    return AT24C02_WaitIdle(bus); // 内部如果超时会自动打 ERROR 日志
  }

  LOG_ERROR("Success to write byte via I2C bus.");
  return 1;
}
/**
 * @brief 连续读取多个字节
 * @note 读取不需要考虑页翻转问题，底层会自动地址+1，到了255之后会自动回滚到0
 */
uint8_t AT24C02_ReadMulti(const i2c_bus_t *bus, uint8_t addr, uint8_t *buf, uint16_t len) {
  ASSERT_NOT_NULL(bus);
  ASSERT_NOT_NULL(buf); // 检查缓冲区指针

  if (len == 0) return 0;

  LOG_DEBUG("Read Multi - Start Addr:0x%02X, Length:%d", addr, len);

  uint8_t res = I2C_Read_Multi_Byte(bus, AT24C02_DEV_ADDR, addr & AT24C02_ADDR_MASK, len, buf);

  if (res != 0) {
    LOG_ERROR("Read Multi failed at Addr:0x%02X", addr);
  }
  return res;
}

/**
 * @brief 连续写入多个字节（带页边界处理）
 * @note 核心逻辑：如果写入的数据跨越了8字节页边界，必须分多次写入
 */
uint8_t AT24C02_WriteMulti(const i2c_bus_t *bus, uint8_t addr, uint8_t *buf, uint16_t len) {
  ASSERT_NOT_NULL(bus);
  ASSERT_NOT_NULL(buf); // 检查缓冲区指针

  if (len == 0) return 0;

  LOG_DEBUG("Write Multi - Start Addr:0x%02X, Total Length:%d", addr, len);

  uint16_t i = 0;
  uint8_t page_count = 0; // 记录分了几页，方便调试

  while (len > 0) {
    // 计算当前页剩余空间
    uint8_t page_remain = AT24C02_PAGE_SIZE - (addr % AT24C02_PAGE_SIZE);

    // 决定本次写入长度
    uint8_t write_len = (len < page_remain) ? len : page_remain;

    // 【调试日志】打印分页细节（这个日志在排查跨页覆盖问题时是神器！）
    LOG_DEBUG("  Page %d: Write %d bytes to Addr 0x%02X", page_count, write_len, addr);

    if (I2C_Write_Multi_Byte(bus, AT24C02_DEV_ADDR, addr & AT24C02_ADDR_MASK, write_len, &buf[i]) != 0) {
      LOG_ERROR("I2C write failed at page %d, addr 0x%02X", page_count, addr);
      return 1;
    }

    // 等待这一页写入完成
    if (AT24C02_WaitIdle(bus) != 0) {
      return 1; // 错误已在 WaitIdle 内部打印
    }

    // 更新指针
    addr += write_len;
    i += write_len;
    len -= write_len;
    page_count++;
  }

  LOG_DEBUG("Write Multi completed successfully in %d page(s).", page_count);
  return 0;
}

void AT24C02_Test_Suite(void) {
    uint8_t write_buf[10] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA};
    uint8_t read_buf[10]  = {0};
    uint8_t test_passed = 0;

    LOG_DEBUG("==================== AT24C02 Test Start ====================");

    /*-----------------------------------------------------*/
    /* Test 1: Single byte R/W */
    /*-----------------------------------------------------*/
    LOG_DEBUG("[Test 1] Single byte R/W (Addr 0x00)");
    AT24C02_WriteByte(&at24c02_iic, 0x00, 0x55);
    test_passed = (AT24C02_ReadOneByte(&at24c02_iic, 0x00) == 0x55) ? 1 : 0;

    if (test_passed) { LOG_DEBUG("Test 1: PASS"); }
    else { LOG_ERROR("Test 1: FAIL"); }

    /*-----------------------------------------------------*/
    /* Test 2: Multi byte R/W within page */
    /*-----------------------------------------------------*/
    LOG_DEBUG("[Test 2] Multi byte R/W within page (Addr 0x02, Len 4)");
    AT24C02_WriteMulti(&at24c02_iic, 0x02, write_buf, 4);
    AT24C02_ReadMulti(&at24c02_iic, 0x02, read_buf, 4);
    test_passed = (memcmp(write_buf, read_buf, 4) == 0) ? 1 : 0;

    LOG_DEBUG("Tx: %02X %02X %02X %02X", write_buf[0], write_buf[1], write_buf[2], write_buf[3]);
    LOG_DEBUG("Rx: %02X %02X %02X %02X", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

    if (test_passed) { LOG_DEBUG("Test 2: PASS"); }
    else { LOG_ERROR("Test 2: FAIL"); }

    /*-----------------------------------------------------*/
    /* Test 3: Cross page boundary R/W */
    /*-----------------------------------------------------*/
    LOG_DEBUG("[Test 3] Cross page boundary R/W (Addr 0x06, Len 6)");
    LOG_DEBUG(">>> Check driver page-split logs above <<<");
    AT24C02_WriteMulti(&at24c02_iic, 0x06, write_buf, 6);

    memset(read_buf, 0, 10);
    AT24C02_ReadMulti(&at24c02_iic, 0x06, read_buf, 6);
    test_passed = (memcmp(write_buf, read_buf, 6) == 0) ? 1 : 0;

    LOG_DEBUG("Tx: %02X %02X %02X %02X %02X %02X", write_buf[0], write_buf[1], write_buf[2], write_buf[3], write_buf[4], write_buf[5]);
    LOG_DEBUG("Rx: %02X %02X %02X %02X %02X %02X", read_buf[0], read_buf[1], read_buf[2], read_buf[3], read_buf[4], read_buf[5]);

    if (test_passed) { LOG_DEBUG("Test 3 Cmp: PASS"); }
    else { LOG_ERROR("Test 3 Cmp: FAIL"); }

    // Anti-overwrite check
    uint8_t check_0x00 = AT24C02_ReadOneByte(&at24c02_iic, 0x00);
    if (check_0x00 == 0x55) {
        LOG_DEBUG("Anti-overwrite: Addr 0x00 = 0x%02X, PASS", check_0x00);
    } else {
        LOG_ERROR("Anti-overwrite: Addr 0x00 = 0x%02X, OVERWRITTEN!", check_0x00);
    }

  /*-----------------------------------------------------*/
  /* Test 4: Read address roll-over */
  /*-----------------------------------------------------*/
  LOG_DEBUG("[Test 4] Read address roll-over (Addr 0xFE, Len 4)");
  AT24C02_ReadMulti(&at24c02_iic, 0xFE, read_buf, 4);

  LOG_DEBUG("Rx: %02X %02X %02X %02X", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

  // read_buf[2] is addr 0x00 (must be 0x55).
  // read_buf[0], [1], [3] are unknown dirty data, do not check them.
  test_passed = (read_buf[2] == 0x55) ? 1 : 0;

  if (test_passed) { LOG_DEBUG("Test 4: PASS"); }
  else { LOG_ERROR("Test 4: FAIL"); }

    LOG_DEBUG("==================== AT24C02 Test End ====================");
}