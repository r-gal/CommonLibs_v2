#ifndef HTTP_PROCESS_H
#define HTTP_PROCESS_H

#include "SignalList.hpp"
#include "GeneralConfig.h"

#include "EthernetTcp.hpp"

#define HTTP_BUFFER_SIZE 512
#define HTTP_SOCKET_BUFFER_SIZE 1024

class HttpProcess_c : public process_c
{
  SocketTcp_c* serverSocket;
  SocketTcp_c* clientSockets[MAX_NO_OF_HTTP_CLIENTS];
 
  public :

  HttpProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId);

  void main(void);

  void DeleteClient(uint8_t idx);

};

  enum client_http_answer_et
  {
  	WEB_REPLY_OK = 200,
  	WEB_NO_CONTENT = 204,
  	WEB_BAD_REQUEST = 400,
  	WEB_UNAUTHORIZED = 401,
  	WEB_NOT_FOUND = 404,
  	WEB_GONE = 410,
  	WEB_PRECONDITION_FAILED = 412,
  	WEB_INTERNAL_SERVER_ERROR = 500,
  };

class HttpInterface_c
{
  public:

  

  
  static void SendAnswer(SocketTcp_c* socket,client_http_answer_et answer,const char* extraLine,const char* data = NULL);
  static const char *webCodename (client_http_answer_et aCode);


  virtual void HandleCommand(char* line,char* extraData,SocketTcp_c*) = 0; 

};

class HttpClientProcess_c
{
  SocketTcp_c* socket;
  uint8_t procIdx;
  HttpProcess_c* serverProcess;

  static HttpInterface_c* httpPostInterface;


  

  char cRxedData[ HTTP_BUFFER_SIZE ];

  int lBytesReceived;
  int idx;
  bool keepAlive;
  char* FetchNextLine(void);
  bool AnalyseLine(char* line, char* extraData);
  void HandleGET(char* line);
  




  public:

  static void HttpClientTaskWrapper( void * pvParameters )
  {
    ((HttpClientProcess_c*)pvParameters)->MainLoop();
  }

  HttpClientProcess_c(SocketTcp_c* socket_,uint8_t idx_, HttpProcess_c* serverProc_);
  ~HttpClientProcess_c(void);

  void MainLoop(void);

  void Close(void);

  static void AssignPostHandler(HttpInterface_c* handler) {httpPostInterface = handler;}

};

#endif