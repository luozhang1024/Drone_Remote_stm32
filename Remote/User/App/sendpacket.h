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
void NrfRxPacket(void);
void NrfInvalidateRx(void);
/* Stage-0 diagnostics, updated only by the DMA send path. */
extern volatile uint32_t g_radio_tx_ok;
extern volatile uint32_t g_radio_tx_fail;
extern volatile uint32_t g_radio_tx_timeout;
extern volatile uint8_t g_radio_last_result;
extern volatile uint32_t lastValidAckTick;
void OledDisplayPairStatus(void);

#endif

