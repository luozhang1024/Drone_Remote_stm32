#ifndef APP_RTOS_H
#define APP_RTOS_H
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* 一条消息就是一整帧副本，不传 DMA 数组的地址。
 * sequence 用来发现覆盖了多少帧；tick 用来判断样本是否已经太旧。 */
typedef struct {
    uint16_t adc[4];
    TickType_t tick;
    uint32_t sequence;
} AdcFrame;

typedef struct {
    uint8_t left;
    uint8_t right;
} KeyState;

/* UI 以后只看这份副本，不直接读取正在变化的无线内部数据。 */
typedef struct {
    uint8_t pair_step;
    uint8_t online;
    uint8_t unlocked;
    uint8_t low_battery;
    uint8_t sampling_fault;
    uint32_t tx_ok;
    uint32_t tx_fail;
} UiSnapshot;

extern QueueHandle_t g_adc_queue;
extern QueueHandle_t g_key_queue;
extern QueueHandle_t g_ui_queue;
extern TaskHandle_t g_radio_task;
extern TaskHandle_t g_slow_task;
extern volatile uint32_t g_slow_count;
extern volatile uint32_t g_adc_sample_count;
extern volatile uint32_t g_adc_received_count;
extern volatile uint32_t g_adc_dropped_count;
extern volatile uint32_t g_adc_stale_count;
extern volatile uint32_t g_adc_timeout_count;
extern volatile uint32_t g_adc_max_age_ticks;
/* 调试时设为 50 可模拟无线任务过慢；默认 0，单位毫秒。 */
extern volatile uint32_t g_radio_test_delay_ms;
extern AdcFrame g_last_adc_frame;
extern const char * volatile g_assert_file;
extern volatile int g_assert_line;
extern TaskHandle_t volatile g_overflow_task;
extern const char * volatile g_overflow_task_name;
void AppRtosStart(void);
#endif
