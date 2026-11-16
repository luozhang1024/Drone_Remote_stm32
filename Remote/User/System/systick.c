#include "systick.h"

#define APP_DWT_CTRL   (*((volatile uint32_t *)0xE0001000UL))
#define APP_DWT_CYCLES (*((volatile uint32_t *)0xE0001004UL))

__IO uint32_t g_system_tick_ms = 0U;

void SysTick_init(void)
{
    /* Startup only; the FreeRTOS port configures SysTick at scheduler start. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    APP_DWT_CTRL |= 1UL; /* Shared with radio timeouts: never reset CYCCNT. */
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        for (;;) { }
    }
}

uint32_t GetTick(void)
{
    return g_system_tick_ms;
}

/* Startup only. Tasks use vTaskDelay/vTaskDelayUntil; never call from ISR. */
void Delay_ms(uint32_t ms)
{
    uint32_t start = GetTick();
    while ((uint32_t)(GetTick() - start) < ms) { }
}

void Delay_us(uint32_t us)
{
    uint32_t cycles_per_us = (SystemCoreClock + 999999U) / 1000000U;
    uint32_t chunk, start, cycles;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    APP_DWT_CTRL |= 1UL;
    /* Bound intervals/multiplication; unsigned subtraction handles wrap. */
    while (us != 0U) {
        chunk = (us > 1000U) ? 1000U : us;
        cycles = chunk * cycles_per_us;
        start = APP_DWT_CYCLES;
        while ((uint32_t)(APP_DWT_CYCLES - start) < cycles) { }
        us -= chunk;
    }
}
