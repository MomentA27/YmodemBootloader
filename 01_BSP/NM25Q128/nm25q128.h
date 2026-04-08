//
// Created by 35540 on 2026/4/8.
//

#ifndef YMODEMBOOTLOADER_NM25Q128_H
#define YMODEMBOOTLOADER_NM25Q128_H
//******************************** Includes *********************************//
#include "main.h"
#ifdef MYDEBUG
#include "elog.h"
#endif
//******************************** Includes *********************************//
//---------------------------------------------------------------------------//
//******************************** Defines **********************************//
#define NM25Q128_DEBUG                             // 调试
#define W25Q128_PAGE_PROG          (0x02)          //页编程指令
#define W25Q128_RD_DATA            (0x03)          //读数据指令
#define W25Q128_RD_SR1             (0x05)          //读状态寄存器 1 指令  return是否忙、写保护是否开启
#define W25Q128_WR_ENABEL          (0x06)          //写使能指令
#define W25Q128_ERASE_4k           (0x20)          //扇区擦除指令
#define W25Q_BLOCK_ERASE_64K       (0xD8)          //块擦除（64KB）
#define W25Q_CHIP_ERASE            (0xC7)          //整片擦除（约20s）
#define W25Q128_RD_ID              (0x9F)          //读 ID 指令

#define W25Q128_PAGE_SIZE       256     // 页大小（字节）
#define W25Q128_SECTOR_SIZE     4096    // 扇区大小（4KB）
#define W25Q128_BLOCK_SIZE_64K  65536   // 块大小（64KB）
#define W25Q128_TOTAL_SIZE      0x1000000  // 总容量（16MB = 1024*1024*16）
#define W25Q128_MAX_ADDR        (W25Q128_TOTAL_SIZE - 1)  // 最大地址（0x000000~0x0FFFFF）

#define NM25Q128_SPI hspi1
#define NM25Q128_ENABLE() HAL_GPIO_WritePin(NM25Q128_CS_GPIO_Port, NM25Q128_CS_Pin, GPIO_PIN_RESET)
#define NM25Q128_DISABLE() HAL_GPIO_WritePin(NM25Q128_CS_GPIO_Port, NM25Q128_CS_Pin, GPIO_PIN_SET)
#define NM25Q128_Wait_Time 200
//******************************** Defines **********************************//
//---------------------------------------------------------------------------//
//******************************** Typedefs *********************************//
typedef enum {
  W25Q_OK                 = 0,
  W25Q_ERROR              = 1,
  W25Q_ADDR_OUT_OF_RANGE  = 2,  // 地址超出范围
  W25Q_DATA_LEN_ERROR     = 3,     // 数据长度错误
  W25Q_SPI_COMM_ERROR     = 4,     // SPI通信错误
  W25Q_TIMEOUT_ERROR      = 5       // 操作超时错误
} W25Q_StatusTypeDef;
//******************************** Typedefs *********************************//
//---------------------------------------------------------------------------//
//**************************** Interface Structs ****************************//
//**************************** Interface Structs ****************************//
//---------------------------------------------------------------------------//
//******************************** Classes **********************************//
//******************************** Classes **********************************//
//---------------------------------------------------------------------------//
//**************************** Extern Variables *****************************//
//**************************** Extern Variables *****************************//
//---------------------------------------------------------------------------//
//******************************** 函数声明 **********************************//
void W25Q128_NSS_Init(void);
W25Q_StatusTypeDef W25Q128_ReadJEDECID(uint8_t *manufacturer_id, uint16_t *device_id);
W25Q_StatusTypeDef W25Q128_WriteEnable(void);
W25Q_StatusTypeDef W25Q128_WaitBusy(uint32_t timeout_ms);
W25Q_StatusTypeDef W25Q128_SectorErase(uint32_t addr);
W25Q_StatusTypeDef W25Q128_BlockErase64K(uint32_t addr);
W25Q_StatusTypeDef W25Q128_ChipErase(void);
W25Q_StatusTypeDef W25Q128_PageProgram(uint32_t addr, const uint8_t *pData, uint16_t len);
W25Q_StatusTypeDef W25Q128_ReadData(uint32_t addr, uint8_t *pData, uint32_t len);
void W25Q128_Test(void);
//******************************** 函数声明 **********************************//
#endif //YMODEMBOOTLOADER_NM25Q128_H