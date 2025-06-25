#ifndef ETHERNETICMP_H
#define ETHERNETICMP_H

#include "TcpDataDef.hpp"
#include "EthernetCommon.hpp"
#include "EthernetIp.hpp"
#include "EthernetMac.hpp"
#include "EthernetSocket.hpp"


class ICMP_c : public Socket_c
{
  uint32_t destIp;
  uint8_t destMac[6];
  uint32_t headerRest;

  uint16_t GetIcmpHeaderSize(void) { return 8; }
  void FillIcmpHeader(Packet_st* packet, uint32_t rest); 

  public:

  ICMP_c(void);

  void HandlePacket(uint8_t* packet_p,uint16_t length);

  void SendData(uint8_t* buf,uint16_t size);

  void Tick(void) {}

  void PrintInfo(char* buffer) {}
};

#endif
