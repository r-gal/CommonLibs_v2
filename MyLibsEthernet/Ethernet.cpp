
 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"


#include "Ethernet.hpp"
#include "Ethernet_TX.hpp"

#include "RngClass.hpp"

#include "EthernetMac.hpp"
#if USE_MDNS == 1
#include "EthernetMDNS.hpp"
#endif

//ETH_DMADescTypeDef __attribute((section(".RAM_D2"))) DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
//ETH_DMADescTypeDef __attribute((section(".RAM_D2"))) DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */
//extern ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
//extern ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];

extern ETH_HandleTypeDef heth;

Ethernet_c* Ethernet_c::ownPtr = nullptr;

 #if ETH_STATS == 1
CommandEth_c commandEth;
#endif

#define TIMEOUT_MS 500

void vFunctionTimeoutTimerCallback( TimerHandle_t xTimer )
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if(EthernetPhy_c::taskToNotifyOnTimeout != nullptr)
  {
    xTaskNotifyFromISR( EthernetPhy_c::taskToNotifyOnTimeout, EMAC_IF_TIMEOUT_EVENT, eSetBits, &( xHigherPriorityTaskWoken ) );
  }

}

Ethernet_c::Ethernet_c(void)
{
  ownPtr = this;
  xMacInitStatus = eMACInit;
  timeoutTimer = xTimerCreate("",pdMS_TO_TICKS(TIMEOUT_MS),pdTRUE,( void * ) 0,vFunctionTimeoutTimerCallback);                 
  xTimerStart(timeoutTimer,0);

}

void Ethernet_c::Init(void)
{
  #if ETH_STATS == 1
  memset(&stats,0,sizeof(EthStats_st));
  #endif
  phy.ProvideData(&heth,ethernetRx.GetRxTask());
  ethernetTx.ProvideData(&heth);
  ethernetRx.ProvideData(&heth,this);
}

void Ethernet_c::InitInternal(void)
{  
  if(InterfaceInitialise())
  {
    LinkStateChanged(1);
  }

}


void Ethernet_c::BuffersInit(void)
{

}

bool Ethernet_c::InterfaceInitialise( void )
{
  HAL_StatusTypeDef xHalEthInitStatus;

  if( xMacInitStatus == eMACInit )
  {
    /*
     * Initialize ETH Handler
     * It assumes that Ethernet GPIO and clock configuration
     * are already done in the ETH_MspInit()
     */
    /*heth.Instance = ETH;    ;
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc = DMATxDscrTab;
    heth.Init.RxDesc = DMARxDscrTab;
    heth.Init.RxBuffLen = 1524;*/

    heth.Init.MACAddr = MAC_c::GetOwnMac();// ucMACAddress ;
  
    //memset( &( DMATxDscrTab ), '\0', sizeof( DMATxDscrTab ) );
    //memset( &( DMARxDscrTab ), '\0', sizeof( DMARxDscrTab ) );

    HAL_ETH_Init( &( heth ) );

    #if USE_MDNS == 1

    //HAL_ETH_SetSourceMACAddrMatch( &( heth ),ETH_MAC_ADDRESS1,MDNS_c::GetMac());
    uint8_t* pMACAddr = MDNS_c::GetMac();

    /* Set MAC addr bits 32 to 47 */
    heth.Instance->MACA1HR = (((uint32_t)(pMACAddr[5]) << 8) | (uint32_t)pMACAddr[4]);
    /* Set MAC addr bits 0 to 31 */
    heth.Instance->MACA1LR = (((uint32_t)(pMACAddr[3]) << 24) | ((uint32_t)(pMACAddr[2]) << 16) |
                                     ((uint32_t)(pMACAddr[1]) << 8) | (uint32_t)pMACAddr[0]);

  /* Enable address and set source address bit */
    heth.Instance->MACA1HR |= (ETH_MACA1HR_AE );
    #endif


    if( ethernetTx.InterfaceInit() == false )
    {
      xMacInitStatus = eMACFailed;
    }

    if( ethernetRx.InterfaceInit() == false )
    {
      xMacInitStatus = eMACFailed;
    }
   
    /* Configure the MDIO Clock */
    HAL_ETH_SetMDIOClockRange( &( heth ) );

    /* Initialize the MACB and set all PHY properties */
    phy.Init();

    /* Force a negotiation with the Switch or Router and wait for LS. */
    bool result = phy.UpdateConfig( pdTRUE );
    if(result == true)
    {
      xMacInitStatus = eMACPass;
      #if ETH_STATS == 1
      stats.cntConfigSucc++;
      #endif
    }
    else
    {
      #if ETH_STATS == 1
      stats.cntConfigFail++;
      #endif
    }
    
  }
  
  if( xMacInitStatus != eMACPass )
  {
      /* EMAC initialisation failed, return pdFAIL. */
      return false;
  }
  else
  {   
     LINK_EVENT_et linkStatus = phy.CheckLinkStatus();
      if(( linkStatus == LINK_ON )  || (linkStatus == LINK_CHANGED_UP))
      {
          //xETH.Instance->DMAIER |= ETH_DMA_ALL_INTS;
          return true;
          //FreeRTOS_printf( ( "Link Status is high\n" ) );
          #if ETH_STATS == 1
          stats.cntLinkUp++;
          #endif
      }
      else
      {
          /* For now pdFAIL will be returned. But prvEMACHandlerTask() is running
           * and it will keep on checking the PHY and set 'ulLinkStatusMask' when necessary. */
          return false;
      }
  }


}



void Ethernet_c::ResetTimout(void)
{
  xTimerReset(timeoutTimer,pdMS_TO_TICKS(TIMEOUT_MS));

}
void Ethernet_c::Timeout()
{
  LINK_EVENT_et linkStatus = phy.CheckLinkStatus();

  if(linkStatus == LINK_CHANGED_UP)
  {
    #if ETH_STATS == 1
    stats.cntLinkUp++;
    #endif
    bool result = phy.UpdateConfig(pdFALSE);
    if(result == true)
    {
      #if ETH_STATS == 1
      stats.cntConfigSucc++;
      #endif
      LinkStateChanged(1);
    }
    else
    {
      #if ETH_STATS == 1
      stats.cntConfigFail++;
      #endif
      /* something wrong, try again after timeout */

    }
    
  }
  else if(linkStatus == LINK_CHANGED_DOWN)
  {
    #if ETH_STATS == 1
    stats.cntLinkDown++;
    #endif
    LinkStateChanged(0);
    phy.UpdateConfig(pdFALSE);
    
  }

}

void Ethernet_c::LinkStateChanged(uint8_t newState)
{
   tcpLinkEventSig_c* sig_p = new tcpLinkEventSig_c;
   sig_p->linkState = newState;
   sig_p->Send();

}


bool Ethernet_c::GetLinkState(void)
{
  return phy.GetLinkStatus();

}






 #if ETH_STATS == 1
comResp_et Com_ethgetstat::Handle(CommandData_st* comData)
{
  char* textBuf  = new char[256];

  Ethernet_c* eth_p = Ethernet_c::GetOwnPtr();
  EthStats_st* stat_p = &eth_p->stats;



  sprintf(textBuf," Ethernet statistics: \n");
  Print(comData->commandHandler,textBuf);

  sprintf(textBuf,"ActState:   %s\n",eth_p->GetLinkState() ? "UP" : "DOWN");
  Print(comData->commandHandler,textBuf);

  sprintf(textBuf,"Link UP:   %d \n",stat_p->cntLinkUp);
  Print(comData->commandHandler,textBuf);
  sprintf(textBuf,"Link Down: %d\n",stat_p->cntLinkDown);
  Print(comData->commandHandler,textBuf);
  sprintf(textBuf,"Config Success: %d\n",stat_p->cntConfigSucc);
  Print(comData->commandHandler,textBuf);
  sprintf(textBuf,"Config Failed : %d\n",stat_p->cntConfigFail);
  Print(comData->commandHandler,textBuf);

  delete[] textBuf;

  //delete sig_p;

  return COMRESP_OK;

}
#endif