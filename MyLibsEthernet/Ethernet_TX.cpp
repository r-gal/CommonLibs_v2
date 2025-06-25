 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "stm32f4xx_hal_eth.h"
//#include "stm32f4xx_hal_rcc.h"
//#include "stm32f4xx_hal_gpio.h"
//#include "stm32f4xx_hal_conf.h"

/* FreeRTOS+TCP includes. */
//#include "FreeRTOS_IP.h"
//#include "FreeRTOS_Sockets.h"
//#include "FreeRTOS_IP_Private.h"
//#include "FreeRTOS_DNS.h"
//#include "FreeRTOS_ARP.h"
//#include "NetworkBufferManagement.h"
//#include "NetworkInterface.h"
//#include "phyHandling.h"

#include "Ethernet_TX.hpp"
#include "Ethernet.hpp"

EthernetTxProcess_c* EthernetTxProcess_c::ownPtr = nullptr;



void HAL_ETH_TxCpltCallback( ETH_HandleTypeDef * heth )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ( void ) heth;

    /* Pass a TX-event and wakeup the prvEMACHandlerTask. */

    EthernetTxProcess_c* ptr = EthernetTxProcess_c::GetOwnPtr();
    if( ptr != NULL )
    {
        xTaskNotifyFromISR( ptr->GetActTaskPid(), EMAC_IF_TX_EVENT, eSetBits, &( xHigherPriorityTaskWoken ) );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

EthernetTxProcess_c::EthernetTxProcess_c(void)
{
  if(ownPtr == nullptr)
  {
    ownPtr = this;
  }
  else
  {
    Error_Handler();
  }
  xTXDescriptorSemaphore = NULL;
/*
  if( xTaskCreate( MainTxWrapper, "EMAC_TX", configEMAC_TASK_STACK_SIZE, this, niEMAC_HANDLER_TASK_PRIORITY, &txTask ) != pdPASS )
  {
      Error_Handler();
  }
*/


}

void EthernetTxProcess_c::MainTx( void )
{

  printf("start Main TX process\n");

  while(1)
  {





  }

}

bool EthernetTxProcess_c::SendBuffer(uint8_t* buffer, uint32_t dataSize,uint8_t* buffer2, uint32_t dataSize2)
{
  uint32_t ulISREvents = 0U;
  bool success = false;

  if(xSemaphoreTake(ownPtr->xTXDescriptorSemaphore, 1000) == pdTRUE )
  {
    ownPtr->actSendingTask = xTaskGetCurrentTaskHandle();

    ownPtr->TxConfig.Length = dataSize;
    ownPtr->TxConfig.TxBuffer->buffer = buffer;
    ownPtr->TxConfig.TxBuffer->len = dataSize;
    if(buffer2 == nullptr)
    {
      ownPtr->TxConfig.TxBuffer->next = nullptr;
    }
    else
    {
      ownPtr->TxConfig.TxBuffer->next = &(ownPtr->buffers[1]);
      ownPtr->TxConfig.TxBuffer->next->buffer = buffer2;
      ownPtr->TxConfig.TxBuffer->next->len = dataSize2;
    }

    HAL_StatusTypeDef result;

    result = HAL_ETH_Transmit_IT(ownPtr->heth_p, &ownPtr->TxConfig);

    /* Wait for a new event or a time-out. */
    if(xTaskNotifyWait( 0U,                /* ulBitsToClearOnEntry */
                     EMAC_IF_TX_EVENT, /* ulBitsToClearOnExit */
                     &( ulISREvents ),  /* pulNotificationValue */
                     1000 ) == pdTRUE  )
    {
      success = true;
    }
    else
    {
      /* notification timeout */
      success =  false;
    }
  /*  char protStr[8];
    switch( pxProtPacket->xTCPPacket.xEthernetHeader.usFrameType )
    {
        case ipARP_FRAME_TYPE:
             strcpy(protStr,"ARP");
             break;

        case ipIPv4_FRAME_TYPE:
             switch( pxProtPacket->xTCPPacket.xIPHeader.ucProtocol )
             {
               case 1:
                 strcpy(protStr,"IP_ICMP");
                 break;
               default:
                 strcpy(protStr,"IP_OTH");
                 break; 



             }
             break;

        default:
             strcpy(protStr,"OTH");
             break;
    }

    printf("TX end, res=%d, suc=%d, size=%d, prot=%s\n",result,success,pxNetworkBuffer->xDataLength,protStr);
*/

    xSemaphoreGive(ownPtr->xTXDescriptorSemaphore);
  }
  else
  {
    /* semaphore timeout */
    success =  false;
  }
  return success;
}



bool EthernetTxProcess_c::InterfaceInit(void)
{
  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  TxConfig.TxBuffer = &buffers[0];
  TxConfig.TxBuffer->next = nullptr;
  buffers[1].next = nullptr;

  //xTXDescriptorSemaphore = xSemaphoreCreateCounting( ( UBaseType_t ) ETH_TXBUFNB, ( UBaseType_t ) ETH_TXBUFNB );
  xTXDescriptorSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xTXDescriptorSemaphore);

  if(xTXDescriptorSemaphore == NULL)
  { 
    return false;
  }
  else
  {
    return true;
  }
}