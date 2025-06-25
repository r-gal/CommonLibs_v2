#ifndef ETHERNETRX_H
#define ETHERNETRX_H

//#include "stm32f4xx_hal_eth.h"
#include "GeneralConfig.h"

/* FreeRTOS+TCP includes. */
//#include "FreeRTOS_IP.h"
//#include "FreeRTOS_Sockets.h"
//#include "FreeRTOS_IP_Private.h"
//#include "FreeRTOS_DNS.h"
//#include "FreeRTOS_ARP.h"
//#include "NetworkBufferManagement.h"
//#include "NetworkInterface.h"
//#include "phyHandling.h"

#define configEMAC_TASK_STACK_SIZE    ( 2 * configMINIMAL_STACK_SIZE )
#define niEMAC_HANDLER_TASK_PRIORITY    configMAX_PRIORITIES - 1

#define EMAC_IF_RX_EVENT        1UL
#define EMAC_IF_TX_EVENT        2UL
#define EMAC_IF_ERR_EVENT       4UL
#define EMAC_IF_TIMEOUT_EVENT   8UL

class Ethernet_c;

class EthernetRxProcess_c
{
  static EthernetRxProcess_c* ownPtr;

  //SemaphoreHandle_t xTXDescriptorSemaphore;

  TaskHandle_t rxTask;
  TaskHandle_t actSendingTask;

  ETH_HandleTypeDef* heth_p;
  Ethernet_c* eth_p;

  static void MainRxWrapper( void * pvParameters )
  {
    ((EthernetRxProcess_c*)pvParameters)->MainRx();
  }

  void MainRx(void );

  bool EarlyCheck( uint8_t * pucEthernetBuffer,uint32_t dataLength );


  public:
  static EthernetRxProcess_c* GetOwnPtr(void) { return ownPtr; }
  TaskHandle_t  GetTaskPid(void) { return rxTask; }
  //TaskHandle_t  GetActTaskPid(void) { return actSendingTask; }
  void ProvideData(ETH_HandleTypeDef* heth_,Ethernet_c* eth_p_) {heth_p = heth_; eth_p = eth_p_;}

  EthernetRxProcess_c(void);

  bool InterfaceInit(void);

  TaskHandle_t GetRxTask(void) {return rxTask;}

};

#endif