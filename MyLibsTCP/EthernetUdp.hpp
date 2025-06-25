#ifndef ETHERNETUDP_H
#define ETHERNETUDP_H

//#include "common.hpp"
#include "EthernetCommon.hpp"
#include "EthernetSocket.hpp"
#include "EthernetIp.hpp"



class SocketUdp_c : public Socket_c
{
  
  
  uint16_t GetUdpHeaderSize(void) { return 8; }

  void FillUdpHeader(Packet_st* packet, uint16_t destPort, uint16_t udpPayloadSize); 

  public:

  void Listen(void) {}

  uint32_t destIp;
  uint8_t destMac[6];

  uint16_t PrepareHeaders(Packet_st* packet, uint16_t destPort, uint16_t udpPayloadSize,  uint8_t* destMac, uint32_t destIp);

  SocketUdp_c(uint16_t port_)  ; 

  void HandlePacket(uint8_t* packet_p,uint16_t packetSize);

  void SendData(uint8_t* buf,uint16_t size);

  void Tick(void) {}

  void PrintInfo(char* buffer) {}
};








#endif