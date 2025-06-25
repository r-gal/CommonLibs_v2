#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"


#include "EthernetIp.hpp"




void IP_c::FillIpHeader(Packet_st* packet, uint8_t protocol,uint32_t ipDest, uint16_t ipPayloadSize)
{

 packet->ipHeader.destIP = htonl(ipDest); 
 packet->ipHeader.srcIP = htonl(ipConfig_p->GetIp());
 packet->ipHeader.dscp_ecn = 0;
 packet->ipHeader.version_ihl = 0x45;
 packet->ipHeader.timeToLive = 255;
 packet->ipHeader.protocol = protocol;
 packet->ipHeader.flags_foffset = 0;
 packet->ipHeader.ident = 0;
 packet->ipHeader.length = htons(sizeof(IpHeader_st) + ipPayloadSize);
 packet->ipHeader.HeaderCS = 0;


}

uint32_t IP_c::GetSrcIp(Packet_st* packet)
{
  return ntohl(packet->ipHeader.srcIP) ;
}

IP_c::IP_c(void)
{
  
}





