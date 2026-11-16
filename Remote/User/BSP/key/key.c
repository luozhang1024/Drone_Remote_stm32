#include "key.h"
#include "systick.h"


_KEY key = {0};

void KeyInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;        
    RCC_APB2PeriphClockCmd(key_left_rcc | key_right_rcc, ENABLE);  
    GPIO_InitStructure.GPIO_Pin = key_left_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU  ;          
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(key_left_port, &GPIO_InitStructure);  
    GPIO_InitStructure.GPIO_Pin = key_right_pin;
    GPIO_Init(key_right_port, &GPIO_InitStructure); 
}

//按键扫描
//0：不支持连续按
//1：支持连续按
/* Poll periodically (10 ms suggested); debounce press and release without waits. */
uint8_t KeyScan(uint8_t mode)
{
    static uint8_t candidate, stable, reported;
    static uint32_t changed_at;
    uint8_t raw = (key_left == 0) ? 1U : ((key_right == 0) ? 2U : 0U);
    uint32_t now = GetTick();
    if (raw != candidate) {
        candidate = raw;
        changed_at = now;
    }
    if ((uint32_t)(now - changed_at) >= 10U) stable = candidate;
    if (stable == 0U) {
        reported = 0U;
    } else if (mode || !reported) {
        reported = 1U;
        return stable;
    }
    return 0U;
}

void key_info(void)
{
    key.value = KeyScan(1);
	if(key.value==1)	key.l = 0xE1;
	if(key.value==2)	key.r = 0xC8;
	
	if(!key.value) 
	{
		key.l = 0;
		key.r = 0;
	}
}












