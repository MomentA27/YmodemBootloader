//
// Created by 35540 on 2026/4/8.
//

#ifndef YMODEMBOOTLOADER_JUMPTOAPP_H
#define YMODEMBOOTLOADER_JUMPTOAPP_H
//******************************** Includes *********************************//
#include "main.h"
#include "at24c02_driver.h"
#include "i2c_bus.h"
#ifdef MYDEBUG
#include "elog.h"
#endif

//******************************** Includes *********************************//
//---------------------------------------------------------------------------//
//******************************** Defines **********************************//
#define JUMPAPP 1
#define APP_START_ADDR      0x08008000
/* EEPROM 地址定义 */
#define EEPROM_OTA_STATE_ADDR         0x00U   /* OTA状态，1字节 */
#define EEPROM_NEW_APP_SIZE_ADDR      0x01U   /* 待更新App大小，4字节 */
#define EEPROM_CURR_APP_SIZE_ADDR     0x05U   /* 当前App大小，4字节 */

#define NO_APP_UPDATE               0x00
#define APP_DOWNLOADING             0x11
#define APP_DOWNLOAD_COMPLETE       0x22


//******************************** Defines **********************************//
//---------------------------------------------------------------------------//
//******************************** Typedefs *********************************//
typedef enum: uint8_t
{
  OTA_OK = 0,                 // 成功
  OTA_ERR_GENERAL,            // 通用错误
  OTA_ERR_SIZE_INVALID,       // 大小无效
  OTA_ERR_ERASE_FAILED,       // 擦除失败
  OTA_ERR_READ_FAILED ,       // 读取失败
  OTA_ERR_WRITE_FAILED,       // 写入失败
  OTA_ERR_AES_DECRYPT,        // AES解密失败
  OTA_ERR_HEADER_INVALID      // 头部无效
} OTA_Status_t;
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
void JumpToApp(void);
void OTA_StateManager(void);
OTA_Status_t ExA_To_ExB_AES(uint32_t *fl_size);
OTA_Status_t App_To_ExA(uint32_t fl_size);
OTA_Status_t ExB_To_App(uint32_t fl_size);
OTA_Status_t ExA_To_App(uint32_t fl_size);
//******************************** 函数声明 **********************************//
#endif //YMODEMBOOTLOADER_JUMPTOAPP_H