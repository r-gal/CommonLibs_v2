 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "TELNET_ServerProcess.hpp"
#include "Ethernet.hpp"
#include "FileSystem.hpp"

#include "commandHandler.hpp"

#include "EthernetTcp.hpp"
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
TelnetProcess_c* TelnetProcess_c::onwRef = nullptr;

TelnetProcess_c::TelnetProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId) : process_c(stackSize,priority,queueSize,procId,"TELNET_serv")
{
  onwRef = this;
 // TimerHandle_t timer = xTimerCreate("",pdMS_TO_TICKS(500),pdTRUE,( void * ) 0,vFunction100msTimerCallback);
 // xTimerStart(timer,0);

  for(int i = 0;i< MAX_NO_OF_TELNET_CLIENTS ;i++)
  {
    clientSockets[i] = nullptr;
  }

  clientListSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(clientListSemaphore);

}

void TelnetProcess_c::main(void)
{
   #if DEBUG_PROCESS > 0
   printf("Telnet proc started \n");
   #endif

  serverSocket = new SocketTcp_c(23,nullptr);
  serverSocket->Listen(MAX_NO_OF_TELNET_CLIENTS);        

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


          xSemaphoreTake(clientListSemaphore,portMAX_DELAY );
          for(idx = 0;idx< MAX_NO_OF_TELNET_CLIENTS ;idx++)
          {
            if(clientSockets[idx] == nullptr)
            {
              clientSockets[idx] = clientSocket;
              break;
            }
          }
          xSemaphoreGive(clientListSemaphore);


          TelnetClientProcess_c* newClientTask = new TelnetClientProcess_c(clientSocket,idx,this);

        }
        else if(event.code == SocketEvent_st::SOCKET_EVENT_DELCLIENT)
        {
          SocketTcp_c* clientSocket = (SocketTcp_c*)event.socket;

          int idx = 0;

          for(idx = 0;idx< MAX_NO_OF_TELNET_CLIENTS ;idx++)
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
          #if DEBUG_TELNET > 0
          printf("TELNET: FreeRTOS_accept error \n");
          #endif
        }
 
  }


}

void TelnetProcess_c::DeleteClient(uint8_t idx)
{
  xSemaphoreTake(clientListSemaphore,portMAX_DELAY );
  clientSockets[idx] = nullptr;
  xSemaphoreGive(clientListSemaphore);
}

void TelnetProcess_c::SendToAllClients(uint8_t* data, uint16_t size)
{
  for(int idx = 0;idx< MAX_NO_OF_TELNET_CLIENTS ;idx++)
  {
    xSemaphoreTake(onwRef->clientListSemaphore,portMAX_DELAY );
    if(onwRef->clientSockets[idx] != nullptr)
    {
      uint8_t* bufTmp = new uint8_t[size];
      memcpy(bufTmp,data,size);
      if(onwRef->clientSockets[idx]->Send(bufTmp,size,0) != size)
      {
          #if DEBUG_TELNET > 0
          printf("TELNET: Send timeout \n");
          #endif
      }

    }
    xSemaphoreGive(onwRef->clientListSemaphore);
  }

}


TelnetClientProcess_c::TelnetClientProcess_c(SocketTcp_c* socket_, uint8_t idx_, TelnetProcess_c* serverProc_)
{
  socket = socket_;
  idx  = idx_;
  serverProcess = serverProc_;
  optionsReceived = false;

  char taskName[12];
  sprintf(taskName,"TEL_con_%d",idx);

  xTaskCreate( TelnetClientTaskWrapper,
               taskName,
               512,
               ( void * ) this,
               tskIDLE_PRIORITY+2,
               NULL );

}

TelnetClientProcess_c::~TelnetClientProcess_c(void)
{

}

void TelnetClientProcess_c::MainLoop(void)
{
  int lBytesReceived;
  #if DEBUG_TELNET > 0
  printf("TelnetClientProcess_c created \n");
  #endif
  socket->SetTask();
  while(1)
  {
    /* Receive another block of data into the cRxedData buffer. */

    //lBytesReceived = socket->Recv((uint8_t*)&cRxedData, TELNET_BUFFER_SIZE,portMAX_DELAY);
    SocketEvent_st event;
    socket->WaitForEvent(&event,portMAX_DELAY);

    if(event.code == SocketEvent_st::SOCKET_EVENT_RX)
    {
      lBytesReceived = socket->Receive((uint8_t*)&cRxedData, TELNET_BUFFER_SIZE,portMAX_DELAY);
      if( lBytesReceived > 0 )
      {
        /* Data was received, process it here. */
        //prvProcessData( cRxedData, lBytesReceived );
        cRxedData[lBytesReceived] = 0;
        #if DEBUG_TELNET > 0
        printf("TelnetClientProcess_c received %d bytes: %s \n",lBytesReceived,cRxedData);
        #endif
        if(lBytesReceived >2)
        {
          if(optionsReceived)
          {
            CommandHandler_c handler(cRxedData,lBytesReceived);
            handler.telnetHandler = this;
            handler.ParseCommand();
          }
          else
          {
            int8_t* sBuf = (int8_t*)cRxedData;
            optionsReceived = true;
            char* buf = new char[32];
            sprintf(buf,TELNET_WELCOME_STRING);
            socket->Send((uint8_t*)buf,strlen(buf),1000);

            //delete[] buf;
          }
        }
      }
    }
    else if(event.code == SocketEvent_st::SOCKET_EVENT_CLOSE)
    {
      Close();
      break;
    }

    
  }
  #if DEBUG_TELNET > 0
  printf("TelnetClientProcess_c end task \n");
  #endif
}

void TelnetClientProcess_c::Send(uint8_t* data, uint16_t size)
{
  if(socket->Send(data,size,10000) != size)
  {
      #if DEBUG_TELNET > 0
      printf("TELNET: Send timeout \n");
      #endif
  }
}

void TelnetClientProcess_c::Close(void)
{

  socket->Shutdown();
  socket->Close();
  //serverProcess->DeleteClient(idx);
  delete this;
  #if USE_FILESYSTEM == 1
  FileSystem_c::CleanTask();
  #endif
  vTaskDelete(NULL);
}
