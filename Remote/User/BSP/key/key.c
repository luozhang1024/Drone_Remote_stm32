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
// mode != 0 时：强制重置按键状态，允许下一次按键重新触发
// mode == 0 时：按正常单击状态机工作
uint8_t KeyScan(uint8_t mode)
{
    static uint8_t key_up = 1;   // 1 表示按键已抬起，可检测新按下

    if (mode)
    {
        key_up = 1;              // 强制重置，让下一次按下被重新识别
    }

    // 只有在“松开状态”下，且任意一键按下时，才进入检测
    if (key_up && (key_left == 0 || key_right == 0))
    {
        Delay_ms(5);             // 去抖

        // 这里直接把按键状态改为“按下中”，避免重复触发
        key_up = 0;

        if (key_left == 0)
        {
            return 1;            // 左键按下
        }
        if (key_right == 0)
        {
            return 2;            // 右键按下
        }
    }

    // 两个键都松开时，重新允许下一次按下
    if (key_left == 1 && key_right == 1)
    {
        key_up = 1;
    }

    return 0;
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












