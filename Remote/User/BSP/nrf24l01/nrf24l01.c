#include "nrf24l01.h"
#include "spi.h"
#include "systick.h"
#include "pair_freq.h"
#include "oled.h"
#include "usart1.h"
#include "sendpacket.h"
#include <stddef.h>

extern Pair pair;

/* CMSIS in this project predates DWT_Type. CYCCNT also runs inside DMA ISR. */
#define NRF_DWT_CTRL   (*((volatile uint32_t *)0xE0001000UL))
#define NRF_DWT_CYCLES (*((volatile uint32_t *)0xE0001004UL))

static void NRF24L01_Recover(void)
{
    Clr_NRF24L01_CE();
    Set_NRF24L01_CSN();
    NRF24L01_Write_Reg(SPI_WRITE_REG + STATUS, RX_OK | TX_OK | MAX_TX);
    NRF24L01_Write_Buf(FLUSH_TX, NULL, 0);
    NRF24L01_Write_Buf(FLUSH_RX, NULL, 0);
    Set_NRF24L01_CSN();
}


//初始化24L01的IO口
void NRF24L01_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    NRF_DWT_CTRL |= 1UL; /* Enable without resetting another user's counter. */
    //CE    
    RCC_APB2PeriphClockCmd(RCC_NRF24L01_CE | RCC_NRF24L01_CSN | RCC_NRF24L01_IRQ, ENABLE);            //使能GPIO的时钟
    GPIO_InitStructure.GPIO_Pin = NRF24L01_CE;                  //NRF24L01 模块片选信号
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;            //推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIO_NRF24L01_CE, &GPIO_InitStructure);
    //CSN
    GPIO_InitStructure.GPIO_Pin = NRF24L01_CSN;      
    GPIO_Init(GPIO_NRF24L01_CSN, &GPIO_InitStructure);
    Set_NRF24L01_CE();                                          //初始化时先拉高
    Set_NRF24L01_CSN();                                         //初始化时先拉高
    //IRQ 
    GPIO_InitStructure.GPIO_Pin = NRF24L01_IRQ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU  ;             //上拉输入
    GPIO_Init(GPIO_NRF24L01_IRQ, &GPIO_InitStructure);
    Clr_NRF24L01_CE(); 	                                        //使能24L01
    Set_NRF24L01_CSN();                                         //SPI片选取消	 
}
//上电检测NRF24L01是否在位
//写5个数据然后再读回来进行比较，相同时返回值:0，表示在位;否则返回1，表示不在位	
uint8_t NRF24L01_Check(void)
{
	uint8_t buf[5]={0xA5,0xA5,0xA5,0xA5,0xA5};
	uint8_t buf1[5];
	uint8_t i,ret; 
	ret=NRF24L01_Write_Buf(SPI_WRITE_REG+TX_ADDR,buf,5);            //写入5个字节的地址.
	printf("这里都写不进去，更别说别的了\n");
	if (ret == NRF24L01_ERR)         // SPI 写失败
        return 1;            // 认为 NRF24L01 不在位
	ret=NRF24L01_Read_Buf(TX_ADDR,buf1,5);                          //读出写入的地址 
	if(ret==NRF24L01_ERR)
		return 1;
	for(i=0;i<5;i++)
	{
	    if(buf1[i]!=0xA5)
		{
		    break;					   
		}
	}
	if(i!=5)
        return 1;                                               //NRF24L01不在位	
	return 0;		                                            //NRF24L01在位
}	 	 

//通过SPI写寄存器，regaddr(高3位指令+低5位寄存器地址)
uint8_t NRF24L01_Write_Reg(uint8_t regaddr,uint8_t data)
{
    uint8_t status;
    uint8_t rx_data;
	uint8_t ret;
    Clr_NRF24L01_CSN();    //使能SPI传输
                                                                
    ret=SPI1_RW(regaddr,&status);  //
    if(ret!=0)
	{
		Set_NRF24L01_CSN();
		return NRF24L01_ERR;
	}
	ret=SPI1_RW(data,&rx_data);  //写入寄存器的值	                                                            
    if(ret!=0)
	{
		Set_NRF24L01_CSN();
		return NRF24L01_ERR;
	}
	Set_NRF24L01_CSN();  //禁止SPI传输                                                                
    return status;       //返回状态值
}
//读取SPI寄存器值 ，regaddr:要读的寄存器
uint8_t NRF24L01_Read_Reg(uint8_t regaddr)
{
	uint8_t reg_val,Status,ret;
 	Clr_NRF24L01_CSN();                                         //使能SPI传输	                                                           
  	ret=SPI1_RW(regaddr,&Status);                                           //发送寄存器号
  	if(ret!=0){
		Set_NRF24L01_CSN();
		return NRF24L01_ERR;
	}
	ret=SPI1_RW(0XFF,&reg_val);                                      //读取寄存器内容                                                                
  	if(ret!=0)
	{
		Set_NRF24L01_CSN();
		return NRF24L01_ERR;
	} 
	Set_NRF24L01_CSN();
  	return reg_val;                                            //返回状态值
}	
//在指定位置读出指定长度的数据
//*pBuf:数据指针
//返回值,此次读到的状态寄存器值 
uint8_t NRF24L01_Read_Buf(uint8_t regcmd,uint8_t *pBuf,uint8_t datalen)
{
	uint8_t status=NRF24L01_ERR;
  	uint8_t ok=1;
	//如果数组为空，但是数据长度不为0，则返回错误
	if ((pBuf == NULL) && (datalen != 0U)) 
	{
		printf("数组为空，但是数据长度不为0，这不就是有问题吗？\n");
		return NRF24L01_ERR;
	}
	
	Clr_NRF24L01_CSN();                                         //使能SPI传输                                                                
  	if(SPI1_RW(regcmd,&status)!=0)                                    //发送寄存器值(位置),并读取状态值   	
 	{
		ok=0;
	}
	else
	{
		while(datalen>0)
		{
			if(SPI1_RW(0xFF,pBuf)!=0)
			{
				ok=0;
				break;
			}
            ++pBuf;
            --datalen;
		}
	}
	Set_NRF24L01_CSN();
	return ok?status:NRF24L01_ERR;
}

//在指定位置写指定长度的数据
//*pBuf:数据指针
//返回值,此次读到的状态寄存器值
uint8_t NRF24L01_Write_Buf(uint8_t regcmd, const uint8_t *pBuf, uint8_t datalen)
{
	uint8_t status=NRF24L01_ERR;
	uint8_t dummy;
	uint8_t ok=1;
	if((pBuf == NULL) && (datalen != 0U)) return NRF24L01_ERR;
 	Clr_NRF24L01_CSN();  //使能SPI传输 
	if(SPI1_RW(regcmd,&status)!=0)
	{
		ok=0;
	}
	else
	{
		while(datalen>0)
		{
			datalen--;
			if(SPI1_RW(*pBuf++,&dummy)!=0)
			{
				ok=0;
				break;
			}
		}
	}                                                
  	Set_NRF24L01_CSN();                                         //关闭SPI传输                                                               
  	return ok?status:NRF24L01_ERR;                                              //返回读到的状态值
}				   
//启动NRF24L01发送一次数据
//sendBuff:待发送数据首地址
//返回值:发送完成状况
/* 发送一包数据，并顺带取走无人机回传的 ACK 载荷(如果有)。返回值为：
 *   TX_OK(0x20)        : 发射成功，且收到了硬件 ACK
 *   MAX_TX(0x10)       : 达到最大自动重发次数仍无 ACK，说明飞机没收到或没应答
 *   NRF24L01_TIMEOUT   : 等 IRQ 等超时，芯片可能卡死或连线异常
 *   NRF24L01_ERR(0xFF) : SPI 通信失败或参数非法
 *
 * 完整流程(ShockBurst 发射时序)：
 *   1. 清中断标志、清空收发 FIFO —— 保证这次的结果不被上一次的残留干扰
 *   2. 写 WR_TX_PLOAD，把 11 字节载荷装进 TX FIFO
 *   3. CE 拉高一个脉冲 -> 芯片自动加前导码/地址/CRC 并发射
 *   4. 等 IRQ 变低，表示有结果了(TX_DS 或 MAX_RT)
 *   5. CE 拉低，读 STATUS 判断成败；成功后读走 RX FIFO 里的 ACK 载荷
 *   6. 清 STATUS 标志，为下一次发射做准备
 *
 * 注意：本函数在 DMA1_Channel1 中断里被调用，所以全程用"轮询 + 周期计数超时"，
 *       不能用任何阻塞延时，也不能依赖 SysTick(优先级可能低于当前中断)。
 *       另外它内部会调用 NrfRxPacket()，即"发一包 = 顺带收一包"，
 *       单条射频链路就实现了双向通信。 */
uint8_t NRF24L01_TxPacket(uint8_t *sendBuff)
{
    uint8_t state;
    uint32_t start;
    uint32_t polls = 0;                                                 // 轮询计数，作为 CYCCNT 不可用时的兜底超时条件
    uint32_t limit = (SystemCoreClock / 1000U) * NRF24L01_TX_TIMEOUT_MS; // 把 15ms 换算成内核周期数，作为超时阈值

    /* 发射前先作废旧的接收标志：否则上一次的 ACK 状态会被当成本次的结果 */
    NrfInvalidateRx();

    /* 拉低 CE 进入 Standby-I，保证后面改 FIFO 时芯片不在收发 */
    Clr_NRF24L01_CE();
    if ((sendBuff == NULL) ||
	//用"写 STATUS = 0x70"这个动作，清掉 RX_DR/TX_DS/MAX_RT 三个中断标志（写 1 清零），并通过返回值是否有错误来判断 SPI 通信是否健康。
        (NRF24L01_Write_Reg(SPI_WRITE_REG + STATUS,RX_OK | TX_OK | MAX_TX) == NRF24L01_ERR) ||
        (NRF24L01_Write_Buf(FLUSH_RX, NULL, 0) == NRF24L01_ERR) ||   //丢残留ACK载荷
        (NRF24L01_Write_Buf(FLUSH_TX, NULL, 0) == NRF24L01_ERR) ||    //丢上次没发出去的旧数据
        (NRF24L01_Write_Buf(WR_TX_PLOAD, sendBuff,TX_PLOAD_WIDTH) == NRF24L01_ERR)) 
	{
        NRF24L01_Recover();                                            
        return NRF24L01_ERR;
    }
    /* 记下发射起始时刻(内核周期数)。CYCCNT 是 32 位且会自然回绕，
     * 所以下面算差值时必须转成 uint32_t 做无符号减法，编译器会自动处理回绕 */
    start = NRF_DWT_CYCLES;
    Set_NRF24L01_CE();

    /* IRQ 仍为高说明还没有结果，继续等；一旦变低说明 TX_DS 或 MAX_RT 已置位 */
    while (READ_NRF24L01_IRQ() != 0) {
        /* Fail-safe if debug/trace configuration stops CYCCNT. */
        /* 两个退出条件缺一不可：
         *   ① 周期计数差超过 15ms —— 正常情况下 1Mbps 发 11 字节远小于这个时间
         *   ② 轮询 100 万次 —— 万一调试器把 DWT 关掉导致 CYCCNT 不再变化，
         *      靠这个次数上限也能跳出，绝不至于死循环卡住整个 DMA 中断 */
        if (((uint32_t)(NRF_DWT_CYCLES - start) >= limit) ||
            (++polls >= 1000000UL)) {
            NRF24L01_Recover();
            return NRF24L01_TIMEOUT;
        }
    }
    /* IRQ 已触发，先拉低 CE 停止收发，再去读状态，避免下一次发射被提前触发 */
    Clr_NRF24L01_CE();
    state = NRF24L01_Read_Reg(STATUS);
    /* 判断发射结果，三种情况都算失败：
     *   state == NRF24L01_ERR : SPI 读失败
     *   state & MAX_TX        : 重发 10 次仍收不到 ACK —— 飞机没收到/信道或地址不对
     *   (state & TX_OK) == 0  : 既没报 MAX_RT 也没报 TX_DS，属于异常状态
     * 注意 MAX_TX 与 TX_OK 是两个不同的位，所以必须先单独判 MAX_TX。 */
    if ((state == NRF24L01_ERR) || ((state & MAX_TX) != 0U) ||((state & TX_OK) == 0U)) 
	{
        NRF24L01_Recover();
        /* 区分返回值的用意：MAX_TX 是"发得出去但没有应答"，属于链路问题，可以计数观察；
         * 而 NRF24L01_ERR 是 SPI 层面的硬故障，两者在统计和 LED 提示上应当分别处理 */
        return (state != NRF24L01_ERR && (state & MAX_TX)) ?MAX_TX : NRF24L01_ERR;
    }

    /* Read RX_DR before clearing the module interrupt flags. */
    /* 步骤 5 续：STATUS 已确认 TX_DS 置位，说明飞机回了 ACK。
     * 若飞机在 ACK 里带了回传数据(EN_ACK_PAY)，此时数据已在 RX FIFO 中，
     * 必须在清中断标志之前读走，否则会被下一次的 FLUSH_RX 丢掉 */
    NrfRxPacket();
    /* 步骤 6：清掉本次产生的中断标志，让 IRQ 引脚恢复高电平供下一轮使用。
     * 这里清标志失败也按错误处理：标志残留会导致下一包的判断逻辑全部错位 */
    if (NRF24L01_Write_Reg(SPI_WRITE_REG + STATUS,
                           RX_OK | TX_OK | MAX_TX) == NRF24L01_ERR) {
        NrfInvalidateRx();                                              // 本次没读成功，接收状态也要作废
        NRF24L01_Recover();
        return NRF24L01_ERR;
    }
    /* 确保 SPI 总线回到空闲态(片选为高)，函数返回时总线不留占用 */
    Set_NRF24L01_CSN();
    return TX_OK; /* Hardware ACK success; payload validity is independent. */
    /* 返回 TX_OK 只代表"射频层拿到了硬件 ACK"，不代表数据内容被应用层接受；
     * 帧头/帧尾/内容是否合法由 NrfRxPacket() 里的协议检查负责 */
}

/* Configure PTX mode; always use W_REGISTER for register writes. */
void NRF24L01_TX_Mode(void)
{														 
	Clr_NRF24L01_CE();		 	
	NRF24L01_Write_Reg(SPI_WRITE_REG+SETUP_AW, 0x03); // 设置地址宽度为 5bytes
	NRF24L01_Write_Buf(SPI_WRITE_REG+TX_ADDR,(uint8_t*)pair.addr,TX_ADR_WIDTH);    //写TX节点地址 
	NRF24L01_Write_Buf(SPI_WRITE_REG+RX_ADDR_P0,(uint8_t*)pair.addr,RX_ADR_WIDTH); //设置RX节点地址,主要为了接收ACK	  
	NRF24L01_Write_Reg(SPI_WRITE_REG+FEATURE, 0x06 );//使能动态负载长度及带负载的ACK应答
	NRF24L01_Write_Reg(SPI_WRITE_REG+DYNPD, 0x01); //使能接收管道0动态负载长度
	NRF24L01_Write_Reg(SPI_WRITE_REG+EN_AA,0x01);               //使能通道0的自动应答    
	NRF24L01_Write_Reg(SPI_WRITE_REG+EN_RXADDR,0x01);           //使能通道0的接收地址  
	NRF24L01_Write_Reg(SPI_WRITE_REG+RF_CH,pair.freq_channel);  //设置RF通道
	NRF24L01_Write_Reg(SPI_WRITE_REG+SETUP_RETR,0x1a);  //设置自动重发间隔时间:500us;最大自动重发次数:10次
	NRF24L01_Write_Reg(SPI_WRITE_REG+RF_SETUP,0x07);  //设置射频数据率为1MHZ，发射功率为7dBm
	NRF24L01_Write_Reg(SPI_WRITE_REG+CONFIG,0x0e);  //配置基本工作模式的参数;开启CRC，配置为发射模式,开启所有中断     
	Set_NRF24L01_CE();                              //CE为高,10us后启动发送
}		  
