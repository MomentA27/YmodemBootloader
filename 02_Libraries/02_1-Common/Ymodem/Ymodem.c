//
// Created by 35540 on 2026/4/8.
//
//******************************** Includes *********************************//
#include "Ymodem.h"

#include <stdlib.h>

#include "nm25q128.h"


//******************************** Includes *********************************//
//---------------------------------------------------------------------------//
//******************************** Defines **********************************//
#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG       "YMODEM"
#else
#define LOG_TAG       "YMODEM"
#endif

//******************************** Defines **********************************//
//---------------------------------------------------------------------------//
//******************************** Variables *********************************//
/** 文件信息结构体 */
static ymodem_file_info_t file_info;
/** 当前数据包序号 */
static uint8_t packet_seq = 0;
/** 已接收文件总字节数 */
static uint32_t total_bytes = 0;

/** 静态数据包缓冲区 (用于跨UART帧的数据包重组或Flash写入前的安全缓冲) */
static uint8_t packet_buf[YMODEM_MAX_PACKET_SIZE];
static uint16_t packet_buf_len = 0;
static uint8_t temp_data_buf[YMODEM_PACKET_SIZE_1024];  // Flash写入期间的安全缓冲区
//******************************** Variables ********************************//
//---------------------------------------------------------------------------//
//******************************** Macros ***********************************//
#if defined(YMODEM) && defined(MYDEBUG)
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
//******************************** Functions ********************************//
/**
 * @brief 计算CRC16校验码 (XMODEM多项式0x1021)
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return CRC16校验值
 */
static uint16_t Ymodem_CRC16(const uint8_t *data, uint16_t len)
{
  uint16_t crc = CRC_INIT;
  uint16_t i, j;

  for (i = 0; i < len; i++)
  {
    crc ^= (uint16_t)data[i] << 8;
    for (j = 0; j < 8; j++)
    {
      if (crc & 0x8000)
        crc = (crc << 1) ^ CRC_POLY;
      else
        crc <<= 1;
    }
  }
  return crc;
}

/**
 * @brief 串口发送单个字节
 * @param byte 要发送的字节
 */
static void uart_send_byte(uint8_t byte)
{
  /*
   * 参数1：UART句柄指针 (假设你用CubeMX生成的串口1句柄是 huart1)
   * 参数2：发送数据缓冲区指针
   * 参数3：发送数据长度
   * 参数4：超时时间。原代码是while死等，所以这里用 HAL_MAX_DELAY 表示无限等待
   */
  HAL_UART_Transmit(&huart1, &byte, 1, HAL_MAX_DELAY);
}

/**
 * @brief 串口接收单个字节 (零拷贝，从UART DMA缓冲区读取)
 * @param data 接收数据指针
 * @param timeout 超时时间(毫秒)
 * @return 0-成功, 1-超时
 */
static uint8_t uart_recv_byte(uint8_t *data, uint32_t timeout)
{
  uart_buf_t *frame;
  uint32_t start_time = DWT->CYCCNT;

  while (1)
  {
    // 超时检查
    if (DWT_TIMEOUT_CHECK(start_time, timeout * 1000))
    {
      LOG_DEBUG("Recv timeout, %u ms", timeout);
      return 1;
    }

    // 尝试获取UART DMA缓冲区
    frame = uart1_rx_get_read_buf();
    if (frame != NULL)
    {
      // 零拷贝：直接从DMA缓冲区读取
      *data = frame->buf[0];

      // 更新缓冲区指针和长度
      frame->buf++;
      frame->len--;

      // 如果缓冲区已空，释放
      if (frame->len == 0)
      {
        uart1_rx_release_read_buf();
      }

      LOG_DEBUG("Recv: 0x%02X", *data);
      return 0;
    }

    DWT_Delay_us(10);
  }
}

/**
 * @brief 接收一个完整的Ymodem数据包 (零拷贝设计)
 * @param packet 数据包结构体指针
 * @return >=0: 数据包序号, -2:EOT, -3:CAN, -1:失败
 */
static int16_t recv_packet(ymodem_packet_t *packet)
{
    uint8_t status;
    uint8_t retry = 0;
    uint16_t data_size;
    uint16_t calc_crc, recv_crc;
    uint8_t *data_ptr = NULL;

    memset(packet, 0, sizeof(ymodem_packet_t));

    // 接收包头
    while (retry < YMODEM_MAX_RETRY)
    {
        status = uart_recv_byte(&packet->header, YMODEM_RX_TIMEOUT_MS);
        if (status != 0)
        {
            retry++;
            LOG_DEBUG("Header retry %d", retry);
            continue;
        }

        // 检查包头类型
        if (packet->header == SOH)
        {
            data_size = YMODEM_PACKET_SIZE_128;
            break;
        }
        else if (packet->header == STX)
        {
            data_size = YMODEM_PACKET_SIZE_1024;
            break;
        }
        else if (packet->header == EOT)
        {
            LOG_DEBUG("Recv EOT");
            return -2;
        }
        else if (packet->header == CAN)
        {
            LOG_ERROR("Recv CAN");
            return -3;
        }
        else
        {
            // 无效包头，增加重试计数并继续
            LOG_DEBUG("Invalid header: 0x%02X, retry", packet->header);
            retry++;
            if (retry >= YMODEM_MAX_RETRY)
            {
                LOG_ERROR("Header max retry");
                return -1;
            }
            uart_send_byte(NAK);  // 请求重传
        }
    }

    if (retry >= YMODEM_MAX_RETRY)
    {
        LOG_ERROR("Header timeout");
        return -1;
    }

    // 接收序号和反序号
    status = uart_recv_byte(&packet->seq_num, YMODEM_RX_TIMEOUT_MS);
    if (status != 0)
    {
        LOG_ERROR("Seq num timeout");
        return -1;
    }

    status = uart_recv_byte(&packet->seq_num_compl, YMODEM_RX_TIMEOUT_MS);
    if (status != 0)
    {
        LOG_ERROR("Seq num compl timeout");
        return -1;
    }

    // 验证序号
    if ((packet->seq_num + packet->seq_num_compl) != 0xFF)
    {
        LOG_ERROR("Seq check failed: %d + %d != 0xFF",
                  packet->seq_num, packet->seq_num_compl);
        return -1;
    }

    packet->data_len = data_size;

    // 零拷贝接收数据：直接从UART DMA缓冲区读取
    // 优先尝试使用UART DMA缓冲区指针，如果跨帧则使用静态缓冲区
    data_ptr = NULL;
    packet_buf_len = 0;

    for (uint16_t i = 0; i < data_size; )
    {
        uart_buf_t *frame = uart1_rx_get_read_buf();

        if (frame != NULL)
        {
            // 当前UART帧有数据
            uint16_t avail = frame->len;      // 当前帧中【可供读取】的剩余字节数
            uint16_t need = data_size - i;    // 还需要读取的字节数

            if (avail >= need)
            {
                // 当前帧数据足够，直接使用DMA缓冲区指针 (零拷贝)
                if (i == 0)
                {
                    // 数据包起始就在DMA缓冲区中
                    data_ptr = frame->buf;

                    // 更新frame状态
                    frame->buf += need;
                    frame->len -= need;

                    if (frame->len == 0)
                    {
                        uart1_rx_release_read_buf();
                    }
                }
                else
                {

                    memcpy(packet_buf + packet_buf_len, frame->buf, need);
                    data_ptr = packet_buf;

                    frame->buf += need;
                    frame->len -= need;

                    if (frame->len == 0)
                    {
                        uart1_rx_release_read_buf();
                    }
                }

                i += need;
            }
            else
            {
                // 当前帧数据不足，拷贝到静态缓冲区
                memcpy(packet_buf + packet_buf_len, frame->buf, avail);
                packet_buf_len += avail;
                data_ptr = packet_buf;

                uart1_rx_release_read_buf();
                i += avail;
            }
        }
        else
        {
            // 等待更多数据
            status = uart_recv_byte(&packet_buf[packet_buf_len], YMODEM_RX_TIMEOUT_MS);
            if (status != 0)
            {
                LOG_ERROR("Data timeout at %d", i);
                return -1;
            }
            packet_buf_len++;
            data_ptr = packet_buf;
            i++;
        }
    }

    packet->data = data_ptr;

    // 接收CRC16
    uint8_t crc_hi, crc_lo;
    status = uart_recv_byte(&crc_hi, YMODEM_RX_TIMEOUT_MS);
    if (status != 0)
    {
        LOG_ERROR("CRC hi timeout");
        return -1;
    }

    status = uart_recv_byte(&crc_lo, YMODEM_RX_TIMEOUT_MS);
    if (status != 0)
    {
        LOG_ERROR("CRC lo timeout");
        return -1;
    }
    recv_crc = ((uint16_t)crc_hi << 8) | crc_lo;

    // 计算并验证CRC16 (零拷贝，直接校验DMA或静态缓冲区)
    calc_crc = Ymodem_CRC16(packet->data, data_size);
    if (calc_crc != recv_crc)
    {
        LOG_ERROR("CRC fail: calc=0x%04X, recv=0x%04X", calc_crc, recv_crc);
        return -1;
    }

    LOG_DEBUG("Packet OK: seq=%d, len=%d, CRC=0x%04X",
              packet->seq_num, data_size, recv_crc);

    return packet->seq_num;
}

/**
 * @brief 解析文件头信息包 (序号0)
 * @param packet 数据包结构体指针
 * @return 0-成功, 非0-失败
 */
static uint8_t parse_header(ymodem_packet_t *packet)
{
    char *p;
    uint32_t file_size = 0;

    memset(&file_info, 0, sizeof(ymodem_file_info_t));

    // 提取文件名
    strncpy(file_info.file_name, (char *)packet->data, YMODEM_FILE_NAME_MAX_LEN - 1);
    file_info.file_name[YMODEM_FILE_NAME_MAX_LEN - 1] = '\0';

    LOG_DEBUG("File: %s", file_info.file_name);

    // 提取文件大小
    p = (char *)packet->data + strlen(file_info.file_name) + 1;
    if (p < (char *)packet->data + packet->data_len)
    {
        file_size = atol(p);
        file_info.file_size = file_size;
        LOG_DEBUG("Size: %u bytes", file_size);
    }
    else
    {
        LOG_ERROR("No size info");
        return 1;
    }

    return 0;
}

/**
 * @brief 处理数据包，直接写入Flash (零拷贝)
 * @param packet 数据包结构体指针
 * @param file_addr 当前文件写入地址
 * @param bytes_written 本次写入的字节数 (输出参数)
 * @return 0-成功, 非0-失败
 */
static uint8_t process_data(ymodem_packet_t *packet, uint32_t file_addr, uint32_t *bytes_written)
{
    uint32_t write_len;
    W25Q_StatusTypeDef status;
    uint8_t *write_ptr;

    // 计算需要写入的字节数 (避免文件末尾的填充0)
    if (total_bytes + packet->data_len > file_info.file_size)
    {
        write_len = file_info.file_size - total_bytes;
        LOG_DEBUG("Truncate: %d -> %u", packet->data_len, write_len);
    }
    else
    {
        write_len = packet->data_len;
    }

    // 关键安全检查：如果packet->data指向潜在的DMA缓冲区（指向packet_buf以外），
    // 则先拷贝到静态缓冲区，避免Flash写入期间DMA缓冲区被覆盖
    if (packet->data != packet_buf)
    {
        write_ptr = temp_data_buf;
        memcpy(write_ptr, packet->data, write_len);
        LOG_DEBUG("Copy to safe buffer for Flash write");
    }
    else
    {
        write_ptr = packet->data;  // packet->data已经是packet_buf，安全
    }

    // Flash扇区对齐擦除
    if (file_addr % 0x1000 == 0)
    {
        W25Q128_SectorErase(file_addr);
    }

    // 零拷贝（或安全拷贝后）写入Flash
    status = W25Q128_WriteData(file_addr, write_ptr, write_len);
    if (status != W25Q_OK)
    {
        LOG_ERROR("Flash write fail at 0x%08X", file_addr);
        return 1;
    }

    *bytes_written = write_len;
    total_bytes += write_len;

    LOG_DEBUG("Write: addr=0x%08X, len=%u, total=%u",
              file_addr, write_len, total_bytes);

    return 0;
}

/**
 * @brief 获取文件信息结构体指针
 * @return 文件信息结构体指针
 */
const ymodem_file_info_t* Ymodem_GetFileInfo(void)
{
    return &file_info;
}


/**
 * @brief Ymodem协议接收入口函数 (零拷贝架构)
 * @param base_addr Flash写入基地址 (需预先擦除)
 * @param max_size Flash最大可写入字节数
 * @return 接收到的文件大小(字节)，失败返回-1
 */
int32_t Ymodem_Receive(uint32_t base_addr, uint32_t max_size)
{
    ymodem_state_t state = YMODEM_STATE_INIT;
    ymodem_packet_t packet;
    int16_t seq_num;
    uint8_t status;
    uint8_t retry = 0;
    uint32_t file_addr = base_addr;
    uint32_t bytes_written;

    // 初始化
    packet_seq = 0;
    total_bytes = 0;
    memset(&file_info, 0, sizeof(file_info));

    LOG_DEBUG("Ymodem start: addr=0x%08X, max=%u", base_addr, max_size);

    // 握手：发送 'C' 请求 CRC 模式
    LOG_DEBUG("Send 'C'");
    uint32_t start = DWT->CYCCNT;
    while (uart1_rx_get_read_buf() == NULL)
    {
        if (DWT_TIMEOUT_CHECK(start, 500000))
        {
            start = DWT->CYCCNT;
            uart_send_byte('C');
        }
    }
    LOG_DEBUG("Handshake success");

    // 主循环
    while (1)
    {
        switch (state)
        {
            case YMODEM_STATE_INIT:
                seq_num = recv_packet(&packet);
                if (seq_num == 0)
                {
                    LOG_DEBUG("Header packet OK");
                    status = parse_header(&packet);
                    if (status != 0)
                    {
                        LOG_ERROR("Parse header fail");
                        uart_send_byte(CAN);
                        uart_send_byte(CAN);
                        return -1;
                    }
                    if (file_info.file_size > max_size)
                    {
                        LOG_ERROR("File too large");
                        uart_send_byte(CAN);
                        uart_send_byte(CAN);
                        return -1;
                    }
                    uart_send_byte(ACK);
                    uart_send_byte('C');
                    state = YMODEM_STATE_RX_DATA;
                    packet_seq = 1;
                    retry = 0;
                }
                else if (seq_num == -2)  // 空文件：直接收到EOT
                {
                    LOG_DEBUG("Empty file");
                    uart_send_byte(ACK);
                    return 0;
                }
                else
                {
                    retry++;
                    if (retry >= YMODEM_MAX_RETRY)
                    {
                        LOG_ERROR("Header max retry");
                        uart_send_byte(CAN);
                        uart_send_byte(CAN);
                        return -1;
                    }
                    else
                        uart_send_byte(NAK);
                }
                break;

            case YMODEM_STATE_RX_DATA:
                seq_num = recv_packet(&packet);
                if (seq_num == packet_seq)
                {
                    LOG_DEBUG("Data packet %d", seq_num);
                    status = process_data(&packet, file_addr, &bytes_written);
                    if (status != 0)
                    {
                        LOG_ERROR("Process data fail");
                        uart_send_byte(CAN);
                        uart_send_byte(CAN);
                        return -1;
                    }
                    uart_send_byte(ACK);
                    file_addr += bytes_written;
                    packet_seq = (packet_seq + 1) % 256;
                    retry = 0;
                }
                else if (seq_num == -2)  // 收到EOT，直接结束
                {
                    LOG_DEBUG("EOT received, transfer complete");
                    uart_send_byte(ACK);      // 回复ACK确认EOT
                    return total_bytes;       // 直接返回，不等待空包
                }
                else if (seq_num == (packet_seq - 1))
                {
                    LOG_DEBUG("Duplicate packet %d", seq_num);
                    uart_send_byte(ACK);
                }
                else
                {
                    retry++;
                    if (retry >= YMODEM_MAX_RETRY)
                    {
                        LOG_ERROR("Data max retry");
                        uart_send_byte(CAN);
                        uart_send_byte(CAN);
                        return -1;
                    }
                    else
                        uart_send_byte(NAK);
                }
                break;

            case YMODEM_STATE_ABORT:
                LOG_ERROR("Ymodem abort");
                return -1;

            default:
                LOG_ERROR("Invalid state: %d", state);
                return -1;
        }
    }
}