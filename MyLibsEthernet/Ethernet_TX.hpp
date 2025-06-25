#ifndef ETHERNETTX_H
#define ETHERNETTX_H

//#include "stm32f4xx_hal_eth.h"
#include "GeneralConfig.h"

#include "FreeRTOS.h"
#include "semphr.h"
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


class EthernetTxProcess_c
{
  static EthernetTxProcess_c* ownPtr;

  SemaphoreHandle_t xTXDescriptorSemaphore;

  TaskHandle_t txTask;
  TaskHandle_t actSendingTask;

  ETH_TxPacketConfig TxConfig;
  ETH_BufferTypeDef buffers[2];
  ETH_HandleTypeDef* heth_p;

  static void MainTxWrapper( void * pvParameters )
  {
    ((EthernetTxProcess_c*)pvParameters)->MainTx();
  }

  void MainTx(void );


  public:
  static EthernetTxProcess_c* GetOwnPtr(void) { return ownPtr; }
  TaskHandle_t  GetTaskPid(void) { return txTask; }
  TaskHandle_t  GetActTaskPid(void) { return actSendingTask; }
  void ProvideData(ETH_HandleTypeDef* heth_) {heth_p = heth_; }

  EthernetTxProcess_c(void);

  bool InterfaceInit(void);

  static bool SendBuffer(uint8_t* buffer, uint32_t dataSize,uint8_t* buffer2, uint32_t dataSize2);


  //static bool SendBuffer(NetworkBufferDescriptor_t * const pxNetworkBuffer, bool releaseAfterSend);











};

#endif