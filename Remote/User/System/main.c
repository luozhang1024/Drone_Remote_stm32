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

int main(void)
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
    for (attempt = 0U; attempt < 3U; ++attempt) {
        if (NRF24L01_Check() == 0U) break;
        Delay_ms(10);
    }
    if (attempt == 3U) printf("NRF24L01 unavailable; continuing with bounded TX errors\r\n");
    NRF24L01_TX_Mode();
    KeyInit();
    ADC_Config();            /* ADC/DMA 已准备好，但还没有定时触发。 */
    timing_trigger_init();   /* 这里只配置 TIM4，真正启动放在 RadioTask。 */
    /* 先创建队列/任务，再启动调度器，最后才让采样中断产生消息。 */
    AppRtosStart();
    for (;;) { }
}
