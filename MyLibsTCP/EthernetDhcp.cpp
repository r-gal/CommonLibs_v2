#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"


#include "EthernetDhcp.hpp"
#include "SignalList.hpp"
#include "EthernetBuffers.hpp"

//uint8_t MAC_c::ownMac[6] = {0x12,0x34,0x56,0x78,0x9A,0xBC};

tcpDhcpTimerSig_c dhcpTimerSig;

void DhcpTimerCallback( TimerHandle_t xTimer )
{
  dhcpTimerSig.timerIndicator = (uint32_t) pvTimerGetTimerID(xTimer);
  dhcpTimerSig.Send();

}

DHCP_c::DHCP_c(void)  : SocketUdp_c(DHCP_PORT)
{
  state = DHCP_STATE_IDLE;

  
}

void DHCP_c::Init(void)
{

  uint32_t timLen[3];
  timLen[DHCP_T1] = (configTICK_RATE_HZ*DEFAULT_LEASE_TIME*5)/10;
  timLen[DHCP_T2] = (configTICK_RATE_HZ*DEFAULT_LEASE_TIME*8)/10;
  timLen[DHCP_T3] = configTICK_RATE_HZ* DEFAULT_TIMEOUT;

/*
  timer[0] = xTimerCreate("",timLen[0],pdFALSE,( void * ) 0,DhcpTimerCallback);
  timer[1] = xTimerCreate("",timLen[1],pdFALSE,( void * ) 1,DhcpTimerCallback);
  timer[2] = xTimerCreate("",timLen[2],pdFALSE,( void * ) 2,DhcpTimerCallback);
  */
  
  for(int i=0;i<3;i++)
  {
    timer[i] = xTimerCreate("",timLen[i],pdFALSE,( void * ) i,DhcpTimerCallback);

  }
  




}

void DHCP_c::HandleLinkStateChange(uint8_t newState)
{
  if(newState == 1)
  {
    vTaskDelay(100);
    

    if(IP_c::ipConfig_p->GetDhcpEna())
    {  
      #if DEBUG_DHCP > 0  
      printf("DHCP: InitStart\n");
      #endif
      InitStart();
    }
    else
    {
      #if DEBUG_DHCP > 0  
      printf("DHCP: UdeAdm\n");
      #endif
      IP_c::ipConfig_p->UseAdministeredConfiguration();      
    }
  }
  else
  {
    #if DEBUG_DHCP > 0  
    printf("DHCP: DeinitStart\n");
    #endif
    DeinitStart();
  }
}

void DHCP_c::InitStart(void)
{
  SendFrame(DHCP_DISCOVER);
  state = DHCP_STATE_DISCOVER;

  uint32_t T3 = configTICK_RATE_HZ* DEFAULT_TIMEOUT;
  xTimerChangePeriod(timer[DHCP_T3],T3,1000);
  xTimerStart(timer[DHCP_T3],1000);
}

void DHCP_c::DeinitStart(void)
{
  IP_c::ipConfig_p->UpdateConfig(0,0,0,0,0);
  state = DHCP_STATE_IDLE;

  xTimerStop(timer[DHCP_T1],1000);
  xTimerStop(timer[DHCP_T2],1000);
  xTimerStop(timer[DHCP_T3],1000);
}

void DHCP_c::SendFrame(uint8_t oper)
{
 uint16_t length = 0;

  uint8_t* packet_p = new uint8_t[1024];
  Packet_st* packet = (Packet_st*) packet_p;

  DhcpFrame_st* frame_p = (DhcpFrame_st*)(&packet->udpPayload);
  uint16_t dataSize = CreateMessage(frame_p,oper);

  uint16_t headersSize = PrepareHeaders(packet,dataSize,DHCP_TX_PORT,nullptr,0);

  uint16_t packetSize = headersSize + dataSize;

  SendPacket(packet_p,packetSize,nullptr,0);

  delete[] packet_p;
}

uint16_t  DHCP_c::CreateMessage(DhcpFrame_st* frame_p,uint8_t oper)
{
  uint16_t dataSize = sizeof(DhcpFrame_st);
  memset(frame_p,0,dataSize);

  frame_p->adrLen = 6;
  frame_p->clientIp = 0;
  memcpy(frame_p->clientPhyAdr,GetOwnMac(),6);
  frame_p->type = 1;
  frame_p->oper = 1;
  frame_p->gateIp = 0;
  frame_p->serverIp = 0;
  frame_p->flags = 0;//0x80;
  if(oper == DHCP_DISCOVER)
  {
    frame_p->clientIp = 0;
    frame_p->serverIp = 0;
  }
  else
  {
    frame_p->clientIp = htonl(givenIp);
    frame_p->serverIp = htonl(dhcpServerIp);
  }
  frame_p->magicCookie = htonl(0x63825363);


  uint8_t* options_p =((uint8_t*)frame_p) + sizeof(DhcpFrame_st);

  dhcpOptions_st* options;

  uint32_t wIp = IP_c::ipConfig_p->GetIp();

  dhcpOptions_st optionsDiscover[] = 
  { 
  {OPT_DHCP_MSG_TYPE,1,{DHCP_DISCOVER}},
  {OPT_HOST_NAME,8,"Osc_v2  "},
  //{OPT_REQ_IP,4,{0,0,0,0}},
  {OPT_REQ_IP,4,{(wIp>>24)& 0xFF,(wIp>>16)& 0xFF,(wIp>>8)& 0xFF,(wIp) & 0xFF}},
  {OPT_REQ_PARAM,3,{OPT_SUBNET_MASK,OPT_GATEWAY,OPT_DNS_SERVER}},
  {OPT_END,0,NULL},
  };

  dhcpOptions_st optionsRequest[] = 
  { 
  {OPT_DHCP_MSG_TYPE,1,{DHCP_REQUEST}},
  {OPT_HOST_NAME,8,"Osc_v2  "},
  {OPT_REQ_IP,4,{(givenIp>>24)& 0xFF,(givenIp>>16)& 0xFF,(givenIp>>8)& 0xFF,(givenIp) & 0xFF}},
  {OPT_DHCP_SERVER,4, {(dhcpServerIp>>24)& 0xFF,(dhcpServerIp>>16)& 0xFF,(dhcpServerIp>>8)& 0xFF,(dhcpServerIp) & 0xFF}},
  {OPT_END,0,NULL},
  };

  if(oper == DHCP_DISCOVER)
  {
    options = optionsDiscover;
  }
  else
  {
    options = optionsRequest;    
  }
  

  int i=0;
  bool stop = false;

  while(stop == false)
  {
    *options_p = options[i].code;
    dataSize++;
    options_p++;

    if(options[i].code == OPT_END)
    {
      stop = true;
    }
    else
    {
      *options_p = options[i].size;
      dataSize++;
      options_p++;

      uint8_t optSize = options[i].size;

      for(int optIdx=0;optIdx<optSize;optIdx++)
      {
        *options_p = options[i].value[optIdx];
        dataSize++;
        options_p++;
      }



    }
    i++;


  }
  
  return dataSize;


}



void DHCP_c::HandlePacket(uint8_t* packet_p,uint16_t packetSize)
{
  //printf("Handle Rx DHCP \n");

  Packet_st* packet = (Packet_st*) packet_p;

  DhcpFrame_st* message_p = (DhcpFrame_st*) &(packet->udpPayload);

  if(memcmp(message_p->clientPhyAdr,GetOwnMac(),6) == 0)
  {
    uint8_t* options_p = (packet->udpPayload) + sizeof(DhcpFrame_st);
    int optBufSize = packet->udpHeader.Length - sizeof(DhcpFrame_st);

    /* Fetch parameters */

    bool ready = false;


    uint32_t gateway;
    bool gatewayValid = false;
    uint32_t subnetMask;
    bool subnetMaskValid = false;
    uint32_t dnsServer;
    bool dnsServerValid = false;
    uint32_t dhcpServer;
    bool dhcpServerValid = false;
    uint32_t leaseTime;
    bool leaseTimeValid = false;
    uint8_t oper;
    bool operValid = false;

    uint32_t ip = ntohl(message_p->givenClientIp);
    //char buf[20];
    //Ip2Str(buf,ip);
    //printf("GIVEN IP=%s\n",buf);

    while((ready == false) && (optBufSize >0))
    {
      uint8_t optCode = *options_p;
      uint8_t optBuf[4];

      if(optCode == OPT_SPACE)
      {
        options_p++;
        optBufSize--;
      }
      else if (optCode == OPT_END)
      {
        ready = true;
      }
      else
      {
        options_p++;
        optBufSize--;
        uint8_t optSize = *options_p;
        options_p++;
        optBufSize--;    
      
        for(int i=0;i< optSize;i++)
        {
          if(i<4)
          {
            optBuf[i] = *options_p;
          }
          options_p++;
          optBufSize--;   

        }

        if(optCode == OPT_DHCP_MSG_TYPE)
        {
          oper = optBuf[0];
          operValid = true;
        }
        else if(optCode == OPT_SUBNET_MASK)
        {
          subnetMask = (optBuf[0] << 24) | (optBuf[1] << 16) | (optBuf[2] << 8) | optBuf[3];

          //Ip2Str(buf,subnetMask);
          //printf("SUBNET=%s\n",buf);
          subnetMaskValid = true;

        }
        else if(optCode == OPT_GATEWAY)
        {
          gateway = (optBuf[0] << 24) | (optBuf[1] << 16) | (optBuf[2] << 8) | optBuf[3];
          //Ip2Str(buf,gateway);
          //printf("GATEWAY=%s\n",buf);
          gatewayValid = true;

        }
        else if(optCode == OPT_DNS_SERVER)
        {
          dnsServer = (optBuf[0] << 24) | (optBuf[1] << 16) | (optBuf[2] << 8) | optBuf[3];
          //Ip2Str(buf,dnsServer);
          //printf("DNS_SERVER=%s\n",buf);
          dnsServerValid = true;
        }
        else if(optCode == OPT_DHCP_SERVER)
        {
          dhcpServer = (optBuf[0] << 24) | (optBuf[1] << 16) | (optBuf[2] << 8) | optBuf[3];
          //Ip2Str(buf,dhcpServer);
          //printf("DHCP_SERVER=%s\n",buf);
          dhcpServerValid = true;
          dhcpServerIp = dhcpServer;
        }
        else if(optCode == OPT_LEASETIME)
        {
          leaseTime = (optBuf[0] << 24) | (optBuf[1] << 16) | (optBuf[2] << 8) | optBuf[3];
          //printf("LeaseTime=%d\n",leaseTime);
          leaseTimeValid = true;
        }
      }
    }
    /* fetch finished */

    if(operValid) 
    {
      if(oper == DHCP_OFFER)
      {
        givenIp = ip;
        SendFrame(DHCP_REQUEST);

 
        state = DHCP_STATE_REQUEST;
      }
      else if (oper == DHCP_ACK)
      {
        ipConfig_p->UpdateConfig(ip,dhcpServerIp,dnsServer,subnetMask,gateway);

        state = DHCP_STATE_CONFIGURED;
        if(leaseTimeValid)
        {
          usedLeaseTime = leaseTime;
        }
        else
        {
          usedLeaseTime = DEFAULT_LEASE_TIME;
        }
        uint32_t T1 = configTICK_RATE_HZ*usedLeaseTime*5/10;
        uint32_t T2 = configTICK_RATE_HZ*usedLeaseTime*8/10;
        uint32_t T3 = configTICK_RATE_HZ*usedLeaseTime;

        xTimerChangePeriod(timer[DHCP_T1],T1,1000);
        xTimerChangePeriod(timer[DHCP_T2],T2,1000);
        xTimerChangePeriod(timer[DHCP_T3],T3,1000);

        xTimerStart(timer[DHCP_T1],1000);
        xTimerStart(timer[DHCP_T2],1000);
        xTimerStart(timer[DHCP_T3],1000);

        ipChanged_c* sig_p = new ipChanged_c;
        sig_p->Send();
 
      }
      else if(oper == DHCP_NACK)
      {
        InitStart();
      }
    }
  }/* MAC compare */


 EthernetBuffers_c::DeleteBuffer(packet_p);

}

void DHCP_c::TimerHandler(uint8_t timerIndicator)
{
  if(timerIndicator == 1)
  {
    if(state == DHCP_STATE_CONFIGURED)
    {
      state = DHCP_STATE_RENEWING;
      SendFrame(DHCP_REQUEST);
    }
  }
  else if(timerIndicator == 2)
  {  
    if(state == DHCP_STATE_RENEWING)
    {
      state = DHCP_STATE_REBINDING;
      SendFrame(DHCP_REQUEST);
    }
  }
  else
  {
    InitStart();
  }

}



