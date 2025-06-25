#ifndef UART_TERMINAL_PHY
#define UART_TERMINAL_PHY

#include "SignalList.hpp"
#include "GeneralConfig.h"

#define TERMINAL_PHY_UART huart1

void MX_USART1_UART_Init(void);
void MX_DMA_Init(void);


#ifdef __cplusplus
 extern "C" {
#endif
void USART1_IRQHandler(void);
void DMA1_Stream1_IRQHandler(void);
void DMA1_Stream2_IRQHandler(void);


#ifdef __cplusplus
}
#endif


#endif