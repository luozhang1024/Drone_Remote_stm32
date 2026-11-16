#include "nvic.h"

void NVIC_config(void)
{
    NVIC_InitTypeDef irq;
    /* 四位都用于抢占优先级；整个程序只在这里配置一次分组。 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    irq.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    irq.NVIC_IRQChannelPreemptionPriority = 6;
    irq.NVIC_IRQChannelSubPriority = 0;
    /* 这里只配置优先级；RadioTask 就绪后再打开 DMA 中断。 */
    irq.NVIC_IRQChannelCmd = DISABLE;
    NVIC_Init(&irq);
}
