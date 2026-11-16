#include "app_rtos.h"
#include "app_tasks.h"

TaskHandle_t g_fast_task;
TaskHandle_t g_slow_task;

void AppRtosStart(void)
{
    BaseType_t result;
    /* Stack depth: 128 32-bit words = 512 bytes per task. */
    result = xTaskCreate(AppFastTask, "fast", 128, NULL, 2, &g_fast_task);
    configASSERT(result == pdPASS);
    result = xTaskCreate(AppSlowTask, "slow", 128, NULL, 1, &g_slow_task);
    configASSERT(result == pdPASS);
    vTaskStartScheduler();
    AppAssertFailed("scheduler returned", 0);
}
