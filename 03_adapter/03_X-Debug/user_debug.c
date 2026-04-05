//
// Created by capting on 2025/10/28.
//

#include "user_debug.h"


/*********************************************************************
*
*       Global functions
*
**********************************************************************
*/
void SEGGER_SYSVIEW_Conf(void) {
  // 假设 CPU 频率为 72MHz，且使用 CPU 频率作为时间戳源
#define SYSVIEW_TIMESTAMP_FREQ  168000000u   // 时间戳频率 (Hz)
#define SYSVIEW_CPU_FREQ        168000000u   // CPU 频率 (Hz)
#define SYSVIEW_RAM_BASE        0x20000000u // 静态缓冲区基地址（若使用）

  // 初始化 SystemView
  SEGGER_SYSVIEW_Init(SYSVIEW_TIMESTAMP_FREQ, SYSVIEW_CPU_FREQ, NULL, NULL);

  // 如果启用了静态缓冲区，设置基地址
#ifdef SEGGER_SYSVIEW_USE_STATIC_BUFFER
  SEGGER_SYSVIEW_SetRAMBase(SYSVIEW_RAM_BASE);
#endif
}
void user_debug_init(void)
{
  SEGGER_RTT_Init();
  SEGGER_SYSVIEW_Conf();
  ElogErrCode ret = elog_init();
  if (ELOG_NO_ERR != ret) SEGGER_RTT_printf(0,"elog is ng");
  elog_set_text_color_enabled(true);

  elog_set_fmt(ELOG_LVL_ASSERT ,ELOG_FMT_ALL&~(ELOG_FMT_P_INFO|ELOG_FMT_DIR));
  elog_set_fmt(ELOG_LVL_ERROR  ,ELOG_FMT_ALL&~(ELOG_FMT_P_INFO|ELOG_FMT_DIR));
  elog_set_fmt(ELOG_LVL_WARN   ,ELOG_FMT_ALL&~(ELOG_FMT_P_INFO|ELOG_FMT_DIR));
  elog_set_fmt(ELOG_LVL_INFO   ,ELOG_FMT_ALL&~(ELOG_FMT_P_INFO|ELOG_FMT_DIR));
  elog_set_fmt(ELOG_LVL_DEBUG  ,ELOG_FMT_ALL&~(ELOG_FMT_P_INFO|ELOG_FMT_DIR));
  elog_set_fmt(ELOG_LVL_VERBOSE,(ELOG_FMT_LVL|
                                          ELOG_FMT_TAG));

  elog_start();
  // elog_flush();
}