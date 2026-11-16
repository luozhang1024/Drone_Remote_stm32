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
// mode=0：一次按下只报告一次；mode!=0：按住时持续报告稳定键值。
// 请周期调用：用时间差确认按键稳定，不再 Delay_ms() 占着 CPU 等待。
uint8_t KeyScan(uint8_t mode)
{
    static uint8_t candidate, stable, reported;
    static uint32_t changed_at;
    uint8_t raw = (key_left == 0) ? 1U : ((key_right == 0) ? 2U : 0U);
    uint32_t now = GetTick();
    if (raw != candidate) {
        candidate = raw;       // 电平刚变化，先记下，暂不认定是真的按下。
        changed_at = now;
    }
    if ((uint32_t)(now - changed_at) >= 10U) stable = candidate;
    if (stable == 0U) {
        reported = 0U;         // 稳定松开后，允许下一次单击。
    } else if (mode || !reported) {
        reported = 1U;
        return stable;         // 1 是左键，2 是右键，同时按优先左键。
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












