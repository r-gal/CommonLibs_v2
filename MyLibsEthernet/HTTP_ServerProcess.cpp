 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "HTTP_ServerProcess.hpp"
#include "Ethernet.hpp"
#include "FileSystem.hpp"

/*
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rtc.h"
#include "stm32f4xx_dma.h"
#include "misc.h"

*/

/*
void vFunction100msTimerCallback( TimerHandle_t xTimer )
{
  //dispActTimSig.Send();

}*/

HttpInterface_c* HttpClientProcess_c::httpPostInterface = nullptr;

HttpProcess_c::HttpProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId) : process_c(stackSize,priority,queueSize,procId,"HTTP_Serv")
{

 // TimerHandle_t timer = xTimerCreate("",pdMS_TO_TICKS(500),pdTRUE,( void * ) 0,vFunction100msTimerCallback);
 // xTimerStart(timer,0);

  for(int i = 0;i< MAX_NO_OF_HTTP_CLIENTS ;i++)
  {
    clientSockets[i] = nullptr;
  }

}

void HttpProcess_c::main(void)
{
   #if DEBUG_PROCESS > 0
   printf("Http proc started \n");
   #endif

  serverSocket = new SocketTcp_c(80,nullptr);

  serverSocket->Listen(MAX_NO_OF_HTTP_CLIENTS);

  while(1)
  {
    /* Accept a connection on a TCP socket. */

    //SocketTcp_c* clientSocket = serverSocket->Accept();

    SocketEvent_st event;
    serverSocket->WaitForEvent(&event,portMAX_DELAY);

    if(event.code == SocketEvent_st::SOCKET_EVENT_NEWCLIENT)
    {
      SocketTcp_c* clientSocket = (SocketTcp_c*)event.socket;

      int idx = 0;

      for(idx = 0;idx< MAX_NO_OF_HTTP_CLIENTS ;idx++)
      {
        if(clientSockets[idx] == nullptr)
        {
          clientSockets[idx] = clientSocket;
          break;
        }
      }
      HttpClientProcess_c* newClientTask = new HttpClientProcess_c(clientSocket,idx,this);
      
    }
    else if(event.code == SocketEvent_st::SOCKET_EVENT_DELCLIENT)
    {
      SocketTcp_c* clientSocket = (SocketTcp_c*)event.socket;

      int idx = 0;

      for(idx = 0;idx< MAX_NO_OF_HTTP_CLIENTS ;idx++)
      {
        if(clientSockets[idx] == clientSocket)
        {
          clientSockets[idx] = nullptr;
          break;
        }
      }
    }
    else
    {
      #if DEBUG_HTTP > 0
      printf("HTTP: FreeRTOS_accept error \n");
      #endif
    }
 
  }


}

void HttpProcess_c::DeleteClient(uint8_t idx)
{
  clientSockets[idx] = nullptr;
}

HttpClientProcess_c::HttpClientProcess_c(SocketTcp_c* socket_,uint8_t idx_, HttpProcess_c* serverProc_)
{
  socket = socket_;
  procIdx  = idx_;
  serverProcess = serverProc_;
  keepAlive = true;

  char taskName[12];
  sprintf(taskName,"HTTP_con_%d",procIdx);

  xTaskCreate( HttpClientTaskWrapper,
               taskName,
               512,
               ( void * ) this,
               tskIDLE_PRIORITY+2,
               NULL );

}

HttpClientProcess_c::~HttpClientProcess_c(void)
{

}

void HttpClientProcess_c::MainLoop(void)
{
  
  socket->SetTask();
  //Ethernet_c::RegisterToNetworkDownEvent(socket);
  while(1)
  {
    /* Receive another block of data into the cRxedData buffer. */
    //lBytesReceived = FreeRTOS_recv( socket, &cRxedData, BUFFER_SIZE, 0 );
    SocketEvent_st event;
    socket->WaitForEvent(&event,portMAX_DELAY);

    if(event.code == SocketEvent_st::SOCKET_EVENT_RX)
    {

      lBytesReceived = socket->Receive((uint8_t*)&cRxedData, HTTP_BUFFER_SIZE-1,portMAX_DELAY);

      cRxedData[lBytesReceived] = 0;

      //lBytesReceived = socket->Recv((uint8_t*)&cRxedData, HTTP_BUFFER_SIZE,portMAX_DELAY);

      if( lBytesReceived > 0 )
      {
        //printf("HttpClientProcess_c received %d bytes: \n %s \n",lBytesReceived,cRxedData);

        char* extraData = strstr(cRxedData,"\r\n\r\n");

        if(extraData != nullptr)
        {
          extraData +=4;
        }

        idx = 0;

        char* line = FetchNextLine();

        bool requestHandled = false;
        while(line != nullptr)
        {
          requestHandled |= AnalyseLine(line,extraData);



          line = FetchNextLine();
        } 

        if(requestHandled != true)
        {
          HttpInterface_c::SendAnswer(socket,WEB_BAD_REQUEST,"Content-Length: 0\r\n");
        }

        /* Data was received, process it here. */
        //prvProcessData( cRxedData, lBytesReceived );
      
      }

    }
    else if(event.code == SocketEvent_st::SOCKET_EVENT_CLOSE)
    {
      Close();
      break;
    }
  }
  
}

void HttpClientProcess_c::Close(void)
{

  socket->Shutdown();
  socket->Close();
  //serverProcess->DeleteClient(idx);
  delete this;
  FileSystem_c::CleanTask();
  vTaskDelete(NULL);
}

char* HttpClientProcess_c::FetchNextLine(void)
{
  /* return pointer to next line. null at end of line will be inserted */
  int startIdx = idx;

  bool phase = false;
  while(idx < lBytesReceived)
  {
    if(cRxedData[idx] == 0x0D) 
    {
      cRxedData[idx] = 0; /* insert end of line */
      idx++;
      phase =true;
    }
    else if(cRxedData[idx] == 0x0A) 
    {
      idx++;
      if(phase && (idx - startIdx > 2))
      {
        return & (cRxedData[startIdx]);
      }
      else
      {
        return nullptr; /* format error */
      }
    }
    else
    {
      idx++;
    }
  }

  return nullptr;
}


bool HttpClientProcess_c::AnalyseLine(char* line,char* extraData)
{
  //printf("HTTP line: %s\n",line);

  if(strncmp(line,"GET",3) == 0)
  {
    HandleGET(line+4);
    return true;
  }
  else if(strncmp(line,"POST",4) == 0)
  {
    if(httpPostInterface != nullptr)
    {
      httpPostInterface->HandleCommand(line+5,extraData,socket);
      return true;
    }
    return false;
  }
  else if(strncmp(line,"Connection: ",12) == 0)
  {
    if(strncmp(line+12,"keep-alive",10) == 0)
    {
      keepAlive = true;
      //printf("HTTP: keep-alive\n");
    }
    else if(strncmp(line+12,"close",5) == 0)
    {
      keepAlive = false;
      //printf("HTTP: close\n");
    }
    return false;
  }
  else
  {
    return false;
  }

}

void HttpClientProcess_c::HandleGET(char* line)
{
  char* firstSpace = strchr(line,' ');
  if(firstSpace != nullptr) { *firstSpace = 0; }

  int urlLength = strlen(line);

  int extrabytes = 8; /* bytes for directory name */
  if(urlLength <= 1) { extrabytes+= 10; } /* bytes for "index.html" */

  char* fileName = new char[urlLength + extrabytes];

  strcpy(fileName,"/http");
  strcat(fileName,line);

  if(urlLength <= 1) { strcat(fileName,"index.html"); }


  #if DEBUG_HTTP > 0
  printf("try open (%s) file\n",fileName);
  #endif
  
  File_c* file = FileSystem_c::OpenFile( fileName,"r");

  if(file == nullptr)
  {
    //SendAnswer(client_p,WEB_NOT_FOUND,"");
    if(keepAlive == false)
    {
      socket->CloseAfterSend();
    }
    HttpInterface_c::SendAnswer(socket,WEB_NOT_FOUND,"Content-Length: 0\r\n");
    #if DEBUG_HTTP > 0
    printf("File not found\n");
    #endif
  }
  else
  {
    uint32_t fileSize = file->GetSize();
    #if DEBUG_HTTP > 0
    printf("File size = %d\n",fileSize);
    #endif
    char extraLine[32];
    sprintf(extraLine,"Content-Length: %d\r\n",fileSize);

    HttpInterface_c::SendAnswer(socket,WEB_REPLY_OK,extraLine);

    int bytesToSend = fileSize;
   

    while(bytesToSend > 0)
    {
      uint16_t fileBufSize = 4096;
      if(bytesToSend<fileBufSize)
      {     
        uint8_t* fileBuf = new uint8_t[fileBufSize];   
        file->Read(fileBuf,bytesToSend);
        //FreeRTOS_send(socket,fileBuf,bytesToSend,0);

        if(keepAlive == false)
        {
          socket->CloseAfterSend();
        }
        //printf("HTTP: send %d bytes\n",bytesToSend);
        if(socket->Send(fileBuf,bytesToSend,10000) != bytesToSend)
        {
          break;
        }

        bytesToSend = 0;
      }
      else
      {        
        uint8_t* fileBuf = new uint8_t[fileBufSize];
        file->Read(fileBuf,fileBufSize);
        //FreeRTOS_send(socket,fileBuf,fileBufSize,0);
        //printf("HTTP: send %d bytes\n",bytesToSend);
        if(socket->Send(fileBuf,fileBufSize,10000) != fileBufSize)
        {
          break;
        }
        bytesToSend -= fileBufSize;
      }  
    }

    file->Close();
  }


  delete[] fileName;


}

const char * HttpInterface_c::webCodename (client_http_answer_et aCode)
{
	switch (aCode) {
	case WEB_REPLY_OK:	//  = 200,
		return "OK";
	case WEB_NO_CONTENT:    // 204
		return "No content";
	case WEB_BAD_REQUEST:	//  = 400,
		return "Bad request";
	case WEB_UNAUTHORIZED:	//  = 401,
		return "Authorization Required";
	case WEB_NOT_FOUND:	//  = 404,
		return "Not Found";
	case WEB_GONE:	//  = 410,
		return "Done";
	case WEB_PRECONDITION_FAILED:	//  = 412,
		return "Precondition Failed";
	case WEB_INTERNAL_SERVER_ERROR:	//  = 500,
		return "Internal Server Error";
	}
	return "Unknown";
}

void HttpInterface_c::SendAnswer(SocketTcp_c* socket,client_http_answer_et answer,const char* extraLine,const char* data)
{
  uint16_t wantedSize = 128 + strlen(extraLine);

  if(data != NULL)
  {
    wantedSize += strlen(data);
  }

  uint8_t* dataBuf = new uint8_t[wantedSize];

/*
  sprintf((char*) dataBuf,
          "HTTP/1.1 %d %s\r\n"
          "Content-Type: text/html\r\n"
          "Connection: close\r\n"
          "%s"
          "\r\n",
          ( int ) answer,
          webCodename (answer),
          extraLine
          );*/

            sprintf((char*) dataBuf,
          "HTTP/1.1 %d %s\r\n"
          "Content-Type: text/html\r\n"
          "Connection: keep-alive\r\n"
          "%s"
          "\r\n",
          ( int ) answer,
          webCodename (answer),
          extraLine
          );
  if(data != NULL)
  {
    strcat((char*) dataBuf,data);
  }

  //FreeRTOS_send(socket,dataBuf,strlen((char*)dataBuf),0);
  socket->Send(dataBuf,strlen((char*)dataBuf),10000);
}
  

