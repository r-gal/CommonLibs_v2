#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"

#include "EthernetSocket.hpp"

Socket_c* Socket_c::first = nullptr;

Socket_c::Socket_c(uint16_t port,uint8_t protocol_,Socket_c* parentSocket) : port(port), protocol(protocol_), parentSocket(parentSocket)
{
  childCnt = 0;

  rxEventQueue = xQueueCreateStatic( SOCKET_QUEUE_LEN,
                               sizeof(SocketEvent_st),
                               rxEventQueueStorageArea,
                               &rxStaticQueue );

  firstChildSocket = nullptr;
  this->next = nullptr;

  socketAddSig_c* sig_p = new socketAddSig_c;
  sig_p->socket = this;
  sig_p->Send();
  
}

void Socket_c::AddNewSocketToList(void)
{
  if(parentSocket == nullptr)
  {
    this->next = first;
    first = this;
  }
  else
  {
    this->next = parentSocket->firstChildSocket;
    parentSocket->firstChildSocket = this;
    parentSocket->childCnt++;
  }
}

void Socket_c::RedirectEvent(Socket_c* dstSocket)
{
  rxEventQueue = dstSocket->rxEventQueue;

}

Socket_c::~Socket_c(void)
{
  Socket_c** first_pp;

  if(firstChildSocket != nullptr)
  {
    /* ERROR */
  }

  if(parentSocket == nullptr)
  {
    first_pp = &first;
  }
  else
  {
    first_pp = &(parentSocket->firstChildSocket);
    parentSocket->childCnt--;
  }

  Socket_c* socket_p = *first_pp;

  if(socket_p == this)
  {
    *first_pp = this->next;
  }
  else
  {

    while(socket_p != NULL)
    {
      if(socket_p->next == this)
      {
        socket_p->next = this->next;
      }
      else
      {
        socket_p = socket_p->next;
      }
    }
  }
}

Socket_c* Socket_c::GetSocket(uint16_t port, uint8_t protocol)
{
 Socket_c* socket_p = first;

 while(socket_p != NULL)
 {
   if((socket_p->port == port) && (socket_p->protocol == protocol))
   {
     return socket_p;
   }
   else
   {
     socket_p = socket_p->next;
   }
 }
 return NULL;
}

void Socket_c::SendSocketEvent(SocketEvent_st::SocketEvent_et code, Socket_c* _socket)
{
  SocketEvent_st event;
  event.code = code;
  event.socket = _socket;
  #if DEBUG_SOCKET > 0
  printf("SendSocketEvent, code=%d\n",code);
  #endif
  // xQueueOverwrite(rxEventQueue,&event);

  if((code != SocketEvent_st::SOCKET_EVENT_RX) || (uxQueueSpacesAvailable(rxEventQueue) > 2))
  {
    if(xQueueSend(rxEventQueue,&event,1000) == pdFALSE)
    {
       #if DEBUG_SOCKET > 0
       printf("SendSocketEvent queue full\n");
       #endif
    }
  }
}

void Socket_c::WaitForEvent(SocketEvent_st* event, TickType_t const ticksToWait)
{
  BaseType_t rec = pdFALSE;

  rec = xQueueReceive(rxEventQueue,event,ticksToWait );
  if(rec == pdTRUE )
  {

  }
  else
  {
    event->code = SocketEvent_st::SOCKET_EVENT_NOEVENT;
  }     

}


SocketSet_c::SocketSet_c(uint8_t maxSockets)
{
  queueSet = xQueueCreateSet( maxSockets * SOCKET_QUEUE_LEN );
}

SocketSet_c::~SocketSet_c(void)
{
  vQueueDelete(queueSet);
}


void SocketSet_c::AddSocket(Socket_c* newSocket)
{
  xQueueAddToSet(newSocket->rxEventQueue,queueSet);
}

Socket_c* SocketSet_c::Select(void)
{
   Socket_c* socket = nullptr;
   #if DEBUG_SOCKET > 0
   printf("SocketSet waiting\n");
   #endif
   QueueHandle_t actQueue = xQueueSelectFromSet(queueSet,portMAX_DELAY);

   if(actQueue != 0)
   {
 
     SocketEvent_st event ;
     event.socket = nullptr;
     xQueuePeek( actQueue, &event, 0 ); 
     socket = event.socket;
     #if DEBUG_SOCKET > 0
     printf("SocketSet select, event=%d, socket=%X\n",event.code,event.socket);
     #endif
   }
   return socket;
}

void SocketSet_c::RemoveSocket(Socket_c* socket_)
{
  xQueueRemoveFromSet(socket_->rxEventQueue,queueSet);
}

/*****************command section **************************/

#if USE_COMMANDS == 1

CommandSocket_c commandSocket;

const char* socketProto[] = {"UDP","TCP"};


comResp_et Com_socketlist::Handle(CommandData_st* comData)
{
  Socket_c* pSocket = Socket_c::first;

  char* strBuf = new char[128];

  Print(comData->commandHandler,"SOCKET LIST:\n");

  while(pSocket != nullptr)
  {
    bool cont = true;
    if(pSocket->protocol == IP_PROTOCOL_UDP)
    {
      sprintf(strBuf, "UDP, PORT=%d ",pSocket->port);
    }
    else if(pSocket->protocol == IP_PROTOCOL_TCP)
    {
      sprintf(strBuf, "TCP, PORT=%d ",pSocket->port);
      pSocket->PrintInfo(strBuf+strlen(strBuf));

    }
    else if(pSocket->protocol == IP_PROTOCOL_ICMP)
    {
      sprintf(strBuf, "ICMP");
      cont = false;
    }
    else
    {
      sprintf(strBuf, "UNN");
      cont = false;
    }
    strcat(strBuf,"\n");
    Print(comData->commandHandler,strBuf);

    Socket_c* cSocket;

    cSocket = pSocket->firstChildSocket;

    while(cSocket != nullptr)
    {
      sprintf(strBuf, "     PORT=%d ",pSocket->port);
      cSocket->PrintInfo(strBuf+strlen(strBuf));
      strcat(strBuf,"\n");
      Print(comData->commandHandler,strBuf);
      cSocket = cSocket->next;
    }


    pSocket = pSocket->next;
  }

  delete[] strBuf;
  return COMRESP_OK;
}

#endif