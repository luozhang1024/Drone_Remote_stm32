#include "app_rtos.h"
#include "app_tasks.h"

volatile uint32_t g_fast_count;
volatile uint32_t g_slow_count;

void AppFastTask(void *argument)
{
    TickType_t last = xTaskGetTickCount();
    (void)argument;
    for (;;) {
        ++g_fast_count;
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
}

void AppSlowTask(void *argument)
{
    (void)argument;
    for (;;) {
        ++g_slow_count;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
