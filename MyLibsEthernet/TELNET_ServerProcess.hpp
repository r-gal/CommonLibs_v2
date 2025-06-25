#ifndef TELNET_PROCESS_H
#define TELNET_PROCESS_H

#include "SignalList.hpp"
#include "GeneralConfig.h"

#include "semphr.h"

//#include "FreeRTOS_Sockets.h"
#include "EthernetTcp.hpp"

#define TELNET_BUFFER_SIZE 512
#define TELNET_SOCKET_BUFFER_SIZE 1024

class TelnetProcess_c : public process_c
{

  SocketTcp_c* serverSocket;
  SocketTcp_c* clientSockets[MAX_NO_OF_TELNET_CLIENTS];
  
  static TelnetProcess_c* onwRef;

  SemaphoreHandle_t clientListSemaphore;
  public :

  TelnetProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId);

  void main(void);

  void DeleteClient(uint8_t idx);
  static void SendToAllClients(uint8_t* data, uint16_t size);
  

};

class TelnetClientProcess_c
{
  SocketTcp_c* socket;
  uint8_t idx;
  TelnetProcess_c* serverProcess;
  bool optionsReceived;

  char cRxedData[ TELNET_BUFFER_SIZE ];

  public:

  static void TelnetClientTaskWrapper( void * pvParameters )
  {
    ((TelnetClientProcess_c*)pvParameters)->MainLoop();
  }
  void Send(uint8_t* data, uint16_t size);

  TelnetClientProcess_c(SocketTcp_c* socket_, uint8_t idx_, TelnetProcess_c* serverProc_);
  ~TelnetClientProcess_c(void);

  void MainLoop(void);

  void Close(void);



};

#endif