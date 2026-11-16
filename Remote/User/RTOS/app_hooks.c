#include "stm32f10x.h"
#include "app_rtos.h"

const char * volatile g_assert_file;
volatile int g_assert_line;
TaskHandle_t volatile g_overflow_task;
const char * volatile g_overflow_task_name;

void AppAssertFailed(const char *file, int line)
{
    __disable_irq();
    g_assert_file = file;
    g_assert_line = line;
    for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
    AppAssertFailed("malloc", 0);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    __disable_irq();
    g_overflow_task = task;
    g_overflow_task_name = name;
    AppAssertFailed("stack overflow", 0);
}
