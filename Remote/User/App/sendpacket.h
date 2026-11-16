#ifndef _sendpacket_h_
#define _sendpacket_h_

#include "stm32f10x.h"

typedef struct
{
    uint16_t thr;  // 油门约 0~1000，超出 uint8_t 的 0~255；发送时拆为低、高两个字节
    uint8_t pit;   // 俯仰约 0~100，发送时占一个字节
    uint8_t rol;   // 横滚约 0~100，发送时占一个字节
    uint8_t yaw;   // 偏航约 0~100，发送时占一个字节
    uint8_t key;
    
}RemoteData;


extern RemoteData tx;
/* 只允许 RadioTask 调用，输入是一整帧采样副本。 */
void analyze_packet(const uint16_t *adcData);
void data_exchange(uint8_t *dateBuff);
uint8_t NrfGetAckState(uint8_t *unlocked, uint8_t *low_battery);
void NrfRxPacket(void);
void NrfInvalidateRx(void);
/* 这些诊断计数现在只由 RadioTask 更新，不在 DMA 中断里做无线业务。 */
extern volatile uint32_t g_radio_tx_ok;
extern volatile uint32_t g_radio_tx_fail;
extern volatile uint32_t g_radio_tx_timeout;
extern volatile uint8_t g_radio_last_result;
extern volatile uint32_t lastValidAckTick;
void OledDisplayPairStatus(void);

#endif

