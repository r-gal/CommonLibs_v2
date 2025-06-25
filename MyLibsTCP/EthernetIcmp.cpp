#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"

#include "EthernetIcmp.hpp"
#include "EthernetBuffers.hpp"

ICMP_c::ICMP_c(void) : Socket_c(0,IP_PROTOCOL_ICMP,nullptr)
{

}

void ICMP_c::FillIcmpHeader(Packet_st* packet, uint32_t rest)
{

 packet->icmpHeader.type = 0;
 packet->icmpHeader.code = 0;
 packet->icmpHeader.checkSum = 0;
 packet->icmpHeader.rest = rest;

}


void ICMP_c::HandlePacket(uint8_t* packet_p,uint16_t length)
{
  printf("Handle ICMP packet\n");

  Packet_st* packet = (Packet_st*) packet_p;
  uint16_t ipSize = ntohs(packet->ipHeader.length);

  FillMacHeader(packet,packet->macHeader.MAC_Src,ETH_TYPE_IPV4);

  uint32_t ip = ntohl(packet->ipHeader.srcIP);
  FillIpHeader(packet,IP_PROTOCOL_ICMP,ip,0);

  packet->icmpHeader.type = 0; /* reply */
  packet->icmpHeader.checkSum = 0;

  packet->ipHeader.length = htons(ipSize);

  uint32_t packetLength = GetMacHeaderSize() + ipSize;

  SendPacket(packet_p,packetLength,nullptr,0);

  EthernetBuffers_c::DeleteBuffer(packet_p);

}

