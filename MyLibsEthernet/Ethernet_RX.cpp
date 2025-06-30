 #include <stdio.h>
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

#include "Ethernet_RX.hpp"
#include "Ethernet.hpp"
#include "TcpDataDef.hpp"
#include "EthernetCommon.hpp"
#include "EthernetBuffers.hpp"

#include "SignalList.hpp"

#if USE_MDNS == 1
#include "EthernetMDNS.hpp"
#endif



EthernetRxProcess_c* EthernetRxProcess_c::ownPtr = nullptr;


void HAL_ETH_RxCpltCallback( ETH_HandleTypeDef * heth )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ( void ) heth;

    /* Pass a RX-event and wakeup the prvEMACHandlerTask. */

    EthernetRxProcess_c* ptr = EthernetRxProcess_c::GetOwnPtr();
    if( ptr != NULL )
    {
        xTaskNotifyFromISR( ptr->GetTaskPid(), EMAC_IF_RX_EVENT, eSetBits, &( xHigherPriorityTaskWoken ) );
        //vTaskNotifyGiveFromISR( ptr->GetTaskPid(),  &( xHigherPriorityTaskWoken ) );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ( void ) heth;

    /* Pass a RX-event and wakeup the prvEMACHandlerTask. */

    EthernetRxProcess_c* ptr = EthernetRxProcess_c::GetOwnPtr();
    if( ptr != NULL )
    {
        xTaskNotifyFromISR( ptr->GetTaskPid(), EMAC_IF_ERR_EVENT, eSetBits, &( xHigherPriorityTaskWoken ) );
        //vTaskNotifyGiveFromISR( ptr->GetTaskPid(),  &( xHigherPriorityTaskWoken ) );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
  *pStart = buff;
//  printf("HAL_ETH_RxLinkCallback\n");
}

void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{

  uint8_t* buffer =  EthernetBuffers_c::NewBuffer();
  buffer +=2;
  *buff = buffer;
}

EthernetRxProcess_c::EthernetRxProcess_c(void)
{
  eth_p = nullptr;
  if(ownPtr == nullptr)
  {
    ownPtr = this;
  }
  else
  {
    Error_Handler();
  }
  //xTXDescriptorSemaphore = NULL;

  if( xTaskCreate( MainRxWrapper, "EMAC_RX", configEMAC_TASK_STACK_SIZE, this, 6, &rxTask ) != pdPASS )
  {
      Error_Handler();
  }



}

void EthernetRxProcess_c::MainRx( void )
{
  uint32_t ulISREvents = 0U;
  BaseType_t xResult;
  void* pAppBuff;
  #if DEBUG_PROCESS > 0
  printf("start Main RX process\n");
  #endif

  while(eth_p == nullptr)
  {
    vTaskDelay(10);
  }
  eth_p->InitInternal();


  while(1)
  {

    if(xTaskNotifyWait( 0U,               
                     EMAC_IF_RX_EVENT|EMAC_IF_TIMEOUT_EVENT, 
                     &( ulISREvents ),  
                     1000  ) == pdTRUE  ) // portMAX_DELAY
    {

      if( ( ulISREvents & EMAC_IF_RX_EVENT ) != 0 )
      {
        //printf(" ETH RX event\n");
        //xResult = prvNetworkInterfaceInput();
        HAL_StatusTypeDef readStatus;
        
        uint8_t framesCnt = 0;

        do
        {
          readStatus =  HAL_ETH_ReadData(heth_p, &pAppBuff);

          if(readStatus == HAL_OK)
          {
            uint32_t dataLength = heth_p->RxDescList.RxDataLength;
            uint8_t *pucEthernetBuffer = (uint8_t *) pAppBuff;

            pucEthernetBuffer -= 2;


            //printf(" ETH RX event, size = %d\n",dataLength);

            if(EarlyCheck(pucEthernetBuffer,dataLength) == true)
            {

              tcpRxEventSig_c* sig_p = new tcpRxEventSig_c;
              sig_p->dataBuffer = pucEthernetBuffer;
              sig_p->dataSize = dataLength;

              sig_p->Send();

            }
            else
            {
              /* discard packet */
              //printf("Discard packet \n");
              delete pucEthernetBuffer;

            }   

            eth_p->ResetTimout();
          }
          framesCnt++;
        }
        while(readStatus == HAL_OK);

        //printf("ETH RX strike = %d\n",framesCnt);
      }
      if( ( ulISREvents & EMAC_IF_TIMEOUT_EVENT ) != 0 )
      {
        eth_p->Timeout();
      }

      if( ( ulISREvents & EMAC_IF_ERR_EVENT ) != 0 )
      {
        uint32_t errorCode = heth_p->DMAErrorCode;
        #if DEBUG_ETHERNET > 0
        printf("Ethernet Error interrupt 0x%08X\n",errorCode);
        #endif


      } 

      
      /*
      if( xPhyCheckLinkStatus( &xPhyObject, xResult ) != 0 )
      {
         //Something has changed to a Link Status, need re-check. 
        prvEthernetUpdateConfig( pdFALSE );
      }*/
    }



  }

}

bool EthernetRxProcess_c::EarlyCheck( uint8_t * pucEthernetBuffer, uint32_t dataLength )
{


    Packet_st* packet = (Packet_st*) pucEthernetBuffer;
    uint16_t ethType = ntohs(packet->macHeader.ethType);

    #if USE_MDNS == 1
    if(memcmp(packet->macHeader.MAC_Dest, MDNS_c::GetMac(), 6) == 0)
    {
      
      return true;
    }
    #endif

    switch(ethType)
    {
      case ETH_TYPE_IPV4:
      {
        uint16_t flagsAndOffset = ntohs(packet->ipHeader.flags_foffset );
        if(flagsAndOffset & 0x1FFF)
        {
          return false;
        }
        if(packet->ipHeader.version_ihl != 0x45)
        {
          //printf("ETH_RX, disckard due to IHL (0x%X)\n",packet->ipHeader.version_ihl);
          return false;
        }


      }
      return true;

      case ETH_TYPE_ARP:
        return true;

      default:
        return false;




    }
   
}

bool EthernetRxProcess_c::InterfaceInit(void)
{

  /* Assign Rx memory buffers to a DMA Rx descriptor */
  ETH_DMADescTypeDef * pxDMADescriptor;


  return true;
}