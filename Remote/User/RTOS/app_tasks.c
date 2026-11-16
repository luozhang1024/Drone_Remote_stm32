#include "app_rtos.h"
#include "app_tasks.h"
#include "adc.h"
#include "timing_trigger.h"
#include "sendpacket.h"
#include "pair_freq.h"
#include "nrf24l01.h"
#include "key.h"
#include "led.h"

volatile uint32_t g_slow_count;
volatile uint32_t g_adc_sample_count;
volatile uint32_t g_adc_received_count;
volatile uint32_t g_adc_dropped_count;
volatile uint32_t g_adc_stale_count;
volatile uint32_t g_adc_timeout_count;
volatile uint32_t g_adc_max_age_ticks;
volatile uint32_t g_radio_test_delay_ms;
AdcFrame g_last_adc_frame;

static void PublishUiSnapshot(uint8_t sampling_fault)
{
    UiSnapshot snapshot = {0};
    snapshot.pair_step = (uint8_t)pair.step;
    /* 本函数和无线收发在同一任务内执行，不会读到一半被另一任务改写。 */
    snapshot.online = NrfGetAckState(&snapshot.unlocked, &snapshot.low_battery);
    snapshot.sampling_fault = sampling_fault;
    snapshot.tx_ok = g_radio_tx_ok;
    snapshot.tx_fail = g_radio_tx_fail;
    xQueueOverwrite(g_ui_queue, &snapshot);
}

void AppRadioTask(void *argument)
{
    AdcFrame frame;
    KeyState keys;
    uint8_t packet[TX_PLOAD_WIDTH];
    uint8_t result;
    uint32_t previous_sequence = 0U;
    TickType_t age;
    (void)argument;

    /* 到这里调度器已运行，队列也已创建，才允许 DMA 中断唤醒任务。
     * main 仅配置 TIM4，没有启动；因此 ADC 现在还不会被触发。 */
    DMA_ClearITPendingBit(DMA1_IT_GL1);
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    timing_trigger_start();

    for (;;) {
        /* 没有采样时任务进入 Blocked，让其他任务运行，不空转等数据。 */
        if (xQueueReceive(g_adc_queue, &frame, pdMS_TO_TICKS(100)) != pdPASS) {
            ++g_adc_timeout_count;
            NrfInvalidateRx();
            PublishUiSnapshot(1U);
            LedColorSet(YELLOW);
            /* 采样停了就不再发送旧油门，等待下一帧新采样。 */
            continue;
        }
        ++g_adc_received_count;
        /* 序号之间缺了几帧，就说明任务忙时队列覆盖了几帧。
         * 无符号减法也适用于 sequence 自然回绕。 */
        g_adc_dropped_count += (uint32_t)(frame.sequence - previous_sequence - 1U);
        previous_sequence = frame.sequence;
        g_last_adc_frame = frame; /* 供暂停调试时查看，不供别的任务读取。 */
        age = xTaskGetTickCount() - frame.tick;
        if (age > g_adc_max_age_ticks) g_adc_max_age_ticks = age;
        if (age >= pdMS_TO_TICKS(100)) {
            ++g_adc_stale_count;
            NrfInvalidateRx();
            PublishUiSnapshot(1U);
            LedColorSet(YELLOW);
            continue;
        }

        /* 第 4 关先在这里采按键；第 5 关再把这行移到独立 KeyTask。
         * 按键队列已经创建，本关尚无独立生产者，下面通常会立即返回空。
         * 后续 KeyTask 发布消息后，无线任务就能用新键值覆盖临时扫描结果。 */
        key_info();
        if (xQueueReceive(g_key_queue, &keys, 0) == pdPASS) {
            key.l = keys.left;
            key.r = keys.right;
        }
        analyze_packet(frame.adc);  /* 原始 ADC 数值转换成协议中的油门/方向。 */
        WaitPairing();              /* 只有这个任务修改配对状态和无线寄存器。 */
        data_exchange(packet);     /* 每次按最新配对状态重新组包。 */
        result = NRF24L01_TxPacket(packet); /* 复用第 0 关已有的有界超时驱动。 */
        g_radio_last_result = result;
        if (result == TX_OK) {
            ++g_radio_tx_ok;
            LedColorSet(GREEN);
        } else {
            ++g_radio_tx_fail;
            if (result == NRF24L01_TIMEOUT) ++g_radio_tx_timeout;
            LedBlink(YELLOW);
        }
        /* 本关只发布快照，OLED 显示交给第 5 关的 UiTask。 */
        PublishUiSnapshot(0U);
        if (g_radio_test_delay_ms != 0U) {
            vTaskDelay(pdMS_TO_TICKS(g_radio_test_delay_ms));
        }
    }
}

void AppSlowTask(void *argument)
{
    (void)argument;
    for (;;) {
        ++g_slow_count; /* 约每秒增加 10，断开飞机后也应持续增长。 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
