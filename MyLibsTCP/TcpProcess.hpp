#ifndef TCP_PROCESS_H
#define TCP_PROCESS_H

#include "SignalList.hpp"
#include "GeneralConfig.h"

#include "TcpDataDef.hpp"
#include "EthernetArp.hpp"
#include "EthernetDhcp.hpp"
#include "EthernetIcmp.hpp"
#if USE_MDNS == 1
#include "EthernetMDNS.hpp"
#endif
#if USE_NTP == 1
#include "EthernetNtp.hpp"
#endif
/*
#include "Ethernet.hpp"
#include "CommandSystem.hpp"
#include "sdCard.hpp"
#include "RngClass.hpp"
#include "TimeClass.hpp"
#include "BootUnit.hpp"

*/

class TcpProcess_c : public process_c
{

  Arp_c arpUnit;
  DHCP_c* dhcpUnit;
  ICMP_c* icmpUnit;
  #if USE_MDNS == 1
  MDNS_c mdnsUnit;
  #endif
  #if USE_NTP == 1
  NTP_c* ntpUnit;
  #endif



  void RouteRxMsg(tcpRxEventSig_c* recSig_p);
  void HandleLinkEvent(uint8_t linkState);

  void ScanTcpSocketsSend(void);
  void TcpSocketsTick(void);

  void AddNewSocketToList(Socket_c* socket);
  

  public :

  TcpProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId);

  void main(void);

  

};



#endif