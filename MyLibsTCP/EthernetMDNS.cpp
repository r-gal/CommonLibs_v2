#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"


#include "EthernetMDNS.hpp"
#include "SignalList.hpp"

uint8_t MDNS_c::mdnsMac[ 6 ] = MDNS_MAC;

uint8_t* MDNS_c::GetMac(void)
{
  return mdnsMac;
}

void MDNS_c::HandlePacket(uint8_t* packet_p,uint16_t packetSize)
{
  printf("Received MDNS packet \n");

  Packet_st* packet = (Packet_st*) packet_p;

  MdnsFrame_st* message_p = (MdnsFrame_st*) &(packet->udpPayload);
  uint16_t flags = ntohs(message_p->flags);
  if((flags & MDNS_FLAG_QR) == 0)
  {
    /* question */

    char hostName[16];
    strcpy(hostName,HOST_NAME);
    uint8_t hostNameLen = strlen(hostName);

    uint8_t* querry_ptr = message_p->payload;

    uint8_t firstLabelLength = *querry_ptr;

    

    if((firstLabelLength == hostNameLen) && (memcmp(hostName,querry_ptr+1,hostNameLen) == 0))
    {
      printf("MDNS wanted name match \n");

      uint8_t totalNameLength = 0;

      while(*querry_ptr != 0)
      {
        totalNameLength += *querry_ptr;
        querry_ptr += *querry_ptr;
        querry_ptr++;
      }
      querry_ptr++;      

      uint16_t rrType = ((*querry_ptr) << 8)| (*(querry_ptr+1));

      if(rrType == 0x0001)
      {
        printf("MDNS type match \n");

        /* prepare answer */

        uint8_t srcMac[6];
        uint32_t srcIp = packet->ipHeader.srcIP;
        memcpy(srcMac,packet->macHeader.MAC_Src,6);

        message_p->NoOfQuestions = 0;
        message_p->noOfAuthorityRRs = 0;
        message_p->noOfAnswers = htons(2);
        message_p->noOfAdditionalRRs = 0;
        message_p->flags = htons(MDNS_FLAG_QR | MDNS_FLAG_AA);

        strcpy(hostName,(char*)message_p->payload);
        hostNameLen = strlen(hostName)+1;

        uint32_t ip = IP_c::ipConfig_p->GetIp();

        uint8_t* msgPtr = message_p->payload;
        
        char ipStr[32];
        sprintf(ipStr,".%d.%d.%d.%d.in-addr.arpa",(ip>>24) & 0xFF, (ip>>16) & 0xFF, (ip>>8) & 0xFF, (ip>>0) & 0xFF);

        uint8_t ipStrLen = strlen(ipStr)+1;
               
        int idx = ipStrLen-2;
        uint8_t labelLen = 0;
        while( idx >= 0)
        {
          if(ipStr[idx] == '.')
          {
            ipStr[idx] = labelLen;
            labelLen = 0;
          }
          else
          {
            labelLen++;
          }
          idx--;
        }

        uint8_t* answerPtr = message_p->payload;
        uint8_t totalAnswerLength = 0;
        /* prepare answer 1 */

        memcpy(answerPtr,ipStr,ipStrLen);
        answerPtr += ipStrLen;
        totalAnswerLength += ipStrLen;

        /* add rrtype */

        *answerPtr++ = 0x00;
        *answerPtr++ = 0x0C;
        totalAnswerLength +=2;

        /* add rrClass */

        *answerPtr++ = 0x00;
        *answerPtr++ = 0x01;
        totalAnswerLength +=2;

        /* addTimeInterval */

        *answerPtr++ = 0x00;
        *answerPtr++ = 0x00;
        *answerPtr++ = 0x00;
        *answerPtr++ = 0x78;
        totalAnswerLength +=4;

        /* add Length */

        *answerPtr++ = (hostNameLen>>8);
        *answerPtr++ = (hostNameLen&0xFF);
        totalAnswerLength +=2;

        /*add name */

        memcpy((char*)answerPtr,hostName,hostNameLen);
        totalAnswerLength += hostNameLen;
        answerPtr+= hostNameLen;

        /* prepare answer 2 */

        /*add name */

        *answerPtr++ = 0xC0;
        *answerPtr++ = 0x33;
        totalAnswerLength +=2;

        /* add rrtype */

        *answerPtr++ = 0x00;
        *answerPtr++ = 0x01;
        totalAnswerLength +=2;

        /* add rrClass */

        *answerPtr++ = 0x00;
        *answerPtr++ = 0x01;
        totalAnswerLength +=2;

        /* addTimeInterval */

        *answerPtr++ = 0x00;
        *answerPtr++ = 0x00;
        *answerPtr++ = 0x00;
        *answerPtr++ = 0x78;
        totalAnswerLength +=4;

        /* add Length */

        *answerPtr++ = 0x00;
        *answerPtr++ = 0x04;
        totalAnswerLength +=2;

        /* add ip*/ 

        *answerPtr++ = (ip>>24)&0xFF;
        *answerPtr++ = (ip>>16)&0xFF;
        *answerPtr++ = (ip>>8)&0xFF;
        *answerPtr++ = (ip>>0)&0xFF;
        totalAnswerLength +=4;

        /* send frame */
      

        uint16_t packetLength = totalAnswerLength + 12; /* mdns header length */

        packetLength += PrepareHeaders(packet,packetLength ,MDNS_PORT,GetMac(),MDNS_IP);


        packet->ipHeader.srcIP = srcIp;
        memcpy(packet->macHeader.MAC_Src,srcMac,6);

        SendPacket(packet_p,packetLength,nullptr,0);











      }








    }
  }


  delete packet_p;
}

MDNS_c::MDNS_c(void)  : SocketUdp_c(MDNS_PORT)
{

}