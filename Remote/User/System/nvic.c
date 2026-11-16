#include "nvic.h"

void NVIC_config(void)
{
    NVIC_InitTypeDef irq;
    /* The application's only priority grouping configuration. */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    irq.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    irq.NVIC_IRQChannelPreemptionPriority = 6;
    irq.NVIC_IRQChannelSubPriority = 0;
    /* Enable in stage 4 only after the receiving objects/tasks exist. */
    irq.NVIC_IRQChannelCmd = DISABLE;
    NVIC_Init(&irq);
}
