#ifndef ETHERNETCOMMON_H
#define ETHERNETCOMMON_H

//#include "common.hpp"





uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);
uint32_t ntohl(uint32_t netlong);
uint16_t ntohs(uint16_t netshort);

void Ip2Str(char* buffer,uint32_t ip);

enum PACKET_INIT_MODE_et
{
  PACKET_INIT_NOALLOC,
  PACKET_INIT_ALLOCPACKET,
  PACKET_INIT_ALLOCHEADER
};










#endif