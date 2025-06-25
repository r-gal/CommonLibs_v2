#ifndef ETHERNETSOCKET_H
#define ETHERNETSOCKET_H

//#include "common.hpp"
#include "EthernetCommon.hpp"
#include "EthernetIp.hpp"

#include "commandHandler.hpp"

class Socket_c;
struct SocketEvent_st
{
  enum SocketEvent_et
  {
    SOCKET_EVENT_NOEVENT,
    SOCKET_EVENT_RX,
    SOCKET_EVENT_CLOSE,
    SOCKET_EVENT_NEWCLIENT,
    SOCKET_EVENT_DELCLIENT,
  } code;
  Socket_c* socket;
};

class Socket_c : public IP_c
{


  StaticQueue_t rxStaticQueue;
  uint8_t rxEventQueueStorageArea[sizeof(SocketEvent_st) * SOCKET_QUEUE_LEN];
  

  public:
  QueueHandle_t rxEventQueue;

  Socket_c* next;
  static Socket_c* first;
  static Socket_c* GetSocket(uint16_t port, uint8_t protocol);

  void SendSocketEvent(SocketEvent_st::SocketEvent_et event, Socket_c* _socket);
  void WaitForEvent(SocketEvent_st* event,TickType_t const ticksToWait);

  void RedirectEvent(Socket_c* dstSocket);
  

  Socket_c* firstChildSocket;
  Socket_c* parentSocket;
  uint8_t childCnt;
  uint8_t childMax;
  
  const uint8_t protocol;
  const uint16_t port;

  Socket_c(uint16_t port,uint8_t protocol_,Socket_c* parentSocket);
  ~Socket_c(void);

  void AddNewSocketToList(void);


  virtual void HandlePacket(uint8_t* packet_p,uint16_t packetSize) = 0;
  virtual void Tick(void) = 0;

  virtual void PrintInfo(char* buffer) = 0;


};

class SocketSet_c
{
  QueueSetHandle_t queueSet;
  public:

  SocketSet_c(uint8_t maxSockets);
  ~SocketSet_c(void);

  void AddSocket(Socket_c* newSocket);

  Socket_c* Select(void);

  void RemoveSocket(Socket_c* socket_);


};

/*****************command section **************************/

#if USE_COMMANDS == 1
class Com_socketlist : public Command_c
{
  public:
  char* GetComString(void) { return (char*)"socketlist"; }
  void PrintHelp(CommandHandler_c* commandHandler ){}
  comResp_et Handle(CommandData_st* comData_);
};


class CommandSocket_c :public CommandGroup_c
{

  Com_socketlist socketlist;

  public:

};


#endif


#endif