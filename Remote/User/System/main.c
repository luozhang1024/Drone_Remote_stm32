#include "stm32f10x.h"
#include "systick.h"
#include "nvic.h"
#include "led.h"
#include "app_rtos.h"

int main(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    NVIC_config();
    SysTick_init();
    LedInit();
    /* Stage 3: ADC/TIM4/DMA, radio, OLED and the old loop remain stopped. */
    AppRtosStart();
    for (;;) { }
}
