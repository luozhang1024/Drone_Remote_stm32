#ifndef APP_RTOS_H
#define APP_RTOS_H
#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t g_fast_task;
extern TaskHandle_t g_slow_task;
extern volatile uint32_t g_fast_count;
extern volatile uint32_t g_slow_count;
extern const char * volatile g_assert_file;
extern volatile int g_assert_line;
extern TaskHandle_t volatile g_overflow_task;
extern const char * volatile g_overflow_task_name;
void AppRtosStart(void);
#endif
