#include "stm32f10x.h"
#include "systick.h"
#include "nvic.h"
#include "led.h"
#include "spi.h"
#include "nrf24l01.h"
#include "usart1.h"
#include "key.h"
#include "adc.h"
#include "pair_freq.h"
#include "timing_trigger.h"
#include "app_rtos.h"

/* main 只创建启动任务，业务任务由它统一创建。 */
static TaskHandle_t AppTaskCreate_Handle = NULL;
static void BSP_Init(void);

int main(void)
{
    BaseType_t xReturn = pdPASS;
    /* 第一步：初始化外设，但先不启动 TIM4，避免 DMA 中断来得太早。 */
    BSP_Init();
    /* 第二步：创建“负责创建业务任务”的启动任务。
     * 此时调度器还没有启动，创建成功也不会马上执行 AppTaskCreate。 */
    xReturn = xTaskCreate((TaskFunction_t) AppTaskCreate,       /* 任务入口函数 */
                         (const char *)   "AppTaskCreate",   /* 调试时显示的任务名 */
                         (uint16_t)       512,               /* 栈深度：512 word = 2048 字节 */
                         (void *)         NULL,              /* 不传入额外参数 */
                         (UBaseType_t)    1,                 /* 启动任务优先级 */
                         (TaskHandle_t *) &AppTaskCreate_Handle); /* 接收任务句柄的地址 */
    configASSERT(xReturn == pdPASS);
    /* 第三步：启动调度器，AppTaskCreate 从这里之后才有机会运行。 */
    if (pdPASS == xReturn)
    {
        vTaskStartScheduler();
    }
    /* 正常不会返回；创建失败或调度器启动失败时保留错误现场。 */
    AppAssertFailed("scheduler start failed", 0);
    for (;;) {}
}

/* 和教程的 BSP_Init 一样，集中放置启动前的硬件初始化。 */
static void BSP_Init(void)
{
    uint8_t attempt;
    SystemInit();
    SystemCoreClockUpdate();
    NVIC_config(); /* 统一 Group_4；DMA 中断先禁用。 */
    SysTick_init();
    LedInit();
    USART_Config();
    get_chip_id();
    SPI1_Init();
    NRF24L01_Init();
    /* 模块缺失最多检查三次，不让系统永远卡在开机自检。 */
    for (attempt = 0U; attempt < 3U; ++attempt)
    {
        if (NRF24L01_Check() == 0U) break;
        Delay_ms(10);
    }
    if (attempt == 3U) printf("NRF24L01 unavailable; continuing with bounded TX errors\r\n");
    NRF24L01_TX_Mode();
    KeyInit();
    ADC_Config();            /* ADC/DMA 已准备好，但还没有定时触发。 */
    timing_trigger_init();   /* 这里只配置 TIM4，真正启动放在 RadioTask。 */
    /* 此处不能启动采样：队列要等 AppTaskCreate 运行时才创建。 */
}
