#include "adc.h"
#include "systick.h"
#include "led.h"
#include "usart1.h"
#include "nrf24l01.h"

volatile uint16_t ADC_value[4];
/* 将 PA0~PA3 配置为模拟输入，对应 4 路 ADC 采样 */
void adc_gpio_init(void)
{
	GPIO_InitTypeDef GPIO_initStructure;    
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	GPIO_initStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;	    
	GPIO_initStructure.GPIO_Mode = GPIO_Mode_AIN;								    
	GPIO_Init(GPIOA,&GPIO_initStructure);	
}
/* 配置 ADC1:
 * 1. 扫描 4 个规则通道
 * 2. 由 TIM4_CH4 外部触发启动转换
 * 3. 采样结果交给 DMA 自动搬运
 */
void adc_config(void)
{
    ADC_InitTypeDef ADC_initStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div8);  // 72/8=9MHz
	ADC_initStructure.ADC_ContinuousConvMode = DISABLE;		// 单次转换模式，每次由定时器触发开始
	ADC_initStructure.ADC_DataAlign = ADC_DataAlign_Right;	// 数据右对齐
	ADC_initStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T4_CC4; // TIM4_CH4 作为外部触发源
	ADC_initStructure.ADC_Mode = ADC_Mode_Independent;							
	ADC_initStructure.ADC_NbrOfChannel = 4;										// 一次顺序采样 4 个通道
	ADC_initStructure.ADC_ScanConvMode = ENABLE;		                        // 扫描模式
	ADC_Init(ADC1,&ADC_initStructure); 	                                         
	// 配置规则通道顺序和每个通道的采样时间。
	ADC_RegularChannelConfig(ADC1,ADC_Channel_0,1,ADC_SampleTime_71Cycles5);	
	ADC_RegularChannelConfig(ADC1,ADC_Channel_1,2,ADC_SampleTime_71Cycles5);
	ADC_RegularChannelConfig(ADC1,ADC_Channel_2,3,ADC_SampleTime_71Cycles5);
	ADC_RegularChannelConfig(ADC1,ADC_Channel_3,4,ADC_SampleTime_71Cycles5);	
	ADC_Cmd(ADC1,ENABLE);
    Delay_ms(1); /* ADC power-up stabilization before calibration. */
	ADC_ResetCalibration(ADC1);	                                                // 复位校准
	while(ADC_GetResetCalibrationStatus(ADC1));		                                // 等待复位完成
	ADC_StartCalibration(ADC1);					                                // 启动校准
	while(ADC_GetCalibrationStatus(ADC1));		                                // 等待校准完成
	ADC_ExternalTrigConvCmd(ADC1,ENABLE);	                                    // 使能外部触发	
}

/**
 * @brief  配置 ADC1 的 DMA 搬运（DMA1_Channel1 → 内存 ADC_value[]）
 * @note   前提：ADC1 已配置为扫描模式(ScanConvMode=ENABLE)、连续转换，
 *         规则组序列长度为 4（例如 PA0~PA3 四个通道），
 *         ADC_value 定义为 u16 ADC_value[4];（元素类型必须是 u16，
 *         因为下面 DataSize 设的是 HalfWord）
 */
void ADC_DMA_Config(void)
{
    DMA_InitTypeDef DMA_initStructure;
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    /* ---------- 传输数量 ---------- */
    DMA_initStructure.DMA_BufferSize = 4;
    /* ---------- 传输方向 ---------- */
    /* PeripheralSRC = 源是外设(ADC->DR)，目的地是内存。 */
    DMA_initStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    /* 关闭内存到内存模式 */
    DMA_initStructure.DMA_M2M = DMA_M2M_Disable;
    /* ---------- 目标地址 ---------- */
    /* 这里放的是"地址"(32 位地址总线)，不是数据本身。
     * 缓冲区元素类型要与下面 MemoryDataSize 匹配：HalfWord → u16。 */
    DMA_initStructure.DMA_MemoryBaseAddr = (u32)ADC_value;
    /* 每次往内存写 16 bit（HalfWord），因为 ADC 结果就 12/16 位 */
    DMA_initStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    /* 每搬完一个值，内存地址 += 2 字节，这样 4 个结果依次落到 [0][1][2][3] */
    DMA_initStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    /* 循环模式：一轮 4 个搬完后指针自动回到 ADC_value[0] 重新开始*/
    DMA_initStructure.DMA_Mode = DMA_Mode_Circular;
    /* ---------- 源地址 ---------- */
    DMA_initStructure.DMA_PeripheralBaseAddr = (u32)&ADC1->DR;
    /* 每次从外设读 16 bit（与 DR 宽度一致，都用 HalfWord） */
    DMA_initStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    /* 外设地址固定不变：始终读 ADC1->DR，不自增 */
    DMA_initStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    /* 优先级中等，够用 */
    DMA_initStructure.DMA_Priority = DMA_Priority_Medium;
    /* ADC1 的 DMA 请求在硬件上固定映射到 DMA1_Channel1，所以这里只能填 Channel1 */
    DMA_Init(DMA1_Channel1, &DMA_initStructure);
    /* 清一次 TC 中断标志，防止初始化前遗留的旧标志误触发 */
    DMA_ClearITPendingBit(DMA1_IT_TC1);
    /* 一轮 4 个搬完后触发传输完成中断，上层可在中断里标记"数据已刷新" */
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
    /* 打开 DMA 通道：DMA 端准备接收请求 */
    DMA_Cmd(DMA1_Channel1, ENABLE);
    /* 打开 ADC1 的 DMA 请求：ADC 端每次转换完成就向 DMA 发请求。*/
    ADC_DMACmd(ADC1, ENABLE);
}


void ADC_Config(void)
{
    // 先准备输入引脚，再配置 ADC 本体，最后打开 DMA。
    adc_gpio_init();
    adc_config();
    ADC_DMA_Config();
}
