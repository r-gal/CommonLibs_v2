#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"

#include "EthernetUdp.hpp"



SocketUdp_c::SocketUdp_c(uint16_t port_) : Socket_c(port_,IP_PROTOCOL_UDP,nullptr)
{

}


void SocketUdp_c::FillUdpHeader(Packet_st* packet, uint16_t destPort, uint16_t udpPayloadSize) 
{


 packet->udpHeader.dstPort = htons(destPort);
 packet->udpHeader.srcPort = htons(port);
 packet->udpHeader.CS = 0;
 packet->udpHeader.Length = htons(sizeof(UdpHeader_st) + udpPayloadSize);
}


uint16_t SocketUdp_c::PrepareHeaders(Packet_st* packet, uint16_t udpPayloadSize,uint16_t destPort, uint8_t* destMac, uint32_t destIp)
{
  uint16_t headersSize = 0;

  if(destMac == nullptr)
  {
    FillIpHeader(packet,IP_PROTOCOL_UDP,BROADCAST_IP,udpPayloadSize + GetUdpHeaderSize());
    FillMacHeader(packet,nullptr,ETH_TYPE_IPV4);
  }
  else
  {
    FillIpHeader(packet,IP_PROTOCOL_UDP,destIp,udpPayloadSize + GetUdpHeaderSize());
    FillMacHeader(packet,destMac,ETH_TYPE_IPV4);
  }
  FillUdpHeader(packet,destPort,udpPayloadSize);

  headersSize = GetMacHeaderSize() + GetIpHeaderSize() + GetUdpHeaderSize();
  return headersSize;
}

void SocketUdp_c::HandlePacket(uint8_t* packet_p,uint16_t packetSize)
{
  




}

void SocketUdp_c::SendData(uint8_t* buf,uint16_t size)
{
  
  /*UDP_c* packet_p = new UDP_c(PACKET_INIT_ALLOCHEADER,(uint8_t*)buf,size);
  
  packet_p->FillUdpHeader();
  packet_p->FillIpHeader(IP_PROTOCOL_UDP,BROADCAST_IP);
  packet_p->FillMacHeader(NULL);

  ethTxPacketSig_c* sig_p = new ethTxPacketSig_c;

  sig_p->buffer = packet_p->buffer_p;
  sig_p->dataLen = packet_p->bufferSize ;
    sig_p->buffer2 = packet_p->buffer2_p;
  sig_p->dataLen2 = packet_p->bufferSize2 ;

  sig_p->Send();

  delete packet_p;*/

}