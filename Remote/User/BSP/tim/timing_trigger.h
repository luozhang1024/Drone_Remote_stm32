#ifndef _timer_h_
#define _timer_h_

#include "stm32f10x.h"


void timing_trigger_init(void);  /* 配置，不启动。 */
void timing_trigger_start(void); /* 调度器运行后再启动采样。 */
    
#endif

