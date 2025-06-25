#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"

#include "EthernetTcp.hpp"
#include "RngClass.hpp"
#include "EthernetBuffers.hpp"

//const uint8_t synOptions[] = {0x02,0x04,0x05,0xB4,0x01,0x03,0x03,0x00,0x01,0x01,0x04,0x02};
//#define SYN_OPT_LENGTH 12
const uint8_t synOptions[] = {0x02,0x04,0x05,0xB4,0x01,0x03,0x03,0x00};
#define SYN_OPT_LENGTH 8

const char* stateStr[] =  
{
  "INIT",
  "LISTEN",
  "SYN_SENT",
  "SYN_RECEIVED",
  "ESTABLISHED",
  "FIN_WAIT_1",
  "FIN_WAIT_2",
  "CLOSE_WAIT",
  "CLOSING",
  "LAST_ACK",
  "TIME_WAIT",
  "CLOSED"
} ;




SocketTcp_c::SocketTcp_c(uint16_t port, Socket_c* parentSocket) : Socket_c(port,IP_PROTOCOL_TCP, parentSocket) 
{

  state = INIT;
  reuseListenSocket = false;
  childMax = 0;
  acceptWaitingTask = 0;
  closeAfterSent = false;
  deleteWhenPossible = false;
  timeWait = 0;
  keepAliveCnt = 0;
  keepAliveProbesCnt = 0;
  ackToSent = false;
  rxBufferSize = TCP_RX_BUFFER_SIZE;
  task = 0;

#if DEBUG_TCP > 0

  PrintSocketInfo();
  printf(" Create TCP socket");
  if(parentSocket != nullptr)
  { 
    printf(" Socket has now %d/%d child",parentSocket->childCnt,parentSocket->childMax);
  }
  printf("\n");

#endif
}

SocketTcp_c::~SocketTcp_c(void)
{
  #if DEBUG_TCP > 0
  PrintSocketInfo();
  printf(" Delete TCP socket");
  if(parentSocket != nullptr)
  { 
    printf(" Socket has now %d/%d child",parentSocket->childCnt-1,parentSocket->childMax);
  }
  printf("\n");

  #endif
}

void SocketTcp_c::SetState( State_et newState)
{
#if DEBUG_TCP > 0
PrintSocketInfo();
printf(" State change %s -> %s \n",stateStr[state],stateStr[newState]);
#endif
  state = newState;
}

SocketTcp_c* SocketTcp_c::GetChildSocket(uint32_t clientIp, uint16_t clientPort)
{
  SocketTcp_c* reqSocket  = nullptr;
  Socket_c* child = firstChildSocket;
  while(child != nullptr)
  {
    SocketTcp_c* childT = (SocketTcp_c*)child;
    if((childT->clientPort == clientPort) && (childT->clientIp == clientIp))
    {
      reqSocket = childT;
      break;
    }
    child = child->next;
  }
  return reqSocket;
 
}

SocketTcp_c* SocketTcp_c::GetTcpSocket(uint32_t clientIp,uint16_t clientPort, uint16_t port)
{
  SocketTcp_c* parentSocket = (SocketTcp_c*) GetSocket(port,IP_PROTOCOL_TCP);
  SocketTcp_c* socket = nullptr;
  if(parentSocket != nullptr)
  {
    if(parentSocket->reuseListenSocket == false)
    {
      socket = parentSocket->GetChildSocket(clientIp,clientPort);
    }
    else if((parentSocket->clientIp == clientIp) && (parentSocket->clientPort == clientPort))
    {
      socket = parentSocket;
    }
  }
  return socket;
}

void SocketTcp_c::HandlePacket(uint8_t* packet_p,uint16_t packetSize)
{
  Packet_st* packet = (Packet_st*) packet_p;

  uint32_t clientIp = ntohl(packet->ipHeader.srcIP);
  uint16_t clientPort = ntohs(packet->tcpHeader.srcPort);

  SocketTcp_c* handlerSocket = nullptr;
  if(reuseListenSocket == false)
  {
    if(state == LISTEN)
    {
      handlerSocket = GetChildSocket(clientIp,clientPort);
      
      if(handlerSocket != nullptr)
      {
        handlerSocket->HandleRoutedPacket(packet_p,packetSize);
      }
      else if((ntohs(packet->tcpHeader.offset_flags) & TCP_SYN_FLAG) && (childCnt < childMax))
      {
        SocketTcp_c* newSocket = new SocketTcp_c(port,this);
        newSocket->SetState(LISTEN);
        newSocket->reuseListenSocket = true;
        newSocket->HandleRoutedPacket(packet_p,packetSize);

        SendSocketEvent(SocketEvent_st::SOCKET_EVENT_NEWCLIENT,newSocket);
      }
    }

  }
  else
  {
    this->HandleRoutedPacket(packet_p,packetSize);
  }
  EthernetBuffers_c::DeleteBuffer(packet_p);


}

void SocketTcp_c::HandleRoutedPacket(uint8_t* packet_p,uint16_t packetSize)
{
  Packet_st* packet = (Packet_st*) packet_p;

  uint32_t clientIpTmp = ntohl(packet->ipHeader.srcIP);
  uint16_t clientPortTmp = ntohs(packet->tcpHeader.srcPort);

  uint16_t flags= ntohs(packet->tcpHeader.offset_flags);



  if(flags & TCP_RST_FLAG)
  {
    SetState(CLOSED);
    CloseEvents();
    return;
  }

  ResetKeepAliveCounter();

  switch(state)
  {
    case LISTEN:
    {
     if(flags & TCP_SYN_FLAG)
     {
       clientIp = clientIpTmp;
       clientPort = clientPortTmp;
       memcpy(clientMac,packet->macHeader.MAC_Src,6);

       uint32_t newRxSeq = ntohl(packet->tcpHeader.seqNo) ;
       uint32_t newTxSeq = (RngUnit_c::GetRandomVal())&0xFFFFFF;
 
       rxWindow.InitWindow(newRxSeq,rxBufferSize);
       txWindow.InitWindow(newTxSeq,TCP_TX_BUFFER_SIZE);
       
       SendSynAck();

       txWindow.UpdateWindowSize(ntohs(packet->tcpHeader.windowSize));

       uint32_t optVal = GetOption(packet,TCP_OPTION_WINSCALE);
       if(optVal != -1)
       { 
         txWindow.SetWindowScale(optVal);
       }

       SetState( SYN_RECEIVED);
     }



    }
    break;
    case SYN_RECEIVED:
    {
     if(flags & TCP_ACK_FLAG)
     { 
       SetState( ESTABLISHED);
       txWindow.UpdateWindowSize(ntohs(packet->tcpHeader.windowSize));
     }
    }
    break;
    case ESTABLISHED:
    {      
      uint16_t tcpOffset = ntohs(packet->tcpHeader.offset_flags);
      tcpOffset >>= 12;
      tcpOffset *= 4;
      uint16_t dataSize = ntohs(packet ->ipHeader.length) - tcpOffset;
      dataSize -= GetIpHeaderSize();

      bool lastPacket = false;
      if(flags & TCP_FIN_FLAG)
      {
        lastPacket = true;
        #if DEBUG_TCP >0
        printf(" receiver last packet\n");
        #endif
      }
      if((dataSize > 0) || (lastPacket))
      {
        
        uint8_t* dataStart = (packet->tcpPayload) + tcpOffset - sizeof(TcpHeader_st);

        uint32_t seqNo = ntohl(packet->tcpHeader.seqNo);

        ackToSent |= rxWindow.InsertData(dataStart,dataSize,seqNo,lastPacket); 
        #if DEBUG_TCP >1
        printf("TCP: Received %d bytes\n",dataSize);
        #endif

      }

      if((lastPacket) || (flags & TCP_PSH_FLAG) || (rxWindow.GetUsage() > 70 ))
      {
        SendSocketEvent(SocketEvent_st::SOCKET_EVENT_RX,this);        
      }
      txWindow.UpdateWindowSize(ntohs(packet->tcpHeader.windowSize));

      if(flags & TCP_ACK_FLAG)
      {
        uint32_t ackSeq = ntohl(packet->tcpHeader.ackNo);
        txWindow.AckData(ackSeq);
      }
      if(flags & TCP_FIN_FLAG)
      {
        if(txWindow.GetNoOfBytesWaiting() >0)
        {
          SetState( CLOSE_WAIT);
        }
        else
        {
          SetState( LAST_ACK);                    
          SendFinAck();
        }

      }
    }
    break;
    case LAST_ACK:
    {
      if(flags & TCP_ACK_FLAG)
      {
        uint32_t ackSeq = ntohl(packet->tcpHeader.ackNo);
        txWindow.AckData(ackSeq);
        SetState( CLOSED);
        CloseEvents();
      }
    }
    break;
    case FIN_WAIT_1:
    {
      if(flags & TCP_ACK_FLAG)
      {
        uint32_t ackSeq = ntohl(packet->tcpHeader.ackNo);
        txWindow.AckData(ackSeq);
        if(flags & TCP_FIN_FLAG)
        {
          SetState( CLOSED); //SetState(  TIME_WAIT);
          timeWait = 0;
          uint32_t seqNo = ntohl(packet->tcpHeader.seqNo);
          rxWindow.InsertData(nullptr,0,seqNo,true);
          SendAck(TCP_ACK_FLAG );
          CloseEvents();
        }
        else
        {
          SetState( FIN_WAIT_2);
        }
      }
    }
    break;
    case FIN_WAIT_2:
    {
      if(flags & TCP_ACK_FLAG)
      {
        uint32_t ackSeq = ntohl(packet->tcpHeader.ackNo);
        txWindow.AckData(ackSeq);
      }
      if(flags & TCP_FIN_FLAG)
      {
        SetState( CLOSED); //SetState( TIME_WAIT);
        timeWait = 0;
        uint32_t seqNo = ntohl(packet->tcpHeader.seqNo);
        rxWindow.InsertData(nullptr,0,seqNo,true);
        SendAck(TCP_ACK_FLAG );
        CloseEvents();
      }
    }
    break;
    default:
    break;
  }


}

void SocketTcp_c::FillTcpHeader(Packet_st* packet, uint16_t flags, uint32_t seqNo)
{
  uint16_t offset = 5;
  if(flags & TCP_SYN_FLAG)
  {
    offset += SYN_OPT_LENGTH/4;
  }
  offset <<= 12;

  if(flags &  TCP_ACK_FLAG)
  {
    rxWindow.AckData();
  }

  packet->tcpHeader.dstPort = htons(clientPort);
  packet->tcpHeader.srcPort = htons(port);
  packet->tcpHeader.checkSum = 0;
  packet->tcpHeader.offset_flags = htons(offset | flags);
  packet->tcpHeader.windowSize = htons(rxWindow.GetWindowSize());
  packet->tcpHeader.urgentPtr = 0;
  packet->tcpHeader.seqNo = htonl(seqNo);
  packet->tcpHeader.ackNo = htonl(rxWindow.GetLastAcked());
}



void SocketTcp_c::SendAck(uint16_t flags)
{

  //rxWindow.AckData();

  uint8_t* packet_p = new uint8_t[256];

  Packet_st* packet = (Packet_st*) packet_p;

  uint16_t size = GetTcpHeaderSize();

  FillTcpHeader(packet,flags,txWindow.GetLastInserted());

  if(flags & TCP_SYN_FLAG)
  {
    size += SYN_OPT_LENGTH;
    memcpy(&(packet->tcpPayload),synOptions,SYN_OPT_LENGTH);
    txWindow.StepSeq();
  }

  if(flags & TCP_FIN_FLAG)
  {
    txWindow.StepSeq();
  }

  FillMacHeader(packet,clientMac,ETH_TYPE_IPV4);
  FillIpHeader(packet,IP_PROTOCOL_TCP,clientIp,size);
  

  size += GetMacHeaderSize();
  size += GetIpHeaderSize();

  SendPacket(packet_p,size,nullptr,0);
  ackToSent = false;

  delete[] packet_p;  
}

void SocketTcp_c::FillDataHeaders(Packet_st* packet,uint16_t dataSize, uint32_t seqNo, uint16_t flags)
{
  uint16_t headersSize = GetTcpHeaderSize();

  if(flags & TCP_SYN_FLAG)
  {
    headersSize += SYN_OPT_LENGTH;
    memcpy(packet->tcpPayload,synOptions,SYN_OPT_LENGTH);
  }

  FillTcpHeader(packet,flags,seqNo);
  FillMacHeader(packet,clientMac,ETH_TYPE_IPV4);
  FillIpHeader(packet,IP_PROTOCOL_TCP,clientIp,dataSize + headersSize);
  headersSize += GetMacHeaderSize();
  headersSize += GetIpHeaderSize();
  packet->packetLength = headersSize;
}

void SocketTcp_c::SendSynAck(void)
{
  txWindow.InsertData(nullptr,0, TCP_SYN_FLAG,false);

  SendFromQueue();
}

void SocketTcp_c::SendFinAck(void)
{

  txWindow.InsertData(nullptr,0,  TCP_FIN_FLAG,false);

  SendFromQueue();
}

void SocketTcp_c::SendProbe(void)
{
  uint8_t dummyByte = 0x42;
  txWindow.InsertData(&dummyByte,1,  0,false);

  SendFromQueue();
}

void SocketTcp_c::ShutdownInternal(void)
{
  switch(state)
  {
    case ESTABLISHED:
    {
      SetState( FIN_WAIT_1);
      SendFinAck();
    }
    break;
    default:
    break;


  }
}

void SocketTcp_c::ResetConnection(void)
{
  SetState(CLOSED);

  txWindow.InsertData(nullptr,0, TCP_RST_FLAG,false);
  SendFromQueue();

  CloseEvents();
}

int SocketTcp_c::GetOption(Packet_st* packet, uint8_t optionKind)
{
  

  uint16_t tcpOffset = ntohs(packet->tcpHeader.offset_flags);
  tcpOffset >>= 12;
  tcpOffset *= 4;

  uint16_t noOfOptBytes = tcpOffset - sizeof(TcpHeader_st);

  uint8_t state_ = 0; /* 0=kind, 1= size, 2 = value */
  uint32_t optVal = 0;

  uint8_t kind;
  uint8_t length = 0;

  for(int i=0; i < noOfOptBytes; i++)
  {
    uint8_t byte = (packet->tcpPayload)[i];



    if(state_ == 0)
    {
      kind = byte;
      if(kind != TCP_OPTION_NOP)
      {
        state_ = 1;
      }
    }
    else if(state_ == 1)
    {
      length = byte;

      if(length > 2)
      {
        length -= 2;
        state_ = 2;
        optVal = 0;
      }
      else
      {
        state_ = 0;
      }
    }
    else  if(state_ ==2)
    {
      optVal <<=8;
      optVal |= byte;
      length--;
      if(length == 0)
      {
        if(kind == optionKind)
        {
          return optVal;
        }
        state_ = 0;
      }
    }
  }
  return -1;
}

void SocketTcp_c::SendFromQueue(void)
{
  if(txWindow.SendFromQueue(this))
  {
    ackToSent = false;
  }
}


void SocketTcp_c::LoopOverChildList( uint8_t oCode)
{
  if(reuseListenSocket == false)
  {
    Socket_c* child = firstChildSocket;
    Socket_c* nextChild;

    while(child !=nullptr)
    {
      nextChild = child->next;

      if(oCode == 0)
      {
        ((SocketTcp_c*)child)->Tick();
      }
      else if(oCode == 1)
      {
        ((SocketTcp_c*)child)->TxQueueTick();
      }
      else if(oCode == 2)
      {
        ((SocketTcp_c*)child)->ShutdownInternal();
      }
      child = nextChild;
    }
  }

  if(oCode == 0)
  {
    Tick();
  }
  else if(oCode == 1)
  {
    TxQueueTick();
  }
  else if(oCode == 2)
  {
    ShutdownInternal();
  }

}

void SocketTcp_c::TxQueueTick(void)
{

  txWindow.ScanQueue();
  if(txWindow.SendFromQueue(this))
  { 
      ackToSent = false;
  }

  if(state == ESTABLISHED)
  {
    if((ackToSent) || (rxWindow.AckData()))
    {
      SendAck(TCP_ACK_FLAG );
    }
  }

  if((state == CLOSE_WAIT) && (txWindow.GetNoOfBytesWaiting() == 0))
  {
    /* all data has been sent */
    SetState( LAST_ACK);
    //rxWindow.StepSeq();
    SendFinAck();
  }

}

void SocketTcp_c::Tick(void)
{
  switch(state)
  {
  
    case ESTABLISHED:
    {

      keepAliveCnt++;

      if(keepAliveCnt > TCP_ALIVE_PROBE_TIMEOUT)
      {

        if(keepAliveProbesCnt > TCP_ALIVE_PROBES)
        {
          ShutdownInternal();
        }
        else
        {
          keepAliveCnt -= TCP_ALIVE_PROBE_TIMEOUT/16;
          keepAliveProbesCnt++;
          SendProbe();
        }
      }

    }
    break;

    case SYN_SENT:
    case SYN_RECEIVED:
    case FIN_WAIT_1:
    case FIN_WAIT_2:
    case CLOSE_WAIT:
    case CLOSING:
    case LAST_ACK:
    {
      keepAliveCnt++;
      if(keepAliveCnt > 100)
      {
        ResetConnection();
      }



    }
    break;

    case TIME_WAIT:
    {
      if (timeWait >= 30)
      {
        SetState(  CLOSED);
        CloseEvents();
      }
      else
      {
        timeWait++;
      }
    }
    break;
    default:
    break;
  }

  if(state == CLOSED)
  {
    if(deleteWhenPossible == true)
    {
      delete this;
    }

    else if(task == 0)
    {
      /* weird error */
      delete this;
    }
  }

}

void SocketTcp_c::CloseEvents(void)
{
 SendSocketEvent(SocketEvent_st::SOCKET_EVENT_CLOSE,this);

 if(parentSocket != nullptr)
 {
   parentSocket->SendSocketEvent(SocketEvent_st::SOCKET_EVENT_DELCLIENT,this);
 }

}

void SocketTcp_c::PrintSocketInfo(void)
{
  #if DEBUG_TCP >1
  printf("TCP socket: %c port=%d client=%d.%d.%d.%d:%d :\n",
  parentSocket == nullptr ? 'P' : 'C',
  port,
  (clientIp>>24) & 0xFF,
  (clientIp>>16) & 0xFF,
  (clientIp>>8) & 0xFF,
  (clientIp>>0) & 0xFF,
  clientPort);
  #endif
}



void SocketTcp_c::Request(socketTcpRequestSig_c* recSig_p)
{
  switch(recSig_p->code)
  {
    case SOCKET_LISTEN:
    {
      SetState( LISTEN);
      childMax = recSig_p->clientMaxCnt;
      xTaskNotifyGive(recSig_p->task);

    }
    break;
    case SOCKET_ACCEPT:
    {
      acceptWaitingTask = recSig_p->task;
    }
    break;
    case SOCKET_SHUTDOWN:
    {
      ShutdownInternal();
      xTaskNotifyGive(recSig_p->task);
    }
    break;
    case SOCKET_CLOSE:
    {
      deleteWhenPossible = true;
      if(state == ESTABLISHED)
      {
        ShutdownInternal();
      }
      else if(state == CLOSED)
      {
        delete this;
      }
      xTaskNotifyGive(recSig_p->task); 
    }
    break;
    case SOCKET_SETTASK:
    {
      task = recSig_p->task;
      xTaskNotifyGive(recSig_p->task); 
    }
    break;
    case SOCKET_DISCONNECT_ALL_CHILD:
    {
      LoopOverChildList(2);
      xTaskNotifyGive(recSig_p->task);
    }
    break;
    case SOCKET_CLOSE_AFTER_SEND:
    {
      closeAfterSent = true;
      xTaskNotifyGive(recSig_p->task);
    }
    break;
    case SOCKET_REUSE_LISTEN:
    {
      reuseListenSocket = true;
      xTaskNotifyGive(recSig_p->task);
    }
    break;
    case SOCKET_GET_LOCAL_ADDR:
    {
      recSig_p->soccAdr->port = port;
      recSig_p->soccAdr->ip = IP_c::ipConfig_p->GetIp();
      xTaskNotifyGive(recSig_p->task);
    }
    break;
    case SOCKET_SET_RXBUFFER_SIZE:
    {
      rxBufferSize = recSig_p->bufferSize;
      xTaskNotifyGive(recSig_p->task);
    }
    break;

    default:
    break;


  }



}

void SocketTcp_c::HandleDataSend(socketSendReqSig_c* recSig_p)
{
  uint32_t bytesSent = 0;
  if((state == ESTABLISHED) || (state == CLOSE_WAIT))
  {
    uint32_t winSize =  txWindow.GetBufferCapacity();
    #if DEBUG_TCP >1
    printf("act winSize = %d, dataSize=%d\n",winSize,recSig_p->bufferSize);
    #endif
    if(winSize >= recSig_p->bufferSize)
    {
      uint16_t flag = 0;
      if(closeAfterSent)
      {
        flag = TCP_FIN_FLAG;
      }

      bytesSent = txWindow.InsertData(recSig_p->buffer_p,recSig_p->bufferSize,flag,true);

      if((closeAfterSent) && (bytesSent == recSig_p->bufferSize))
      {
        SetState( FIN_WAIT_1);
      }

      SendFromQueue();
    }
    else
    {
      #if DEBUG_TCP >1
      printf("socket send buffer to small (size=%d)\n",winSize);
      #endif
    }
  }
  else
  {
    #if DEBUG_TCP >1
    printf("invalid socket state\n");
    #endif
  }
  recSig_p->bytesSent = bytesSent;
  xTaskNotifyGive(recSig_p->task);
}

void SocketTcp_c::HandleDataReceive(socketReceiveReqSig_c* recSig_p)
{

  int result = rxWindow.ReadData(recSig_p->buffer_p,recSig_p->bufferSize);

  recSig_p->bytesReceived = result;
  xTaskNotifyGive(recSig_p->task);
}

void SocketTcp_c::ResetKeepAliveCounter(void)
{
  keepAliveCnt = 0;
  keepAliveProbesCnt = 0;
}

/*************************** USER FUNCTIONS *********************************************/

void  SocketTcp_c::Listen(uint8_t clientMax_) 
{ 
  socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_LISTEN;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->clientMaxCnt = clientMax_;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );

}

SocketTcp_c* SocketTcp_c::Accept(void)
{
  SocketTcp_c* newSocket;

  socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_ACCEPT;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->Send();

  xTaskNotifyWait(0,0xFFFFFFFF,(uint32_t*)&newSocket,portMAX_DELAY);  
  
  return newSocket;
}


int SocketTcp_c::Send(uint8_t* buffer,uint32_t dataSize,uint32_t delay)
{
  socketSendReqSig_c* sig_p = new socketSendReqSig_c;
  bool ok = false;

  while(1)
  {

    sig_p->task = xTaskGetCurrentTaskHandle();
    sig_p->socket = this;
    sig_p->buffer_p = buffer;
    sig_p->bufferSize = dataSize;
    sig_p->Send();

    ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );

    if(sig_p->bytesSent == dataSize)
    {
      ok = true;
      break;
    }
    else if(delay < 50)
    {
      break;
    }
    else
    {
      delay -= 50;
      vTaskDelay(50);
    }
  }

  if(ok == false)
  {
    /* socket has rejected handlig this packet so it is necessary to release them in this place */
    delete buffer;
    printf("release rejected TX packet \n");
  }

  uint32_t bytesSent = sig_p->bytesSent;
  delete sig_p;
  return bytesSent;


}



int SocketTcp_c::RecvCount(void)
{
  int result = rxWindow.GetNoOfBytesWaiting();
  return result;
}

int SocketTcp_c::Recv(uint8_t* buffer,uint32_t dataSize,uint32_t delay)
{
  int result;
  SocketEvent_st event;
  BaseType_t rec = pdFALSE;

  if(uxQueueMessagesWaiting(rxEventQueue) > 0)
  {
    rec = xQueueReceive(rxEventQueue,&event,0 );
  }

  result = rxWindow.ReadData(buffer,dataSize);

  if(result != 0)
  {
    return result;
  }
  else if((rec == pdTRUE ) &&(event.code == SocketEvent_st::SOCKET_EVENT_CLOSE))
  {
    return -1;
  }

  if(delay == 0)
  {
    return 0;
  }

  /* blocking on rxEvent */

  
  rec = xQueueReceive(rxEventQueue,&event,delay );
  if(rec == pdTRUE )
  {
    if(event.code == SocketEvent_st::SOCKET_EVENT_RX)
    {
      result = rxWindow.ReadData(buffer,dataSize);

      if(result != 0)
      {
        return result;
      }
    }
    else
    {
      return -1;
    }
  }

  return 0;
}

int SocketTcp_c::Receive(uint8_t* buffer,uint32_t dataSize,uint32_t delay)
{
  socketReceiveReqSig_c* sig_p = new socketReceiveReqSig_c;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->buffer_p = buffer;
  sig_p->bufferSize = dataSize;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );

  int bytesReceived = sig_p->bytesReceived;
  delete sig_p;

  return bytesReceived;
}

void SocketTcp_c::Shutdown(void)
{
  socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_SHUTDOWN;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );
}

void SocketTcp_c::SetTask(void)
{
  socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_SETTASK;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );
}

void SocketTcp_c::Close(void)
{
  socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_CLOSE;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );
}

void SocketTcp_c::DisconnectAllChild(void)
{
  socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_DISCONNECT_ALL_CHILD;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );
}

void SocketTcp_c::CloseAfterSend()
{
 socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_CLOSE_AFTER_SEND;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );
}

void SocketTcp_c::ReuseListenSocket(void)
{
 socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_REUSE_LISTEN;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );
}

void SocketTcp_c::GetLocalAddress(SocketAdress_st* socketAdress)
{
  socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_GET_LOCAL_ADDR;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->soccAdr = socketAdress;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );
}

void SocketTcp_c::SetRxBufferSize(uint32_t bufferSize)
{
 socketTcpRequestSig_c* sig_p = new socketTcpRequestSig_c;
  sig_p->code = SOCKET_SET_RXBUFFER_SIZE;
  sig_p->task = xTaskGetCurrentTaskHandle();
  sig_p->socket = this;
  sig_p->bufferSize = bufferSize;
  sig_p->Send();

  ulTaskNotifyTake(pdTRUE ,portMAX_DELAY );
}


/*******************COMMAND FUNCTIONS ************************/

const char* tcpSocketStateStr[] = {
    "INIT",
    "LISTEN",
    "SYN_SENT",
    "SYN_RECEIVED",
    "ESTABLISHED",
    "FIN_WAIT_1",
    "FIN_WAIT_2",
    "CLOSE_WAIT",
    "CLOSING",
    "LAST_ACK",
    "TIME_WAIT",
    "CLOSED"
    };

void SocketTcp_c::PrintInfo(char* buffer)
{
  if(parentSocket != nullptr)
  {
    sprintf(buffer,"     ");
  }
  else
  {
    if(reuseListenSocket)
    {  
      sprintf(buffer,"reused ");
    }
    else
    {
      sprintf(buffer,"child %d/%d ",childCnt,childMax);
    }
  }
  buffer += strlen(buffer);

  sprintf(buffer,"STATE=%s CLIENT=", tcpSocketStateStr[state]);

  IpConfig_c::PrintIp(buffer+strlen(buffer),clientIp,"");

  sprintf(buffer+strlen(buffer),"/%d",clientPort);

  if(task != NULL)
  {
    sprintf( buffer+strlen(buffer), " %s",pcTaskGetName(task));
  }
  else
  {
    sprintf( buffer+strlen(buffer), " no task");
  }


}
/*
void TcpTimerCallBack( TimerHandle_t xTimer )
{
  ethSocketTimeoutSig_c* sig_p = ( ethSocketTimeoutSig_c* ) pvTimerGetTimerID( xTimer );
  if(sig_p != NULL)
  {
    sig_p->Send();
  }
}





TCP_c::TCP_c(PACKET_INIT_MODE_et mode, uint8_t* data_p, uint16_t size) : IP_c(mode,data_p,size,sizeof(TcpHeader_st))
{
  tcpHeader_p = (TcpHeader_st*) ipPayload_p;
  if(mode == PACKET_INIT_ALLOCHEADER)
  {
    tcpPayload_p = data_p;
    buffer2_p = data_p;
    bufferSize2 = size;
    tcpPayloadSize = ipPayloadSize - sizeof(TcpHeader_st);
  }
  else if(mode == PACKET_INIT_NOALLOC)
  {
    uint16_t tcpHeaderSize = ntohs(tcpHeader_p->offset_flags);
    tcpHeaderSize >>= 12;
    tcpHeaderSize *=4;

    tcpPayloadSize = ipPayloadSize - tcpHeaderSize;
    tcpPayload_p = ipPayload_p + tcpHeaderSize;
  }
  else 
  {
    tcpPayload_p = ipPayload_p + GetTcpHeaderSize();
    tcpPayloadSize = ipPayloadSize - sizeof(TcpHeader_st);
  }


}



void SocketTcp_c::SendData(uint8_t* buf,uint16_t size)
{

}

int SocketTcp_c::FetchId(void)
{
  for(int i=0;i<MAX_NO_OF_CLIENTS;i++)
  {
    if(clientList[i] == NULL)
    {
      return i;
    }
  }
  return -1;
}

void SocketTcp_c::HandlePacket(uint8_t* packet_p,uint16_t packetSize)
{

  TCP_c* recPacket_p = new TCP_c(PACKET_INIT_NOALLOC,packet_p,packetSize);

  uint32_t clientIp = ntohl(recPacket_p->ipHeader_p->srcIP);
  uint16_t clientPort = ntohs(recPacket_p->tcpHeader_p->srcPort); 

  ClientTcp_c* client_p = GetClient(clientIp,clientPort);

  if( client_p == NULL) 
  {
    if(ntohs(recPacket_p->tcpHeader_p->offset_flags) | TCP_SYN_FLAG)
    {
 
      int id = FetchId();
      if(id >= 0)
      {
        xSemaphoreTake(xSemaphore,portMAX_DELAY );
        client_p =  new ClientTcp_c(this,clientIp,clientPort,recPacket_p->macHeader_p->MAC_Src,id);
        xSemaphoreGive(xSemaphore);
      }

    }
  }


  if( client_p != NULL) 
  {
    client_p->HandlePacket(recPacket_p);
  }

  


  delete recPacket_p;

}


int SocketTcp_c::ReadData(uint8_t clientId, uint8_t* buffer_p)
{
  ClientTcp_c* client_p = clientList[clientId];

  return client_p->DataRx(buffer_p);

}

int SocketTcp_c::RxDataLength(uint8_t clientId)
{
  ClientTcp_c* client_p = clientList[clientId];

  return client_p->RxDataLength();
}





ClientTcp_c* SocketTcp_c::GetClient(uint32_t ip,uint16_t port)
{
  for(int i=0;i<MAX_NO_OF_CLIENTS;i++)
  {
    ClientTcp_c* iter_p = clientList[i];
    if(iter_p != NULL)
    {
      if((iter_p->clientIp == ip) && (iter_p->clientPort == port))
      {
        return iter_p;
      }
    }
  }
  return NULL;
}

void SocketTcp_c::StartDataTx(ethSocketTxReqSig_c* recSig_p)
{
  ClientTcp_c* client_p = clientList[recSig_p->clientId];
  if(client_p != NULL)
  {
    client_p->StartDataTx(recSig_p);
  }
}

void SocketTcp_c::RxDataWaiting(uint8_t clientId)
{
  ClientTcp_c* client_p = clientList[clientId];
  if(client_p != NULL)
  { 
    printf("SOCKET_DATA_WAITING \n");

    ethSocketEventSig_c* sig_p = new ethSocketEventSig_c(procHandle);
    sig_p->client_p = client_p;
    sig_p->event = SOCKET_DATA_WAITING;
    sig_p->Send();
  }

 
}

void SocketTcp_c::Tick(void)
{
  for(int i=0;i<MAX_NO_OF_CLIENTS;i++)
  {
    if(clientList[i] != NULL)
    {
      clientList[i]->Tick();
    }
  }
}


ClientTcp_c::ClientTcp_c(SocketTcp_c* socket_p_,uint32_t ip_,uint16_t port_,uint8_t* mac_p,uint8_t id_) : clientIp(ip_), clientPort(port_), id(id_)
{
  
  state = LISTEN;
  socket_p = socket_p_;
  socket_p->clientCnt++;
  socket_p->clientList[id] = this;

  memcpy(clientMac,mac_p,6);

  packetList = NULL;
  transferTxOngoing = 0;

  txAck = TCP_TX_SEQSTART;
  txSeq = TCP_TX_SEQSTART;
  rxSeq = 0;
  rxAck = 0;
  txWindowSize = 0;
  rxWindowSize = TCP_WINDOW_SIZE;

  rxMsgBuffer = xStreamBufferCreate(TCP_WINDOW_SIZE,1);
  
  ackTimer = 0;

  timerSig = new ethSocketTimeoutSig_c;
  timerSig->socket_p = socket_p;
  timerSig->clientId = id;
  timer = xTimerCreate("",pdMS_TO_TICKS(TCP_ALIVE_TIMER),pdFALSE,(void*) timerSig,TcpTimerCallBack);
  xTimerStart(timer,1000);

  printf(" new TCP client\n");
  ethSocketEventSig_c* sig_p= new ethSocketEventSig_c(socket_p->procHandle);
  sig_p->client_p = this;
  sig_p->event = SOCKET_NEW_CLIENT;
  sig_p->Send();
}

ClientTcp_c::~ClientTcp_c(void)
{

  vStreamBufferDelete(rxMsgBuffer);

  PacketQueueItem_st* iter = packetList;
  PacketQueueItem_st* tmp;

  xTimerDelete(timer,1000);
  delete timerSig;

  while(iter != NULL)
  {
    tmp = iter;
    iter = iter->next;

    delete tmp->buffer;
    delete tmp;
  }
}




bool ClientTcp_c::HandleDataFromPacket(TCP_c* recPacket_p)
{
  bool sendAck = false;
  uint32_t packetSize = recPacket_p->tcpPayloadSize;
  uint32_t packetSeqNo = ntohl(recPacket_p->tcpHeader_p->seqNo);


  if(((packetSeqNo + packetSize) < (rxAck + rxWindowSize)) && (packetSeqNo >=rxAck))
  {
 

    if(rxAck == packetSeqNo)
    {
      // received first packet in window 
      rxAck += recPacket_p->tcpPayloadSize;
      sendAck = true;

      uint16_t sendSize = xStreamBufferSend(rxMsgBuffer,recPacket_p->tcpPayload_p,packetSize,1000);
      
      //scan queue
      xSemaphoreTake(socket_p->xSemaphore,portMAX_DELAY );

      PacketQueueItem_st* tmp;

      while(packetList != NULL)
      {
        if(packetList->seqNo == rxAck)
        { 
          tmp = packetList;
          packetList = packetList->next; 
               
          rxAck += tmp->bufferSize;  
          uint16_t sendSize = xStreamBufferSend(rxMsgBuffer,tmp->buffer,tmp->bufferSize,1000);
          delete[] tmp->buffer;
          delete tmp;
        }
        else
        {
          break;
        }
      }

      rxWindowSize = xStreamBufferSpacesAvailable(rxMsgBuffer);

      xSemaphoreGive(socket_p->xSemaphore);

      socket_p->RxDataWaiting(id);
    }
    else
    {
      uint8_t* buf = new uint8_t[packetSize];
      memcpy(buf,recPacket_p->tcpPayload_p,packetSize);

      PacketQueueItem_st* newItem = new PacketQueueItem_st;
      newItem->buffer = buf;
      newItem->bufferSize = packetSize;
      newItem->seqNo = packetSeqNo;
      newItem->next = NULL;
      xSemaphoreTake(socket_p->xSemaphore,portMAX_DELAY );
      InsertItemInList(newItem);
      xSemaphoreGive(socket_p->xSemaphore);
    }

  }
  else
  {
    printf("Packet outside window \n");
  }
  return sendAck;

}


void ClientTcp_c::HandlePacket(TCP_c* recPacket_p)
{
  if(ntohs(recPacket_p->tcpHeader_p->offset_flags) & TCP_RST_FLAG)
  {
    Destroy();

    return;
  }

  if(transferTxOngoing == false)
  {
    // reset alive timer 
    xTimerReset(timer,pdMS_TO_TICKS(1000));
  }

  txWindowSize = ntohs(recPacket_p->tcpHeader_p->windowSize);

  if(transferTxOngoing)
  {
    if(ntohs(recPacket_p->tcpHeader_p->offset_flags) & TCP_ACK_FLAG)
    {   
      uint32_t newTxAck = ntohl(recPacket_p->tcpHeader_p->ackNo);

      if(newTxAck > txAck)
      {
        txAck = newTxAck;
        RunDataTx();
      }
    }
  }


  switch(state)
  {
    case LISTEN:
    {
      if(ntohs(recPacket_p->tcpHeader_p->offset_flags) & TCP_SYN_FLAG)
      {
        txSeq = TCP_TX_SEQSTART;
        //rxAck = ntohl(recPacket_p->tcpHeader_p->ackNo);
        rxSeq = ntohl(recPacket_p->tcpHeader_p->seqNo);  
        rxAck = rxSeq;      
        rxAck++;
        SendAck(TCP_SYN_FLAG | TCP_ACK_FLAG);
        txSeq++;
        state = SYN_REC;
      }
      break;
    }
    case SYN_REC:
    {
      if(ntohs(recPacket_p->tcpHeader_p->offset_flags) & TCP_ACK_FLAG)
      {
        //uint32_t seq = ntohl(recPacket_p->tcpHeader_p->seqNo);
        uint32_t ack = ntohl(recPacket_p->tcpHeader_p->ackNo);
        if(ack == txSeq)
        {
          state = ESTABLISHED;
        }

        txAck = ack;
      }
      break;
    }
    case ESTABLISHED:
    {
      //printf("ESTABLISHED, size=%d\n",recPacket_p->tcpPayloadSize);

      bool sendAck = false;

      if(recPacket_p->tcpPayloadSize > 0)
      {
        // handle RX data 
        sendAck = HandleDataFromPacket(recPacket_p);
        ackTimer = 0;
      }

      if( transferTxOngoing )  // data to send waiting 
      {
        if(ntohs(recPacket_p->tcpHeader_p->offset_flags) & TCP_FIN_FLAG)
        {
          rxAck++;
          SendAck(TCP_ACK_FLAG);
          state = CLOSE_WAIT;
        }
      }
      else
      {
        if(ntohs(recPacket_p->tcpHeader_p->offset_flags) &  TCP_FIN_FLAG)
        {
          rxAck++;
          SendAck(TCP_ACK_FLAG | TCP_FIN_FLAG);
          state = LAST_ACK;
        }
        else if(sendAck)
        {
          SendAck(TCP_ACK_FLAG);
        } 
      }

      break;
    }
    case FIN_WAIT1:
    {
      //printf("FIN_WAIT1, size=%d\n",recPacket_p->tcpPayloadSize);

      bool sendAck = false;

      if(recPacket_p->tcpPayloadSize > 0)
      {
        // handle RX data 
        sendAck = HandleDataFromPacket(recPacket_p);
      }

      if(ntohs(recPacket_p->tcpHeader_p->offset_flags) &  TCP_ACK_FLAG)
      {
        if(ntohs(recPacket_p->tcpHeader_p->offset_flags) &  TCP_FIN_FLAG)
        {
          rxAck++;
          SendAck(TCP_ACK_FLAG);
          state = LAST_ACK;
          sendAck = false;
          Destroy();
        }
        else
        {
          state = FIN_WAIT2;
        }
      }

      if(sendAck)
      {
        SendAck(TCP_ACK_FLAG);
      } 
    }
    break;
    case FIN_WAIT2:
    {
      //printf("FIN_WAIT2, size=%d\n",recPacket_p->tcpPayloadSize);

      bool sendAck = false;

      if(recPacket_p->tcpPayloadSize > 0)
      {
        // handle RX data 
        sendAck = HandleDataFromPacket(recPacket_p);
      }

      if(ntohs(recPacket_p->tcpHeader_p->offset_flags) &  TCP_FIN_FLAG)
      {
        rxAck++;
        SendAck(TCP_ACK_FLAG);
        state = LAST_ACK;
        sendAck = false;
        Destroy();
      }

      if(sendAck)
      {
        SendAck(TCP_ACK_FLAG);
      } 
    }
    break;
    case LAST_ACK:
    if(ntohs(recPacket_p->tcpHeader_p->offset_flags) &  TCP_ACK_FLAG)
    {
      Destroy();
    }

    break;
    default:
    break;

  }

}
int ClientTcp_c::RxDataLength(void)
{
  uint16_t size = xStreamBufferBytesAvailable(rxMsgBuffer);
  return size;
}

int ClientTcp_c::DataRx(uint8_t* buf_p)
{

  uint16_t size = xStreamBufferReceive(rxMsgBuffer,buf_p,ETH_MAX_PACKET_SIZE,0);
  return size;



}


int ClientTcp_c::Send(uint8_t* buffer_p, uint16_t bufferSize)
{
  int resp = 0;
  ethSocketTxReqSig_c* sig_p = new ethSocketTxReqSig_c;
  sig_p->data_p = buffer_p;
  sig_p->dataSize = bufferSize;
  sig_p->socket_p = socket_p;
  sig_p->clientId = id;
  SemaphoreHandle_t sem = xSemaphoreCreateBinary();
  sig_p->ackSeaphore = sem;
  sig_p->Send();

  if(xSemaphoreTake (sem,10000) == pdTRUE )
  {

    resp = 0;
  }
  else
  {
    printf("ETH_TX timeout\n");
    resp = -1;
  }
  vSemaphoreDelete(sem);
  return resp;


}

void ClientTcp_c::StartDataTx(ethSocketTxReqSig_c* recSig_p)
{
  if(transferTxOngoing)
  {
    // is should never happen 
    return;
  }

  // store TX data 

  txData_p = recSig_p->data_p;
  txDataSize = recSig_p->dataSize;
  txDataOffset = txSeq;
  txSemaphore = recSig_p->ackSeaphore;

  transferTxOngoing = true;
  xTimerStop(timer,1000);
  xTimerChangePeriod(timer,pdMS_TO_TICKS(TCP_TXACK_TIMER),1000);
  xTimerStart(timer,1000);

  RunDataTx();

}

void ClientTcp_c::RunDataTx(void)
{
  // send as many data as possible 

  bool cont = true;

  while(cont)
  {
    uint16_t dataSizeLimit = ETH_MAX_PACKET_SIZE - 54;
    uint16_t windowSizeLimit = 0;
    if((txAck + txWindowSize) > txSeq) { windowSizeLimit = (txAck + txWindowSize) - txSeq;  }
    uint16_t currentPacketDataSize = txDataSize - (txSeq - txDataOffset);

    if(currentPacketDataSize > dataSizeLimit) { currentPacketDataSize = dataSizeLimit;  }
    if(currentPacketDataSize > windowSizeLimit) { currentPacketDataSize = windowSizeLimit;  }

    if(currentPacketDataSize > 0)
    {
      TCP_c* packet_p = new TCP_c(PACKET_INIT_ALLOCHEADER,&txData_p[txSeq - txDataOffset],currentPacketDataSize);

      packet_p->FillTcpHeader(this,TCP_ACK_FLAG);
      packet_p->FillIpHeader(IPPROTO_TCP,clientIp);
      packet_p->FillMacHeader(clientMac);     

      ethTxPacketSig_c* sig_p = new ethTxPacketSig_c;

      sig_p->buffer = packet_p->buffer_p;
      sig_p->dataLen = packet_p->bufferSize ;
      sig_p->buffer2 = packet_p->buffer2_p;
      sig_p->dataLen2 = packet_p->bufferSize2 ;
      sig_p->releaseBuffer2 = false;

      sig_p->Send();

      delete packet_p; 


      txSeq += currentPacketDataSize;

      if((txSeq+1) >= (txDataSize + txDataOffset))
      {
        cont = false;
      }

    }
    else
    {
      if((txAck == txSeq) && ((txDataSize + txDataOffset) == txSeq))
      {
        printf("all TX acked\n");
        TerminateTx();
      }

      cont = false;
    }

  }



}

void ClientTcp_c::TerminateTx(void)
{
  transferTxOngoing = false;

  xTimerStop(timer,1000);
  xTimerChangePeriod(timer,pdMS_TO_TICKS(TCP_ALIVE_TIMER),1000);
  xTimerStart(timer,1000);

  xSemaphoreGive(txSemaphore);

}

void ClientTcp_c::InsertItemInList(PacketQueueItem_st* newItem)
{
  if(packetList == NULL)
  {
    packetList = newItem;
  }
  else if(packetList->seqNo > newItem->seqNo)
  {
    newItem->next = packetList;
    packetList = newItem;
  }
  else
  {
    PacketQueueItem_st* iter = packetList;

    while(iter != NULL)
    {

      if(iter->next == NULL)
      {
        iter->next = newItem;
        break;
      }
      else if(iter->next->seqNo > newItem->seqNo)
      {
        newItem->next = iter->next;
        iter->next = newItem;
        break;
      }
      else
      {
        iter = iter->next;
      }
    }
  }
  printf("packet queued\n");
}

void SocketTcp_c::Action(uint8_t clientId, SOCKET_ACTION_et action) 
{
  ClientTcp_c* client_p = clientList[clientId];
  if(client_p != NULL)
  {
    client_p->Action(action);;
  }

}

void SocketTcp_c::Timeout(uint8_t clientId)
{
  ClientTcp_c* client_p = clientList[clientId];
  if(client_p != NULL)
  {
    client_p->Timeout();
  }
}


void ClientTcp_c::Action(SOCKET_ACTION_et action) 
{


  switch(action)
  {

    case SOCKET_CLOSE:
    switch(state)
    {
      case ESTABLISHED:
      {
        // send FIN packet 
        SendAck(TCP_FIN_FLAG);
        txSeq++;

        state = FIN_WAIT1;
      }
      break;
      case CLOSE_WAIT:
      {
        // send FIN packet 
        SendAck(TCP_FIN_FLAG);
        txSeq++;

        state = LAST_ACK;
      }
      break;
      default:
      break;


    }
    break;
    default:
    break;


  }


}

void ClientTcp_c::Timeout(void)
{
  printf("client timeout \n");
  if( transferTxOngoing)
  {


  }
  else
  {
    switch(state)
    {
      case FIN_WAIT1:
      case FIN_WAIT2:
      case TIME_WAIT:
      {
        SendAck(TCP_RST_FLAG);
        delete this;
      }
      break;

      case CLOSE_WAIT:
      {
        // send FIN packet 
        SendAck(TCP_FIN_FLAG);
        txSeq++;
        state = LAST_ACK;
        xTimerStart(timer,1000);
      }
      
      case ESTABLISHED:
      default:
      {
        //send FIN packet 
        SendAck(TCP_FIN_FLAG);
        txSeq++;
        state = FIN_WAIT1;
        xTimerStart(timer,1000);
      }
      break;
    }
  }
}

bool ClientTcp_c::UpdateRxWindow(void)
{
  bool updated = false;

  if(rxWindowSize < TCP_WINDOW_SIZE)
  {
    uint16_t actWindow = xStreamBufferSpacesAvailable(rxMsgBuffer);
    if(rxWindowSize != actWindow)
    {
      updated = true;
      rxWindowSize = actWindow;
    }
  }
  return updated;
}

void ClientTcp_c::Tick(void)
{
  if(state == ESTABLISHED)
  {
    if(ackTimer > ACK_TIMER_TOP)
    {
      if(UpdateRxWindow())
      {
        SendAck(TCP_ACK_FLAG);
        ackTimer = 0;
      }
    }
    else
    {
      ackTimer++;
    }
  }
}

void ClientTcp_c::Destroy(void)
{
  // unregister 

  socket_p->clientCnt--;
  socket_p->clientList[id] = NULL;

  ethSocketEventSig_c* sig_p= new ethSocketEventSig_c(socket_p->procHandle);
  sig_p->client_p = this;
  sig_p->event = SOCKET_END_CLIENT;
  sig_p->Send();

}
*/