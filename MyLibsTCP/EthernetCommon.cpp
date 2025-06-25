#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"

#include "EthernetCommon.hpp"



uint32_t htonl(uint32_t hostlong) 
{     
  uint8_t bytes[4]; 

  bytes[0] = hostlong;
  bytes[1] = hostlong>>8;
  bytes[2] = hostlong>>16;
  bytes[3] = hostlong>>24;

  uint32_t netLong = bytes[0]<<24 | bytes[1] <<16 | bytes[2]<<8 | bytes[3];
  return netLong;

}
uint16_t htons(uint16_t hostshort) 
{  
  uint8_t bytes[2]; 

  bytes[0] = hostshort;
  bytes[1] = hostshort>>8;

  uint16_t netlong = bytes[0]<<8 | bytes[1];
  return netlong;
 }
uint32_t ntohl(uint32_t netlong) 
{  
  uint8_t bytes[4]; 

  bytes[0] = netlong;
  bytes[1] = netlong>>8;
  bytes[2] = netlong>>16;
  bytes[3] = netlong>>24;

  uint32_t hostlong = bytes[0]<<24 | bytes[1] <<16 | bytes[2]<<8 | bytes[3];
  return hostlong;
 }
uint16_t ntohs(uint16_t netshort)
{  
  uint8_t bytes[2]; 

  bytes[0] = netshort;
  bytes[1] = netshort>>8;

  uint16_t hostshort = bytes[0]<<8 | bytes[1];
  return hostshort;
}

void Ip2Str(char* buffer,uint32_t ip)
{
  uint8_t b[4]; 

  b[0] = ip;
  b[1] = ip>>8;
  b[2] = ip>>16;
  b[3] = ip>>24;

  sprintf(buffer,"%d:%d:%d:%d",b[3],b[2],b[1],b[0]);
}