#ifndef UART_TERMINAL
#define UART_TERMINAL

#include "SignalList.hpp"
#include "GeneralConfig.h"
#include "semphr.h"
//#include "UartTerminalPhy_h7.hpp"
#define UART_RX_BUFFER_SIZE 256
#define UART_TX_MAX_LEN 1024

#define USART_ECHO 1


class UartTerminalProcess_c : public process_c
{


  SemaphoreHandle_t usartTxSemaphore;

  uint16_t  rxPtr;
  

  uint8_t rxBuffer[UART_RX_BUFFER_SIZE];

  uint8_t txBuffer[UART_RX_BUFFER_SIZE];

  void InitTerminal(void);

  public:

  static UartTerminalProcess_c* ownRef;

  UART_HandleTypeDef* huart_p;



  TaskHandle_t txTaskHandle;
  TaskHandle_t rxTaskHandle;
  uint16_t rxSize;

  UartTerminalProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId);
  void main(void);

  static void SendToAll(uint8_t* buf, uint16_t size);
  void SendBuffer(uint8_t* buf, uint16_t size);
  
};







#endif