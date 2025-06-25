#ifndef UART_TERMINAL_PHY
#define UART_TERMINAL_PHY

#include "SignalList.hpp"
#include "CommonDef.hpp"

#define TERMINAL_PHY_UART huart1

void MX_USART1_UART_Init(void);
void MX_DMA_Init(void);


#ifdef __cplusplus
 extern "C" {
#endif
void USART1_IRQHandler(void);
void DMA2_Stream2_IRQHandler(void);
void DMA2_Stream7_IRQHandler(void);


#ifdef __cplusplus
}
#endif


#endif