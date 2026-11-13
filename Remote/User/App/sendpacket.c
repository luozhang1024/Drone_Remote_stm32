#include "sendpacket.h"
#include "timing_trigger.h"
#include "usart1.h"
#include "adc.h"
#include "nrf24l01.h"
#include "key.h"
#include "led.h"
#include "pair_freq.h"
#include "oled.h"
#include "systick.h"
#include <string.h>

RemoteData tx = {0};
extern Pair pair;

static u8 rxPacket[TX_PLOAD_WIDTH] = {0};
volatile u8 rxPacketStatus;
volatile uint32_t lastValidAckTick;
volatile uint32_t g_radio_tx_ok;
volatile uint32_t g_radio_tx_fail;
volatile uint32_t g_radio_tx_timeout;
volatile uint8_t g_radio_last_result;



void analyze_packet(volatile uint16_t *adcData)
{
    /* ADC1 为 12 位、右对齐，原始读数范围为 0~4095；这里缩放为遥控协议数值，
     * 不是换算电压或实际角度。DMA 按 PA0、PA1、PA2、PA3 顺序保存到 adcData[0~3]。
     * 油门：0.24420f 约等于 1000 / 4095，目标量程约 0~1000，需要 uint16_t 存储。
     * 方向：0.02442f 约等于 100 / 4095，目标量程约 0~100，uint8_t 足以存储。
     * f 表示 float 常量；转为无符号整数时会截去小数，并非四舍五入。
     * 注意：现有系数略小于精确比例，4095 实际转换为油门 999、方向 99；
     *       摇杆中位约为方向 50。这里没有做中位校准、死区或正负方向换算。
     */
    tx.thr = (uint16_t)(((float)(adcData[1]))*0.24420f);
    tx.yaw = (uint8_t)(((float)(adcData[0]))*0.02442f);
    tx.pit = (uint8_t)(((float)(adcData[3]))*0.02442f);
    tx.rol = (uint8_t)(((float)(adcData[2]))*0.02442f);
}

void data_exchange(uint8_t *dateBuff)
{
    /* NOT: a zero probe without 0x8B cannot trigger Drone wait_pairing(). */
    memset(dateBuff, 0, TX_PLOAD_WIDTH);
	//发送新的地址和频点给飞机
	if(pair.step == STEP1){   //如果此时处于第一阶段，也就是油门已经推到大于900，那么在WaitPairing函数已将ID转换为通讯地址了
			dateBuff[0] = 0xA8;					//帧头
			dateBuff[1] = pair.addr[0];		  //5个字节地址 现在只需要将最新的通讯地址放在dataBuff里面（也就是tx_dat)发送给无人机   
			dateBuff[2] = pair.addr[1];	    //    
			dateBuff[3] = pair.addr[2];			//	
			dateBuff[4] = pair.addr[3];			//	
			dateBuff[5] = pair.addr[4];			//	
			dateBuff[6] = pair.freq_channel;		//通信频点		
			dateBuff[TX_PLOAD_WIDTH-1] = 0x8B;	//帧尾      
	}
	//正常数据通信
	else if(pair.step == DONE){
			dateBuff[0] = 0xA8;		//帧头
            /* 无线载荷按字节打包，不能把 16 位油门直接塞进一个 u8。
             * STM32F103 按小端存储：低地址是低 8 位，下一地址是高 8 位。
             * 因此 [1] = thr & 0xFF，[2] = (thr >> 8) & 0xFF；
             * 接收端应按 thr = [1] | ([2] << 8) 重组（此处说明协议，非接收端代码）。
             * pit/rol/yaw 各为 uint8_t，分别直接占用 [3]/[4]/[5] 一个字节。
             */
			dateBuff[1] = *((u8*)&tx.thr);		//油门低八位
			dateBuff[2] = *(((u8*)&tx.thr)+1);		//油门高八位
			dateBuff[3] = tx.pit;		//俯仰舵向
			dateBuff[4] = tx.rol;		//横滚舵向
			dateBuff[5] = tx.yaw;		//偏航舵向
			dateBuff[6] = key.l;		//左功能按键
			dateBuff[7] = key.r;		//右功能按键
			dateBuff[8] = 0;		//未使用，可自行添加
			dateBuff[9] = 0;		//未使用，可自行添加
			dateBuff[TX_PLOAD_WIDTH-1] = 0x8B;		//帧尾     
	}
}

uint8_t tx_dat[TX_PLOAD_WIDTH] = {0};  //定义一个发送数组


//DMA通道1中断触发，如果触发，则表明DMA已经将ADC数据搬运到内存数组中，则可以进行发送数据了    
void DMA1_Channel1_IRQHandler(void)
{
	uint8_t ret=0;
	
	if(DMA_GetITStatus(DMA1_IT_TC1) == SET){	
		analyze_packet(ADC_value);		//解析ADC采集，DMA搬运到ADC_value数组的摇杆数据到tx结构体
		data_exchange(tx_dat);//数据装入发送数组

		ret=NRF24L01_TxPacket(tx_dat);//发送数据，得到发送结果ret
		g_radio_last_result = ret;
		if(ret == TX_OK){
            ++g_radio_tx_ok;
			LedColorSet(GREEN);		//发送成功绿灯亮
		} else{
            ++g_radio_tx_fail;
            if (ret == NRF24L01_TIMEOUT) ++g_radio_tx_timeout;
			LedBlink(YELLOW);		//信号丢失黄灯闪烁
		}
		
		DMA_ClearITPendingBit(DMA1_IT_TC1);		
		
	}
}

//接收数据包
/* 从 nRF24L01 的 RX FIFO 取出一包无人机回传的数据（ACK 载荷）。
 * 调用时机：NRF24L01_TxPacket() 发射成功（TX_OK 置位）之后调用，
 *           也可以在别处主动轮询调用。函数不阻塞，没数据就立刻返回。
 *
 * 本函数是"尽力而为"的：任何一步条件不满足就静默返回，不报错、不等待，
 * 因为它是顺带执行的操作，不应该影响主流程（发射）的成败。
 *
 * 有效性判定共四关，全部通过才采纳：
 *   1. 有没有新数据   -> RX_OK 置位 且 RX FIFO 非空
 *   2. 长度对不对     -> 动态长度必须等于约定的 RX_PLOAD_WIDTH
 *   3. 读得出来吗     -> SPI 读 FIFO 成功
 *   4. 帧头帧尾对不对 -> 首字节 0xAA、末字节 0xAC */
void NrfRxPacket(void)
{
    uint8_t status, fifo, width;
    uint8_t packet[RX_PLOAD_WIDTH];             // 临时缓冲：先校验通过，再拷进全局变量
    /* 先作废旧数据：这样即使下面任何一步失败，上层也不会把上一帧的旧内容当成新数据 */
    NrfInvalidateRx();
    status = NRF24L01_Read_Reg(STATUS);         // STATUS：看 RX_OK(RX_DR) 位
    fifo = NRF24L01_Read_Reg(FIFO_STATUS);      // FIFO_STATUS：看 RX FIFO 是否为空
	//状态或者FIFO状态寄存器获取报错，又或者RX_OK标志位未置位，FIFO寄存器空标志置位了，都返回
    /* 第一关：有没有数据可读。两种情况都会走到这里：
     *   RX_OK 没置位  -> 没有收到新包
     *   fifo & 1      -> RX_FIFO_EMPTY(bit0)=1，FIFO 是空的
     * 两个判断互补：一个是"有新数据"的硬件标志，一个是"读不到东西"的兜底，
     * 都查一遍可以避免空读。 
	 * RX_DR 一旦置位就保持，直到主机写 1 清除。它反映的是**“曾经收到过”，不是“现在还有”
	 * RX_DR 可能被发射流程顺带清掉，但 FIFO 里仍有数据*/
    if ((status == NRF24L01_ERR) || (fifo == NRF24L01_ERR) ||((status & RX_OK) == 0U) || ((fifo & 1U) != 0U)) 
	{
        return;
    }
    /* 第二关：确认这包数据有多长。
     * 因为使能了动态长度(DYNPD)，必须先用 R_RX_PL_WID 命令问芯片"这包几个字节"，
     * 不能想当然按固定长度读。长度和约定的 RX_PLOAD_WIDTH 不一致，说明对方协议
     * 不同或数据已损坏，必须清掉 FIFO 丢弃，否则脏数据会一直卡在 FIFO 里。 */
    if ((NRF24L01_Read_Buf(R_RX_PL_WID, &width, 1) == NRF24L01_ERR) ||
        (width != RX_PLOAD_WIDTH)) 
	{
        NRF24L01_Write_Buf(FLUSH_RX, NULL, 0);  // 丢弃这包脏数据
        return;
    }
    /* 第三关：从 RX FIFO 读出 width 个字节到临时缓冲。
     * 读失败也要清 FIFO，否则下次读到的还是同一包坏数据，形成死循环。 */
    if (NRF24L01_Read_Buf(RD_RX_PLOAD, packet, width) == NRF24L01_ERR) {
        NRF24L01_Write_Buf(FLUSH_RX, NULL, 0);
        return;
    }
    /* 第四关：应用层协议校验。帧头 0xAA、帧尾 0xAC 是与无人机约定的边界标志。
     * 射频层的 CRC 只能保证"位没翻转"，不能保证"这是一帧正确的应用数据"，
     * 所以还要做这层判断。不通过就静默丢弃（数据此时已出 FIFO，无需再清）。 */
    if ((packet[0] == 0xAA) && (packet[RX_PLOAD_WIDTH - 1] == 0xAC)) {
        memcpy(rxPacket, packet, sizeof(rxPacket));  // 拷进全局缓冲，供显示函数按字段解析
        lastValidAckTick = GetTick();                // 记录有效通信时刻，超时后判定为信号丢失
        rxPacketStatus = 1;                          // 标记"已建立有效通信"，同时触发 OLED 刷新判断
    }
}

void NrfInvalidateRx(void)
{
    rxPacketStatus = 0;
}

void DisplayPlaneInfo(void)
{
	static u8 clear = 0;
	
	if(clear != ((rxPacket[1]<<4)|rxPacket[2])){
		OLED_Clear();
		clear = (rxPacket[1]<<4)|rxPacket[2];
	}
		
	if(rxPacket[1] == 0){
		OledDisplayChinese(3,3,lock,2);//锁定
	}
	else if(rxPacket[1] == 1){
		OledDisplayChinese(3,3,unlock,2);//解锁
	}
	
	if(rxPacket[2] == 1){
		OledDisplayChinese(6,2,powerLow,3);//电量低
	} 
}

//屏幕显示
void OledDisplayPairStatus(void)
{
	static u8 clearDetect = 0;
    if ((uint32_t)(GetTick() - lastValidAckTick) >= 200U) {
        NrfInvalidateRx();
    }
	if(clearDetect != ((rxPacketStatus<<4)|pair.step)){
		OLED_Clear();
		clearDetect = (rxPacketStatus<<4)|pair.step;
	}
	
	if(rxPacketStatus == 1 && pair.step == NOT){		
		OledDisplayChinese(0,2,pairNot,3);//未对频
		OledDisplayChinese(3,0,pairTips1,9);//左遥感上推进行对频
	}
	else if(pair.step == STEP1){
		OledDisplayChinese(0,2,pairing,3);//对频中
		OledDisplayChinese(3,0,pairTips2,9);//左摇杆下拉完成对频
	}
	else if(rxPacketStatus == 1 && pair.step == DONE){
		OledDisplayChinese(0,2,pairDone,4);//对频完成
		DisplayPlaneInfo();
	}
	else if(rxPacketStatus == 0 && pair.step != DONE){		
		OledDisplayChinese(0,1,NoPlaneDetected,6);//未检测到飞机
		OledDisplayChinese(3,0,planeIfOn,16);
	}
	else if(rxPacketStatus == 0 && pair.step == DONE){		
		OledDisplayChinese(0,2,signalLost,4);//信号丢失
	}
}

