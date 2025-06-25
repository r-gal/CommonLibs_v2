#ifndef ETHERNETIP_H
#define ETHERNETIP_H

#include "TcpDataDef.hpp"
#include "EthernetCommon.hpp"
#include "EthernetMac.hpp"
#include "EthernetIpConfig.hpp"

#define BROADCAST_IP 0xFFFFFFFF

class DHCP_c;



class IP_c :public MAC_c
{
  public:

  static IpConfig_c* ipConfig_p;
  IP_c(void);

  uint16_t GetIpHeaderSize(void) { return 20; }

  void FillIpHeader(Packet_st* packet,uint8_t protocol,uint32_t ipDest, uint16_t ipPayloadSize);

  uint32_t GetSrcIp(Packet_st* packet);

};







#endif