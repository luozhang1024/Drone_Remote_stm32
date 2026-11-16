#include "app_rtos.h"
#include "app_tasks.h"

QueueHandle_t g_adc_queue;
QueueHandle_t g_key_queue;
QueueHandle_t g_ui_queue;
TaskHandle_t g_radio_task;
TaskHandle_t g_slow_task;

void AppRtosStart(void)
{
    BaseType_t result;

    /* 长度为 1：处理不及时就用新消息覆盖旧消息，避免发送过期油门。
     * 创建动作单独写，再检查结果，不能把创建动作藏进断言里。 */
    g_adc_queue = xQueueCreate(1, sizeof(AdcFrame));
    configASSERT(g_adc_queue != NULL);
    g_key_queue = xQueueCreate(1, sizeof(KeyState));
    configASSERT(g_key_queue != NULL);
    g_ui_queue = xQueueCreate(1, sizeof(UiSnapshot));
    configASSERT(g_ui_queue != NULL);

    /* 无线任务涉及组包、SPI，起始栈给 384 word，即 1536 字节。 */
    result = xTaskCreate(AppRadioTask, "radio", 384, NULL, 4, &g_radio_task);
    configASSERT(result == pdPASS);
    /* 保留第 3 关的慢计数任务：无线掉线时也能检查系统是否继续调度。 */
    result = xTaskCreate(AppSlowTask, "slow", 128, NULL, 1, &g_slow_task);
    configASSERT(result == pdPASS);
    vTaskStartScheduler();
    /* 正常不会走到这里；内核任务内存不足等情况会导致启动失败。 */
    AppAssertFailed("scheduler returned", 0);
}
