#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#if CONF_USE_UART_TERMINAL == 1

#include "usart.h"
#include "dma.h"

#include "UartTerminal.hpp"

#include "commandHandler.hpp"


extern UART_HandleTypeDef TERMINAL_PHY_UART;

UartTerminalProcess_c* UartTerminalProcess_c::ownRef = nullptr;



UartTerminalProcess_c::UartTerminalProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId) : process_c(stackSize,priority,queueSize,procId,"UART")
{
  ownRef = this;
  huart_p = &TERMINAL_PHY_UART;
}

void UartTerminalProcess_c::main(void)
{

  InitTerminal();

  rxPtr = 0;

  while(1)
  {
    rxTaskHandle = xTaskGetCurrentTaskHandle();

    if(rxPtr >= UART_RX_BUFFER_SIZE) { rxPtr = 0; }

    HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(huart_p,rxBuffer + rxPtr,UART_RX_BUFFER_SIZE - rxPtr);

    if(status == HAL_OK)
    {
  

      ulTaskNotifyTake(pdTRUE,portMAX_DELAY );

      rxBuffer[rxPtr + rxSize] = 0;

      #if USART_ECHO == 1
        SendBuffer(rxBuffer + rxPtr,rxSize);


      #endif

      rxPtr += rxSize;


      if(rxBuffer[rxPtr - 1] == '\r')
      {  
        rxPtr--;

        char* str = new char[16];
        strcpy(str,"\nHandle:\n");
        SendBuffer((uint8_t*)str,strlen(str));
        delete[] str;
        SendBuffer(rxBuffer,rxPtr);
        strcpy(str,"\n");
        SendBuffer((uint8_t*)str,strlen(str));
        CommandHandler_c handler((char*)rxBuffer,rxPtr);
        handler.uartHandler = this;
        handler.ParseCommand();

        rxPtr = 0;
      }
      else if(rxBuffer[rxPtr - 1] == 0x7F) /* DEL */
      {
        if(rxPtr > 1)
        {
          rxPtr -= 2;
        }
        else
        {
          rxPtr = 0;
        }
      }
    }
    else
    {
      printf("uart rx error\n");
    }
  }
}

void UartTerminalProcess_c::InitTerminal(void)
{
  
  usartTxSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(usartTxSemaphore);

  MX_DMA_Init();
  MX_USART1_UART_Init();

}

void UartTerminalProcess_c::SendBuffer(uint8_t* buf, uint16_t size)
{

  xSemaphoreTake(usartTxSemaphore,portMAX_DELAY );

  txTaskHandle = xTaskGetCurrentTaskHandle();

  HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(huart_p,buf,size);

  if(status == HAL_OK)
  {

    ulTaskNotifyTake(pdTRUE,portMAX_DELAY );
  }
  else
  {
    printf("uart tx error\n");
  }


  xSemaphoreGive(usartTxSemaphore);  

}

void UartTerminalProcess_c::SendToAll(uint8_t* buf, uint16_t size)
{
  UartTerminalProcess_c* uartTask = ownRef;
  uartTask->SendBuffer(buf,size);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  TaskHandle_t uartTaskHandle = UartTerminalProcess_c::ownRef->txTaskHandle;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR( uartTaskHandle, 1, eSetBits, &( xHigherPriorityTaskWoken ) );
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );

}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  UartTerminalProcess_c* uartTask = UartTerminalProcess_c::ownRef;
  TaskHandle_t uartTaskHandle = uartTask->rxTaskHandle;
  uartTask->rxSize = Size;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  printf("received %d bytes\n",Size);
  xTaskNotifyFromISR( uartTaskHandle, 1, eSetBits, &( xHigherPriorityTaskWoken ) );
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );

}




#endif


