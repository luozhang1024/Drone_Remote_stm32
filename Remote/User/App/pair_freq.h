#ifndef _pair_freq_h_
#define _pair_freq_h_
#include "stm32f10x.h"
#define CHIP_ID_ADRESS  ((uint32_t)0x1FFFF7E8U)
typedef enum{
	NOT = 0,
	STEP1,
	DONE,
}PairStep;

typedef struct
{
	PairStep step;
	uint8_t addr[5];
	uint8_t freq_channel;
}Pair;

/* 配对状态只由 RadioTask 修改，其他任务应读取 UI 快照。 */
extern Pair pair;
void WaitPairing(void);
void get_chip_id(void);


#endif

