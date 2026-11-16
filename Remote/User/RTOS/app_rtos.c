#include "app_rtos.h"
#include "app_tasks.h"

/**************************** 内核对象与业务任务句柄 ****************************/
QueueHandle_t g_adc_queue = NULL;
QueueHandle_t g_key_queue = NULL;
QueueHandle_t g_ui_queue = NULL;
TaskHandle_t g_radio_task = NULL;
TaskHandle_t g_slow_task = NULL;

/* 对应野火教程的 AppTaskCreate：它本身也是一个任务，只在开机运行一次。
 * 必须使用 void * 参数，和 FreeRTOS 的任务入口类型一致，不用强转掩盖签名差异。 */
void AppTaskCreate(void *pvParameters)
{
    BaseType_t xReturn = pdPASS;
    (void)pvParameters; /* 本任务没有用到传入参数。 */

    /* 和教程一样，在临界区中把队列和任务一次创建完。
     * RadioTask 优先级更高，如果不保护，创建后可能立刻运行并启动采样。
     * 这里只创建少量对象，不做 printf、硬件等待或 vTaskDelay。 */
    taskENTER_CRITICAL();

    /* 队列长度为 1：来不及处理时保留最新帧，避免积压过期油门。
     * sizeof 是一条消息的字节数，队列复制结构体，不只复制地址。 */
    g_adc_queue = xQueueCreate((UBaseType_t) 1,
                              (UBaseType_t) sizeof(AdcFrame));
    configASSERT(g_adc_queue != NULL);
    g_key_queue = xQueueCreate((UBaseType_t) 1,
                              (UBaseType_t) sizeof(KeyState));
    configASSERT(g_key_queue != NULL);
    g_ui_queue = xQueueCreate((UBaseType_t) 1,
                             (UBaseType_t) sizeof(UiSnapshot));
    configASSERT(g_ui_queue != NULL);

    xReturn = xTaskCreate((TaskFunction_t) AppRadioTask, /* 无线任务入口 */
                         (const char *)   "radio",    /* 任务名字 */
                         (uint16_t)       384,        /* 384 word = 1536 字节 */
                         (void *)         NULL,       /* 不传额外参数 */
                         (UBaseType_t)    4,          /* 优先级：高于启动任务 */
                         (TaskHandle_t *) &g_radio_task); /* 保存任务句柄 */
    configASSERT(xReturn == pdPASS);

    /* 保留慢计数任务，观察无线掉线后系统是否还能继续调度。 */
    xReturn = xTaskCreate((TaskFunction_t) AppSlowTask, /* 慢计数任务入口 */
                         (const char *)   "slow",     /* 任务名字 */
                         (uint16_t)       128,        /* 128 word = 512 字节 */
                         (void *)         NULL,       /* 不传额外参数 */
                         (UBaseType_t)    1,          /* 任务优先级 */
                         (TaskHandle_t *) &g_slow_task); /* 保存任务句柄 */
    configASSERT(xReturn == pdPASS);

    /* 全部创建完成后才允许切换任务。退出这里时 RadioTask 可能马上运行，
     * 但它需要的队列已经就绪，因此可以安全开启 DMA 中断和 TIM4。 */
    taskEXIT_CRITICAL();

    /* NULL 表示删除自己。必须先退出临界区，再删除自身，不能依赖删除后
     * 还有机会执行退出临界区语句。启动任务的栈和 TCB 由 Idle 稍后回收。 */
    vTaskDelete(NULL);
}
