#ifndef ETHERNET_H
#define ETHERNET_H

#include "EthernetConfig.hpp"
#include "GeneralConfig.h"
#include "Ethernet_TX.hpp"
#include "Ethernet_RX.hpp"
#include "EthernetPhy.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#define EMAC_DMA_BUFFER_SIZE    ( ( uint32_t ) ( ETH_MAX_PACKET_SIZE - ipBUFFER_PADDING ) )
  #if ETH_STATS == 1
struct EthStats_st
    {
      int cntLinkUp;
      int cntLinkDown;
      int cntConfigSucc;
      int cntConfigFail;
    };
#endif

typedef enum
{
    eMACInit,   /* Must initialise MAC. */
    eMACPass,   /* Initialisation was successful. */
    eMACFailed, /* Initialisation failed. */
} eMAC_INIT_STATUS_TYPE;



class Ethernet_c
{
  EthernetTxProcess_c ethernetTx;
  EthernetRxProcess_c ethernetRx;
  EthernetPhy_c phy;

  void BuffersInit(void);

  eMAC_INIT_STATUS_TYPE xMacInitStatus;
  static Ethernet_c* ownPtr;

  TimerHandle_t timeoutTimer;

  public:

  #if ETH_STATS == 1
    EthStats_st stats; 



  #endif
  bool GetLinkState(void);

  
  
  static Ethernet_c* GetOwnPtr(void) { return ownPtr ;}
  Ethernet_c(void);

  bool InterfaceInitialise(void );
  void Init(void);
  void InitInternal(void);

  void ResetTimout(void);
  void Timeout();
  void LinkStateChanged(uint8_t newState);

};

 #if ETH_STATS == 1
#include "commandHandler.hpp"

class Com_ethgetstat : public Command_c
{
  public:
  char* GetComString(void) { return (char*)"ethgetstat"; }
  void PrintHelp(CommandHandler_c* commandHandler ){}
  comResp_et Handle(CommandData_st* comData_);
};

class CommandEth_c :public CommandGroup_c
{

  Com_ethgetstat ethgetstat;



  public:
};
#endif




#endif