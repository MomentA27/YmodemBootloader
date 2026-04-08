//
// Created by 35540 on 2026/4/8.
//
//******************************** Includes *********************************//
#include "nm25q128.h"

#include <sys/types.h>

#include "spi.h"
//******************************** Includes *********************************//
//---------------------------------------------------------------------------//
//******************************** Defines **********************************//
#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG       "NM25Q128"
#else
#define LOG_TAG       "NM25Q128"
#endif

// 直接除以 1000，无需自己定义任何主频宏
#define DWT_CYCLES_PER_MS  (SystemCoreClock / 1000)
//******************************** Defines **********************************//
//---------------------------------------------------------------------------//
//******************************** Variables *********************************//
//******************************** Variables ********************************//
//---------------------------------------------------------------------------//
//******************************** Macros ***********************************//

#if defined(NM25Q128_DEBUG) && defined(MYDEBUG)
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
 * @brief 初始化W25Q16
 * @details 此函数用于初始化W25Q16（配置CS引脚），将片选信号拉高，使芯片处于未选中状态
 * @param 无
 * @return 无返回值
 * @note 应在其他SPI操作之前调用此函数进行初始化
 */
void W25Q128_NSS_Init(void)
{
  NM25Q128_DISABLE(); // 默认拉高CS，不选中
  LOG_DEBUG("NM25Q128 initialized");
}

/**
 * @brief 等待W25Q128忙标志结束
 * @details 循环读取状态寄存器1的BUSY位，直到芯片完成当前操作或超时
 * @param timeout_ms 超时时间，单位为毫秒
 * @return W25Q_StatusTypeDef 枚举类型
 *         - W25Q_OK: 操作成功，BUSY位已清零
 *         - W25Q_SPI_COMM_ERROR: SPI通信失败
 *         - W25Q_TIMEOUT_ERROR: 等待超时
 * @note
 *       - 使用DWT周期计数器实现精确超时控制，避免HAL_GetTick()的低精度问题
 *       - 该函数会自动处理CS片选信号的拉低和拉高
 *       - 超时时间应根据具体操作设置：擦除操作建议500ms以上，写操作建议10ms以上
 * @warning 超时参数不宜设置过小，否则可能导致操作未完成就退出等待
 */
W25Q_StatusTypeDef W25Q128_WaitBusy(uint32_t timeout_ms)
{
  uint8_t status_reg1 = 0;
  uint8_t cmd = W25Q128_RD_SR1;

  // 1. 将超时时间(ms) 转换为 DWT 的周期数
  uint32_t timeout_cycles = timeout_ms * DWT_CYCLES_PER_MS;
  uint32_t start_cycle = DWT->CYCCNT; // 记录起始周期

  NM25Q128_ENABLE();

  // 2. 只发送一次"读状态寄存器"指令
  if (HAL_SPI_Transmit(&NM25Q128_SPI, &cmd, 1, 100) != HAL_OK) {
    NM25Q128_DISABLE();
    return W25Q_SPI_COMM_ERROR;
  }

  // 3. 循环接收状态，直到 BUSY 位清零或超时
  do {
    if (HAL_SPI_Receive(&NM25Q128_SPI, &status_reg1, 1, 100) != HAL_OK) {
      NM25Q128_DISABLE();
      return W25Q_SPI_COMM_ERROR;
    }
    // DWT 无符号减法，天然抗 25.5 秒溢出Bug
    if ((DWT->CYCCNT - start_cycle) > timeout_cycles) {
      NM25Q128_DISABLE();
      return W25Q_TIMEOUT_ERROR;
    }
  } while ((status_reg1 & 0x01) != 0);

  NM25Q128_DISABLE();
  return W25Q_OK;
}

/**
 * @brief 使能W25Q128写操作
 * @details 发送写使能命令(0x06)，设置状态寄存器中的WEL(Write Enable Latch)位
 * @param 无
 * @return W25Q_StatusTypeDef 枚举类型
 *         - W25Q_OK: 写使能成功
 *         - W25Q_SPI_COMM_ERROR: SPI通信失败
 * @note
 *       - 每次执行编程(Program)或擦除(Erase)操作前必须先调用此函数
 *       - 写使能在一次编程或擦除操作后会自动清除
 *       - 函数内部会先等待芯片空闲再发送写使能命令
 * @warning 如果连续多次调用而未执行实际写操作，WEL位可能保持设置状态
 */
W25Q_StatusTypeDef W25Q128_WriteEnable(void)
{
  uint8_t cmd = W25Q128_WR_ENABEL;
  W25Q_StatusTypeDef wait_status = W25Q128_WaitBusy(NM25Q128_Wait_Time);
  if (wait_status != W25Q_OK) return wait_status;

  NM25Q128_ENABLE();
  W25Q_StatusTypeDef ret = (HAL_SPI_Transmit(&NM25Q128_SPI, &cmd, 1, 100) == HAL_OK) ? W25Q_OK : W25Q_SPI_COMM_ERROR;
  NM25Q128_DISABLE();

  return ret;
}

/**
 * @brief 读取W25Q128的JEDEC ID
 * @details 读取芯片的制造商ID和设备ID，用于验证芯片型号和通信是否正常
 * @param[out] manufacturer_id 指向存储制造商ID的变量指针(8位)
 * @param[out] device_id 指向存储设备ID的变量指针(16位)
 * @return W25Q_StatusTypeDef 枚举类型
 *         - W25Q_OK: 读取成功
 *         - W25Q_ERROR: 参数为空指针
 *         - W25Q_SPI_COMM_ERROR: SPI通信失败
 * @note
 *       - JEDEC ID由3字节组成：1字节制造商ID + 2字节设备ID
 *       - 可用于检测芯片是否正确连接以及型号是否匹配
 *       - 典型值：制造商ID=0xEF(Winbond), 设备ID=0x4018(W25Q128)
 * @warning 调用前必须确保传入有效的指针，否则会返回W25Q_ERROR
 */
W25Q_StatusTypeDef W25Q128_ReadJEDECID(uint8_t *manufacturer_id, uint16_t *device_id)
{
  uint8_t id_buf[3] = {0};
  uint8_t cmd = W25Q128_RD_ID;

  if (manufacturer_id == NULL || device_id == NULL) return W25Q_ERROR;

  NM25Q128_ENABLE();
  if (HAL_SPI_Transmit(&NM25Q128_SPI, &cmd, 1, 100) != HAL_OK) {
    LOG_ERROR("transmit read id error");
    NM25Q128_DISABLE();
    return W25Q_SPI_COMM_ERROR;
  }
  if (HAL_SPI_Receive(&NM25Q128_SPI, id_buf, 3, 100) != HAL_OK) {
    LOG_ERROR("receive read id error");
    NM25Q128_DISABLE();
    return W25Q_SPI_COMM_ERROR;
  }
  NM25Q128_DISABLE();

  *manufacturer_id = id_buf[0];
  *device_id = (id_buf[1] << 8) | id_buf[2];
  LOG_DEBUG("W25Q128 ID: %02X %04X", *manufacturer_id, *device_id);
  return W25Q_OK;
}

/**
 * @brief 擦除W25Q128的一个扇区(4KB)
 * @details 擦除指定地址所在的4KB扇区，将该扇区所有数据置为0xFF
 * @param addr 要擦除的扇区起始地址(必须是4KB对齐)
 * @return W25Q_StatusTypeDef 枚举类型
 *         - W25Q_OK: 擦除成功
 *         - W25Q_ADDR_OUT_OF_RANGE: 地址超出范围或未对齐
 *         - W25Q_ERROR: 写使能失败
 *         - W25Q_SPI_COMM_ERROR: SPI通信失败
 * @note
 *       - 地址必须是4096字节(0x1000)的倍数，即最低12位必须为0
 *       - 擦除操作会将整个扇区的数据全部设置为0xFF
 *       - 函数内部自动执行写使能和等待忙操作
 *       - 典型擦除时间：40-200ms
 * @warning
 *       - 擦除是不可逆操作，会丢失扇区内所有数据
 *       - 地址不对齐会导致返回错误，不会执行擦除
 *       - 最大地址为W25Q128_MAX_ADDR(16MB-1)
 */
W25Q_StatusTypeDef W25Q128_SectorErase(uint32_t addr)
{
  if (addr > W25Q128_MAX_ADDR || (addr % W25Q128_SECTOR_SIZE) != 0)
  {
    LOG_ERROR("erase addr error");
    return W25Q_ADDR_OUT_OF_RANGE;
  }
  if (W25Q128_WriteEnable() != W25Q_OK)
  {
    LOG_ERROR("enable write error");
    return W25Q_ERROR;
  }

  // 统一风格：指令 + 24位地址，拼接成 4 字节包
  uint8_t tx_buf[4] = { W25Q128_ERASE_4k, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF };

  NM25Q128_ENABLE();
  W25Q_StatusTypeDef ret = (HAL_SPI_Transmit(&NM25Q128_SPI, tx_buf, 4, 100) == HAL_OK) ? W25Q_OK : W25Q_SPI_COMM_ERROR;
  NM25Q128_DISABLE();

  return (ret == W25Q_OK) ? W25Q128_WaitBusy(500) : ret;
}

/**
 * @brief 擦除W25Q128的一个块(64KB)
 * @details 擦除指定地址所在的64KB块，将该块所有数据置为0xFF
 * @param addr 要擦除的块起始地址(必须是64KB对齐)
 * @return W25Q_StatusTypeDef 枚举类型
 *         - W25Q_OK: 擦除成功
 *         - W25Q_ADDR_OUT_OF_RANGE: 地址超出范围或未对齐
 *         - W25Q_ERROR: 写使能失败
 *         - W25Q_SPI_COMM_ERROR: SPI通信失败
 * @note
 *       - 地址必须是65536字节(0x10000)的倍数，即最低16位必须为0
 *       - 比扇区擦除效率高，适合大范围数据擦除
 *       - 函数内部自动执行写使能和等待忙操作
 *       - 典型擦除时间：1-3秒
 * @warning
 *       - 擦除是不可逆操作，会丢失块内所有数据
 *       - 地址不对齐会导致返回错误，不会执行擦除
 *       - 擦除时间较长，timeout设置需足够大(建议2000ms以上)
 */
W25Q_StatusTypeDef W25Q128_BlockErase64K(uint32_t addr)
{
  if (addr > W25Q128_MAX_ADDR || (addr % W25Q128_BLOCK_SIZE_64K) != 0) return W25Q_ADDR_OUT_OF_RANGE;
  if (W25Q128_WriteEnable() != W25Q_OK) return W25Q_ERROR;

  // 统一风格：指令 + 24位地址，拼接成 4 字节包
  uint8_t tx_buf[4] = { W25Q_BLOCK_ERASE_64K, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF };

  NM25Q128_ENABLE();
  W25Q_StatusTypeDef ret = (HAL_SPI_Transmit(&NM25Q128_SPI, tx_buf, 4, 100) == HAL_OK) ? W25Q_OK : W25Q_SPI_COMM_ERROR;
  NM25Q128_DISABLE();

  return (ret == W25Q_OK) ? W25Q128_WaitBusy(2000) : ret;
}

/**
 * @brief 擦除W25Q128整颗芯片
 * @details 擦除整个Flash芯片的所有数据，将所有存储单元置为0xFF
 * @param 无
 * @return W25Q_StatusTypeDef 枚举类型
 *         - W25Q_OK: 擦除成功
 *         - W25Q_ERROR: 写使能失败
 *         - W25Q_SPI_COMM_ERROR: SPI通信失败
 * @note
 *       - 这是最耗时的擦除操作，会清空整个16MB存储空间
 *       - 函数内部自动执行写使能和等待忙操作
 *       - 典型擦除时间：15-30秒(取决于芯片型号)
 *       - 适用于完全重置Flash内容的场景
 * @warning
 *       - 此操作不可逆，会丢失所有数据，使用前务必确认
 *       - 等待时间很长，timeout设置需足够大(建议25000ms以上)
 *       - 建议在用户明确确认后才执行此操作
 */
W25Q_StatusTypeDef W25Q128_ChipErase(void)
{
  if (W25Q128_WriteEnable() != W25Q_OK) return W25Q_ERROR;

  uint8_t cmd = W25Q_CHIP_ERASE;

  NM25Q128_ENABLE();
  W25Q_StatusTypeDef ret = (HAL_SPI_Transmit(&NM25Q128_SPI, &cmd, 1, 100) == HAL_OK) ? W25Q_OK : W25Q_SPI_COMM_ERROR;
  NM25Q128_DISABLE();

  return (ret == W25Q_OK) ? W25Q128_WaitBusy(25000) : ret;
}

/**
 * @brief 向W25Q128写入一页数据(最多256字节)
 * @details 将数据编程到指定的地址，支持单次最多256字节的页编程
 * @param addr 写入的起始地址
 * @param pData 指向要写入数据的指针
 * @param len 要写入的数据长度(字节数)
 * @return W25Q_StatusTypeDef 枚举类型
 *         - W25Q_OK: 写入成功
 *         - W25Q_ERROR: 参数错误或空指针
 *         - W25Q_DATA_LEN_ERROR: 数据长度超限或跨页
 *         - W25Q_ADDR_OUT_OF_RANGE: 地址超出范围
 *         - W25Q_SPI_COMM_ERROR: SPI通信失败
 * @note
 *       - 单次写入不能超过256字节(W25Q128_PAGE_SIZE)
 *       - 写入不能跨越页边界(256字节边界)
 *       - 只能将1变为0，不能将0变为1(需要先擦除)
 *       - 函数内部自动执行写使能和等待忙操作
 *       - 典型编程时间：0.4-3ms
 * @warning
 *       - 如果写入区域未预先擦除，可能导致数据错误
 *       - 跨页写入会被拒绝，需要分多次调用
 *       - 地址必须在有效范围内(0 ~ W25Q128_MAX_ADDR)
 */
W25Q_StatusTypeDef W25Q128_PageProgram(uint32_t addr, const uint8_t *pData, uint16_t len)
{
  if (addr > W25Q128_MAX_ADDR || pData == NULL || len == 0 || len > W25Q128_PAGE_SIZE) {
    return (len > W25Q128_PAGE_SIZE) ? W25Q_DATA_LEN_ERROR : W25Q_ERROR;
  }
  if ((addr & 0xFF) + len > W25Q128_PAGE_SIZE) return W25Q_DATA_LEN_ERROR;
  if (W25Q128_WriteEnable() != W25Q_OK) return W25Q_ERROR;

  uint8_t header[4] = { W25Q128_PAGE_PROG, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF };

  NM25Q128_ENABLE();
  if (HAL_SPI_Transmit(&NM25Q128_SPI, header, 4, 100) != HAL_OK) {
    NM25Q128_DISABLE(); return W25Q_SPI_COMM_ERROR;
  }
  if (HAL_SPI_Transmit(&NM25Q128_SPI, (uint8_t *)pData, len, 100) != HAL_OK) {
    NM25Q128_DISABLE(); return W25Q_SPI_COMM_ERROR;
  }
  NM25Q128_DISABLE();

  return W25Q128_WaitBusy(10);
}

/**
 * @brief 从W25Q128读取数据
 * @details 从指定地址读取任意长度的数据
 * @param addr 读取的起始地址
 * @param pData 指向存储读取数据的缓冲区指针
 * @param len 要读取的数据长度(字节数)
 * @return W25Q_StatusTypeDef 枚举类型
 *         - W25Q_OK: 读取成功
 *         - W25Q_ADDR_OUT_OF_RANGE: 地址超出范围或参数错误
 *         - W25Q_SPI_COMM_ERROR: SPI通信失败
 * @note
 *       - 支持任意长度和任意地址的读取操作
 *       - 读取速度较快，不受页边界限制
 *       - 可以连续读取整个地址空间的数据
 *       - 典型读取时钟频率可达50MHz
 * @warning
 *       - 必须确保pData指向的缓冲区有足够空间容纳len字节数据
 *       - 读取地址+长度不能超过芯片最大地址范围
 *       - 对于大数据量读取，注意缓冲区大小避免栈溢出
 */
W25Q_StatusTypeDef W25Q128_ReadData(uint32_t addr, uint8_t *pData, uint32_t len)
{
  if (addr > W25Q128_MAX_ADDR || pData == NULL || len == 0 || (addr + len - 1) > W25Q128_MAX_ADDR) {
    return W25Q_ADDR_OUT_OF_RANGE;
  }

  uint8_t header[4] = { W25Q128_RD_DATA, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF };

  NM25Q128_ENABLE();
  if (HAL_SPI_Transmit(&NM25Q128_SPI, header, 4, 100) != HAL_OK) {
    NM25Q128_DISABLE(); return W25Q_SPI_COMM_ERROR;
  }
  if (HAL_SPI_Receive(&NM25Q128_SPI, pData, len, 1000) != HAL_OK) {
    NM25Q128_DISABLE(); return W25Q_SPI_COMM_ERROR;
  }
  NM25Q128_DISABLE();

  return W25Q_OK;
}

/*****===============功能测试函数======================****/
void W25Q128_Test(void) {
  W25Q_StatusTypeDef status;
  uint8_t manu_id = 0;
  uint16_t dev_id = 0;
  uint32_t addr = 0x00;
  // 2. 初始化W25Q128
  W25Q128_NSS_Init();

  // 3. 读JEDEC ID，验证通信
  status = W25Q128_ReadJEDECID(&manu_id, &dev_id);
  if (status == W25Q_OK && manu_id == 0xEF && dev_id == 0x4018) {
    LOG_DEBUG("spi flash id success");
  } else {
    LOG_ERROR("spi flash id error");
    return;
  }

  LOG_DEBUG("erase shanks ing");
  status = W25Q128_SectorErase(addr); // 擦除 0x00 所在的 4KB 扇区
  if (status != W25Q_OK)
  {
    LOG_ERROR("erase shanks error!");
    return;
  }

  uint8_t write_buffer[10] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA};
  uint8_t read_buffer[10] = {0};
  LOG_DEBUG("page write ing");
  status = W25Q128_PageProgram(addr,write_buffer,sizeof(write_buffer));
  if (status != W25Q_OK)
  {
    LOG_ERROR("page write error!");
    return;
  }
  LOG_DEBUG("page read ing");
   status = W25Q128_ReadData(addr,read_buffer,sizeof(read_buffer));
  if (status != W25Q_OK)
  {
    LOG_ERROR("page read error!");
    return;
  }

  LOG_DEBUG("read and write success");
  // 6. ⚠️ 关键步骤：逐字节对比数据，验证结果
  for (uint8_t i = 0; i < sizeof(write_buffer); i++)
  {
    if (write_buffer[i] != read_buffer[i])
    {
      LOG_ERROR("read and write error!");
      break;
    }
  }
}