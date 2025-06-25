#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"

#include "EthernetMac.hpp"
//#include "EthernetConfig.hpp"
#include "TcpDataDef.hpp"
#include "Ethernet_TX.hpp"


MAC_Initializer_c dummyInstance; /*  instance to call constructor */

uint8_t MAC_c::ownMac[ 6 ] = OWN_MAC;   

MAC_c::MAC_c(void)
{


}

void MAC_c::InitMac(void)
{
  #if USE_UID_TO_MAC == 1

  int idVal = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
  ownMac[2] = (idVal>>24) & 0xFF;
  ownMac[3] = (idVal>>16) & 0xFF;
  ownMac[4] = (idVal>>8) & 0xFF;
  ownMac[5] = (idVal) & 0xFF;
  #endif
}

uint8_t* MAC_c::GetOwnMac(void)
{

  return ownMac;
}

void MAC_c::FillMacHeader(Packet_st* packet, uint8_t* mac, uint16_t ethType)
{
  if(mac == NULL)
  {
    memset(packet->macHeader.MAC_Dest,0xFF,6);
  }
  else
  {
    memcpy(packet->macHeader.MAC_Dest,mac,6);
  }
  //memset(packet->macHeader.MAC_Src,0,6); /* OWN MAC will be overwritten by hardware */
  memcpy(packet->macHeader.MAC_Src,GetOwnMac(),6);
  packet->macHeader.ethType = htons(ethType);


}

uint8_t* MAC_c::GetSrcMac(Packet_st* packet)
{
  return packet->macHeader.MAC_Src;
}

bool MAC_c::SendPacket(uint8_t* buf1, uint16_t buf1len, uint8_t* buf2, uint16_t buf2len)
{
  return EthernetTxProcess_c::SendBuffer(buf1+2,buf1len,buf2,buf2len);
}


#ifdef __cplusplus
 extern "C" {
#endif
uint8_t* GetMacAddress(void)
{
  return MAC_c::GetOwnMac();
}
#ifdef __cplusplus
}
#endif