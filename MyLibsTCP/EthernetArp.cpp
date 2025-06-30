#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "TcpDataDef.hpp"
#include "EthernetArp.hpp"
#include "EthernetIp.hpp"
#include "EthernetIpConfig.hpp"
#include "EthernetBuffers.hpp"

#define P_TYPE_IPV4 0x0800
#define H_TYPE_ETH 0x0001 

ArpEntry_st Arp_c::arpArray[ARP_ARRAY_SIZE];

void Arp_c::HandlePacket(uint8_t* packet_p,uint16_t length)
{
  //printf("Handle ARP packet\n");

  Packet_st* packet = (Packet_st*) packet_p;

  uint32_t senderIp = packet->arpHeader.pAdrS[0]<<24 | packet->arpHeader.pAdrS[1]<<16 | packet->arpHeader.pAdrS[2]<<8 | packet->arpHeader.pAdrS[3] ;

  AddEntry(senderIp,packet->arpHeader.hAdrS);

  switch(ntohs(packet->arpHeader.oper))
  {
    case 1: /* request */
    {
      uint32_t tIp = packet->arpHeader.pAdrR[0]<<24 | packet->arpHeader.pAdrR[1]<<16 | packet->arpHeader.pAdrR[2]<<8 | packet->arpHeader.pAdrR[3] ;
      if(tIp == IP_c::ipConfig_p->GetIp())
      {
        /* prepare reply */
        uint8_t dMac[6];
        memcpy(dMac, packet->macHeader.MAC_Src,6);
        uint32_t dIp = senderIp;

        FillMacHeader(packet,dMac,ETH_TYPE_ARP);

        FillArpHeader(packet,2,dMac,dIp);

        uint16_t packetSize = GetMacHeaderSize() + GetArpHeaderSize();

        SendPacket(packet_p,packetSize,nullptr,0);
        EthernetBuffers_c::DeleteBuffer(packet_p);
 
      }
      else
      {
        EthernetBuffers_c::DeleteBuffer(packet_p);
      }
      break;
    }
    default:
      EthernetBuffers_c::DeleteBuffer(packet_p);
      break;
  }



}

void Arp_c::SendRequest(uint32_t requestedIp)
{
  Packet_st* packet = new Packet_st;

  uint8_t dMac[6];
  memset(dMac,0xFF,6);

  FillMacHeader(packet,dMac,ETH_TYPE_ARP);
  FillArpHeader(packet,1,dMac,requestedIp);

  uint16_t packetSize = GetMacHeaderSize() + GetArpHeaderSize();

  SendPacket((uint8_t*) packet,packetSize,nullptr,0);


  delete packet;


}

void Arp_c::IpToByteArray(uint8_t* array, uint32_t ip)
{
  array[0] = (ip >> 24) & 0xFF;
  array[1] = (ip >> 16) & 0xFF;
  array[2] = (ip >> 8) & 0xFF;
  array[3] = (ip >> 0) & 0xFF;
}

void Arp_c::FillArpHeader(Packet_st* packet, uint8_t oper, uint8_t*  destMac, uint32_t destIp)
{
  packet->arpHeader.oper = htons(oper);
  packet->arpHeader.hAdrLen = 6;
  packet->arpHeader.pAdrLen = 4;
  packet->arpHeader.hType = htons(H_TYPE_ETH);
  packet->arpHeader.pType = htons(P_TYPE_IPV4);
  memcpy(packet->arpHeader.hAdrR,destMac,6);
  memcpy(packet->arpHeader.hAdrS,GetOwnMac(),6);
  IpToByteArray(packet->arpHeader.pAdrR, destIp);
  IpToByteArray(packet->arpHeader.pAdrS, IP_c::ipConfig_p->GetIp());
}

void Arp_c::CleanArray(void)
{
  memset(arpArray,0,sizeof(ArpEntry_st) * ARP_ARRAY_SIZE);
}

uint8_t* Arp_c::FetchMac(uint32_t ip)
{
  for(int i=0;i<ARP_ARRAY_SIZE;i++)
  {
    if(arpArray[i].ip == ip)
    {  
      return arpArray[i].mac;
    }
  }

  /* send request */

  SendRequest(ip);

  return nullptr;


}
void Arp_c::AddEntry(uint32_t ip, uint8_t* mac)
{
  int  firstFreeIdx = -1;
  int  oldestIdx = -1;
  uint16_t oldestAge = 0;
  int idx = -1;
  for(int i=0;i<ARP_ARRAY_SIZE;i++)
  {
    if(arpArray[i].ip == ip)
    {  
      idx = i;
      break;
    }
    else if((arpArray[i].ip == 0) && (firstFreeIdx < 0))
    {
      firstFreeIdx = i;
    }
    else if(arpArray[i].age > oldestAge)
    {
      oldestAge = arpArray[i].age;
      oldestIdx = i;
    }
  }

  if(idx == -1)
  {
    if(firstFreeIdx >= 0)
    {
      idx = firstFreeIdx;
    }
    else
    {
      idx = oldestIdx;
    }
  }
  memcpy(arpArray[idx].mac,mac,6);
  arpArray[idx].age = 0;
  arpArray[idx].ip = ip;

}

void Arp_c::Tick1s(void)
{
  for(int i=0;i<ARP_ARRAY_SIZE;i++)
  {
    if(arpArray[i].ip != 0)
    {  
      arpArray[i].age++;
      if(arpArray[i].age == ARP_MAX_AGE/2)
      {
        SendRequest(arpArray[i].ip);
      } 


      if(arpArray[i].age > ARP_MAX_AGE)
      {
        memset(&arpArray[i],0,sizeof(ArpEntry_st));
      }      
    }
  }
}

/*****************command section **************************/

#if CONF_USE_COMMANDS == 1


CommandArp_c commandArp;

comResp_et Com_arp::Handle(CommandData_st* comData)
{

  char* strBuf = new char[64];

  sprintf(strBuf,"ARP ARRAY:\n");
  Print(comData->commandHandler,strBuf);

  for(int i=0;i<ARP_ARRAY_SIZE;i++)
  {
    ArpEntry_st* arpEntry = Arp_c::GetEntry(i);

    if(arpEntry->ip != 0)
    {
      IpConfig_c::PrintIp(strBuf,arpEntry->ip,(char*)"");
      uint8_t len = strlen(strBuf);
      memset(strBuf+len,' ',16-len);
      sprintf(strBuf + 16,"%02X.%02X.%02X.%02X.%02X.%02X %ds\n",arpEntry->mac[0],arpEntry->mac[1],arpEntry->mac[2],arpEntry->mac[3],arpEntry->mac[4],arpEntry->mac[5],arpEntry->age);
      Print(comData->commandHandler,strBuf);
    }
  }  

  delete[] strBuf;

  return COMRESP_OK;
}

#endif