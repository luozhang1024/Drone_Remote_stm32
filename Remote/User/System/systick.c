#include "systick.h"

#define APP_DWT_CTRL   (*((volatile uint32_t *)0xE0001000UL))
#define APP_DWT_CYCLES (*((volatile uint32_t *)0xE0001004UL))

__IO uint32_t g_system_tick_ms = 0U;

void SysTick_init(void)
{
    /* 仅在调度前初始化；启动调度器后，SysTick 由内核接管。 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    APP_DWT_CTRL |= 1UL; /* 无线超时也用这个计数器，只开启，不能清零。 */
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        for (;;) { }
    }
}

uint32_t GetTick(void)
{
    return g_system_tick_ms;
}

/* 仅用于开机初始化；任务用 vTaskDelay，禁止在中断里毫秒忙等。 */
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
    /* 每段最多等 1000 微秒，避免乘法溢出；无符号相减可处理计数回绕。 */
    while (us != 0U) {
        chunk = (us > 1000U) ? 1000U : us;
        cycles = chunk * cycles_per_us;
        start = APP_DWT_CYCLES;
        while ((uint32_t)(APP_DWT_CYCLES - start) < cycles) { }
        us -= chunk;
    }
}
