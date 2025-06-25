 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "TcpProcess.hpp"
#include "Ethernet_TX.hpp"
#include "EthernetTcp.hpp"
#include "EthernetBuffers.hpp"

tcpSendSig_c tcpSendTimSig;
tcpTickSig_c tcpTickSig;

void tcpSendTimerCallback( TimerHandle_t xTimer )
{
  tcpSendTimSig.Send();
}

void tcpTickTimerCallback( TimerHandle_t xTimer )
{
  tcpTickSig.Send();
}


TcpProcess_c::TcpProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId) : process_c(stackSize,priority,queueSize,procId,"TCPPROC")
{

  TimerHandle_t timer = xTimerCreate("",pdMS_TO_TICKS(100),pdTRUE,( void * ) 0,tcpSendTimerCallback);
  xTimerStart(timer,0);

  timer = xTimerCreate("",pdMS_TO_TICKS(1000),pdTRUE,( void * ) 0,tcpTickTimerCallback);
  xTimerStart(timer,0);
  

}

void TcpProcess_c::main(void)
{
  #if USE_DHCP == 1
  dhcpUnit = new DHCP_c;
  #endif
  icmpUnit = new ICMP_c;
  #if USE_NTP == 1
  ntpUnit = new NTP_c;
  #endif

  #if USE_DHCP == 1
  dhcpUnit->Init();
  #endif

   /********************************************/

  while(1)
  {
    releaseSig = true;
    RecSig();
    uint8_t sigNo = recSig_p->GetSigNo();

    

    switch(sigNo)
    {
      case SIGNO_TCP_RXEVENT:
      {
        RouteRxMsg((tcpRxEventSig_c*) recSig_p);
      }
      break;
      case SIGNO_TCP_LINKEVENT:
      {
        HandleLinkEvent(((tcpLinkEventSig_c*) recSig_p)->linkState);
      }
      break;
      case SIGNO_SOCKET_TCP_REQUEST:
      {
        socketTcpRequestSig_c* sig_p = (socketTcpRequestSig_c*) recSig_p;
        sig_p->socket->Request(sig_p);
        break;
      }
      break;
      case SIGNO_SOCKET_SEND_REQUEST:
      {
        socketSendReqSig_c* sig_p = (socketSendReqSig_c*) recSig_p;
        sig_p->socket->HandleDataSend(sig_p);
        releaseSig = false;
        break;
      }
      break;
      case SIGNO_SOCKET_REC_REQUEST:
      {
        socketReceiveReqSig_c* sig_p = (socketReceiveReqSig_c*) recSig_p;
        sig_p->socket->HandleDataReceive(sig_p);
        releaseSig = false;
        break;
      }      
      #if USE_DHCP == 1
      case SIGNO_TCP_DHCP_TIMEOUT:
      {
        dhcpUnit->TimerHandler(((tcpDhcpTimerSig_c*) recSig_p)->timerIndicator);
        releaseSig = false;
      }
      break;
      #endif
      case SIGNO_TCP_SEND_TIMER:
      {
        ScanTcpSocketsSend();
        releaseSig = false;
      }
      break;

      case SIGNO_TCP_TICK:
      {
        TcpSocketsTick();
        arpUnit.Tick1s();
        releaseSig = false;
      }
      break;
      #if USE_CONFIGURABLE_IP == 1
      case SIGNO_IP_CHANGED:
        arpUnit.SendRequest(IP_c::ipConfig_p->GetIp());
        break;
      #endif
      default:
      break;
      case SIGNO_SOCKET_ADD:
        Socket_c* socket = ((socketAddSig_c*) recSig_p)->socket;
        socket->AddNewSocketToList();
        break;

    }
    if(releaseSig) { delete  recSig_p; }
 
  }
}



void TcpProcess_c::RouteRxMsg(tcpRxEventSig_c* recSig_p)
{ 
  uint16_t packetLen = recSig_p->dataSize;
  Packet_st* packet = (Packet_st*) recSig_p->dataBuffer;
  uint16_t ethType = ntohs(packet->macHeader.ethType);
  if(ethType == ETH_TYPE_IPV4)
  {
    uint16_t port = 0;
    if(packet->ipHeader.protocol == IP_PROTOCOL_UDP)
    {
      port = ntohs(packet->udpHeader.dstPort);
    }
    else if(packet->ipHeader.protocol == IP_PROTOCOL_TCP)
    {
      port = ntohs(packet->tcpHeader.dstPort);
    } 
    Socket_c* socket = Socket_c::GetSocket(port,packet->ipHeader.protocol);
    if(socket != nullptr)
    {
      socket->HandlePacket(recSig_p->dataBuffer,packetLen);
    }
    else
    {
      /* no socket, discard packet */
      EthernetBuffers_c::DeleteBuffer(recSig_p->dataBuffer);
    }
  }
  else if(ethType == ETH_TYPE_ARP)
  { 
    arpUnit.HandlePacket(recSig_p->dataBuffer,packetLen);
  }
  else
  {
    /* should never happen */
    EthernetBuffers_c::DeleteBuffer(recSig_p->dataBuffer);
  }
}
void TcpProcess_c::HandleLinkEvent(uint8_t linkState)
{
  arpUnit.CleanArray();
  #if USE_DHCP == 1
  dhcpUnit->HandleLinkStateChange(linkState);
  #else
  IP_c::ipConfig_p->UseAdministeredConfiguration(); 
  #endif
  #if USE_NTP == 1
  ntpUnit->HandleLinkStateChange(linkState);
  #endif

}








void TcpProcess_c::ScanTcpSocketsSend(void)
{
  Socket_c* socket = Socket_c::first;

  while(socket != nullptr)
  {
    if(socket->protocol == IP_PROTOCOL_TCP)
    {
      SocketTcp_c* tcpSocket = ((SocketTcp_c* )socket);
      tcpSocket->LoopOverChildList(1);
    }

    socket = socket->next;
  }
}

void TcpProcess_c::TcpSocketsTick(void)
{
  Socket_c* socket = Socket_c::first;
  Socket_c* nextSocket;

  while(socket != nullptr)
  {
    nextSocket = socket->next;
    if(socket->protocol == IP_PROTOCOL_TCP)
    {
      ((SocketTcp_c* )socket)->LoopOverChildList(0);
    }

    socket = nextSocket;
  }

}
