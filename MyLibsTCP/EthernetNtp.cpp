#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"


#include "EthernetNtp.hpp"
#include "SignalList.hpp"
#include "EthernetArp.hpp"
#include "EthernetBuffers.hpp"

#include "TimeClass.hpp"

#define NTP_TO_UNIX_TIME_DIFF (2208988800)

uint32_t* NTP_c::ntpServerIp = nullptr;
uint32_t NTP_c::lastUpdate = 0;
uint32_t NTP_c::lastHardUpdate = 0;


void ntpTimerCallback( TimerHandle_t xTimer )
{
  //tcpTickSig.Send();
 
  NTP_c* ntp = (NTP_c*)pvTimerGetTimerID(xTimer);
  ntp->TimerHandler();
}

NTP_c::NTP_c(void)  : SocketUdp_c(NTP_PORT)
{
  timer = xTimerCreate("",pdMS_TO_TICKS(1000)*NTP_INTERVAL,pdTRUE,( void * ) this,ntpTimerCallback);
  ntpServerIp = (uint32_t*)(&(RTC->BKP8R));
  waitForResponse = false;
  currentIdx = 0;
}


  //
  

void NTP_c::HandleLinkStateChange(uint8_t newState)
{
  if(newState == 1)
  {
      #if DEBUG_NTP > 0  
      printf("NTP: InitStart\n");
      #endif
      xTimerStart(timer,1000);
      
  }
  else
  {
    #if DEBUG_NTP > 0  
    printf("NTP: DeinitStart\n");
    #endif
    xTimerStop(timer,1000);
    

  }
}

void NTP_c::TimerHandler(void)
{
  uint32_t gatewayIp = IP_c::ipConfig_p->GetGateway();

  if(gatewayIp != 0)
  {
    uint8_t* gatewayMac = Arp_c::FetchMac(gatewayIp);

    if(gatewayMac == nullptr)
    {
      xTimerChangePeriod(timer,pdMS_TO_TICKS(1000),0);
      xTimerStart(timer,0);
    }
    else
    {
      if(waitForResponse)
      {
        currentIdx++;

        if(currentIdx>= NTP_SERVERS_MAX)
        {
          currentIdx = 0;
        }

      }

      SendRequest(gatewayMac,GetServerIp(currentIdx));/*80.50.231.226*/
   //   SendRequest(gatewayMac,0xD4A06AE2);	/*212.160.106.226*/
     // SendRequest(gatewayMac,0xC21D82FC);  /*194.29.130.252*/
    //  SendRequest(gatewayMac,0xC3BBF537);  /*195.187.245.55*/
      waitForResponse = true;
      xTimerChangePeriod(timer,pdMS_TO_TICKS(1000*NTP_INTERVAL),0);
      xTimerStart(timer,0);
    }
  }

}

void NTP_c::SendRequest(uint8_t* gatewayMac, uint32_t ntpServerIp)
{
  #if DEBUG_NTP > 0
  printf("Send NTP packet \n");
  #endif

    uint16_t length = 0;

  uint8_t* packet_p = new uint8_t[1024];
  Packet_st* packet = (Packet_st*) packet_p;

  NtpFrame_st* frame_p = (NtpFrame_st*)(&packet->udpPayload);
  uint16_t dataSize = sizeof(NtpFrame_st);
  memset(frame_p,0,sizeof(NtpFrame_st));


  frame_p->flagsMode = 0xD9;
  frame_p->poll = 0x0A;
  frame_p->precision = 0xFA;
  frame_p->rootDispersion = htonl(0x90020100);

  uint32_t currentSec,currentSubSec;

  TimeUnit_c::MkPrecisonUtcTime(&currentSec,&currentSubSec);

  currentSec += NTP_TO_UNIX_TIME_DIFF;

  frame_p->transmitTimestampSec = htonl(currentSec);
  frame_p->transmitTimestampFrac = htonl(currentSubSec);

  uint16_t headersSize = PrepareHeaders(packet,dataSize,NTP_PORT,gatewayMac,ntpServerIp);

  uint16_t packetSize = headersSize + dataSize;

  SendPacket(packet_p,packetSize,nullptr,0);

  delete[] packet_p;

}

void NTP_c::HandlePacket(uint8_t* packet_p,uint16_t packetSize)
{
  #if DEBUG_NTP > 0
  printf("Received NTP packet \n");
  #endif
  waitForResponse = false;

  Packet_st* packet = (Packet_st*) packet_p;

  NtpFrame_st* message_p = (NtpFrame_st*) &(packet->udpPayload);

  uint32_t currentSec,currentSubSec;
  TimeUnit_c::MkPrecisonUtcTime(&currentSec,&currentSubSec);

  uint64_t T1,T2,T3,T4;


  T1 = ((uint64_t)((ntohl(message_p->originTimestampSec) - NTP_TO_UNIX_TIME_DIFF)) << 32) | ntohl(message_p->originTimestampFrac);
  T2 = ((uint64_t)((ntohl(message_p->receiveTimestampSec) - NTP_TO_UNIX_TIME_DIFF)) << 32 )| ntohl(message_p->receiveTimestampFrac);
  T3 = ((uint64_t)((ntohl(message_p->transmitTimestampSec) - NTP_TO_UNIX_TIME_DIFF)) << 32) | ntohl(message_p->transmitTimestampFrac);
  T4 = ((uint64_t)(currentSec) << 32) | currentSubSec;

  if((T4>T1) && (T3>T2))
  {
    uint64_t totalTime = T4-T1;
    uint64_t calcTime = T2 + (T3-T2)/2 - totalTime/2;

    bool diffSign;
    uint32_t diffSec,diffSubSec;
    uint64_t tmp;
    if(calcTime > T4)
    {
      diffSign = true;
      tmp = calcTime-T4;

    }
    else
    {
      diffSign = false;
      tmp = T4-calcTime;
    }
    diffSubSec = tmp & 0xFFFFFFFF;
    diffSec = tmp>>32;

    diffSubSec = diffSubSec / (4294967);
    #if DEBUG_NTP > 0
    printf("Time diff %s sec=%d, ms=%d\n",diffSign ? "POS" : "NEG", diffSec,diffSubSec);
    #endif

    uint32_t newSec,newSubSec;
    newSec = calcTime>>32;

    lastUpdate = newSec;

    if(diffSec > 1)
    {
      newSubSec = calcTime & 0xFFFFFFFF;
      TimeUnit_c::SetUtcTime(newSec,newSubSec);
      lastHardUpdate = newSec;
    }
    else if(  diffSubSec > 100)
    {
      TimeUnit_c::ShiftTime(diffSign,diffSubSec/2);
    }
  
  
  
  } 


  EthernetBuffers_c::DeleteBuffer(packet_p);
}

void NTP_c::SetServerIp(uint8_t idx, uint32_t ip)
{
  if(idx < NTP_SERVERS_MAX)
  {
    //HAL_PWR_EnableBkUpAccess();
    ntpServerIp[idx] = ip;
    //HAL_PWR_DisableBkUpAccess();
  }
}

uint32_t NTP_c::GetServerIp(uint8_t idx)
{
  if(idx < NTP_SERVERS_MAX)
  {
    return ntpServerIp[idx] ;
  }
  else
  {
    return 0;
  }
}


/*****************command section **************************/

#if USE_COMMANDS
CommandNtp_c commandNtp;

comResp_et Com_ntp::Handle(CommandData_st* comData)
{

  uint32_t newServerIp;
  uint32_t idx;


  bool newServerIpValid = FetchParameterIp(comData,"IP",&newServerIp);

  bool idxValid = FetchParameterValue(comData,"IDX",&idx,0,NTP_SERVERS_MAX-1);

  if(idxValid && newServerIpValid)
  {
    NTP_c::SetServerIp(idx, newServerIp);
  }
  else if(idxValid != newServerIpValid)
  {
    return COMRESP_NOPARAM;
  }



  char* strBuf = new char[512];
  char* strPtr = strBuf;

  sprintf(strPtr,"NTP SERVERS LIST:\n");
  strPtr += strlen(strPtr);

  for(int i=0;i<NTP_SERVERS_MAX;i++)
  {
    sprintf(strPtr,"NTP server %d: ",i);
    strPtr += strlen(strPtr);

    uint32_t ip = NTP_c::GetServerIp(i);

    IpConfig_c::PrintIp(strPtr,ip,"");

    strPtr += strlen(strPtr);

    strcpy(strPtr,"\n"); strPtr++;  

  }

  sprintf(strPtr,"Last update: ");
  strPtr += strlen(strPtr);

  SystemTime_st pxTime;
  TimeUnit_c::GmTime( &pxTime, NTP_c::lastUpdate + TimeUnit_c::GetTimeZoneOffset() );
  TimeUnit_c::PrintTime(strPtr,&pxTime);

  strPtr += strlen(strPtr);

  sprintf(strPtr,"Last hard update: ");
  strPtr += strlen(strPtr);

  TimeUnit_c::GmTime( &pxTime, NTP_c::lastHardUpdate + TimeUnit_c::GetTimeZoneOffset() );
  TimeUnit_c::PrintTime(strPtr,&pxTime);

  strPtr += strlen(strPtr);
  

  Print(comData->commandHandler,strBuf);

  delete[] strBuf;

  return COMRESP_OK;
}

#endif



