 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "FTP_ServerProcess.hpp"
#include "Ethernet.hpp"
#include "FileSystem.hpp"
#include "EthernetTcp.hpp"

#include "RngClass.hpp"

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

const FtpCommand_st ftpCommand[] = 
{

  {"USER"}, /*FTP_COM_USER*/
  {"PASS"}, /*FTP_COM_PASS*/
  {"SYST"}, /*FTP_COM_SYST*/
  {"FEAT"}, /*FTP_COM_FEAT*/
  {"PWD" }, /*FTP_COM_PWD*/
  {"TYPE"}, /*FTP_COM_TYPE*/
  {"PASV"}, /*FTP_COM_PASV*/
  {"PORT"}, /*FTP_COM_PORT*/
  {"LIST"}, /*FTP_COM_LIST*/
  {"CWD"},  /*FTP_COM_CWD*/
  {"SIZE"}, /*FTP_COM_SIZE*/
  {"MDTM"}, /*FTP_COM_MDTM*/
  {"RETR"}, /*FTP_COM_RETR*/
  {"STOR"}, /*FTP_COM_STOR*/
  {"NOOP"}, /*FTP_COM_NOOP*/
  {"QUIT"}, /*FTP_COM_QUIT*/
  {"APPE"}, /*FTP_COM_APPE*/
  {"ALLO"}, /*FTP_COM_ALLO*/
  {"MKD"},  /*FTP_COM_MKD*/
  {"RMD"},  /*FTP_COM_RMD*/
  {"DELE"}, /*FTP_COM_DELE*/
  {"RNFR"}, /*FTP_COM_RNFR*/
  {"RNTO"}, /*FTP_COM_RNTO*/
  {"REST"}, /*FTP_COM_REST*/
  {"UNKN"}  /*FTP_COM_UNKN*/


};

const char* monthNames[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

const char* conStateStr[] = {    
    "FTP_CLIENT_IDLE",
    "FTP_CLIENT_AUTH",
    "FTP_CLIENT_READY",
    "FTP_CLIENT_PASVEST",
    "FTP_CLIENT_PASVCOM",
    "FTP_CLIENT_INCOMING_DATA"};

FtpProcess_c::FtpProcess_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId) : process_c(stackSize,priority,queueSize,procId,"FTP_Serv")
{

 // TimerHandle_t timer = xTimerCreate("",pdMS_TO_TICKS(500),pdTRUE,( void * ) 0,vFunction100msTimerCallback);
 // xTimerStart(timer,0);

  for(int i = 0;i< MAX_NO_OF_FTP_CLIENTS ;i++)
  {
    clientSockets[i] = nullptr;
  }

}

void FtpProcess_c::main(void)
{
   #if DEBUG_PROCESS > 0
   printf("Ftp proc started \n");
   #endif

   serverSocket = new SocketTcp_c(21,nullptr);

  serverSocket->Listen(MAX_NO_OF_FTP_CLIENTS);

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


      
      for(idx = 0;idx< MAX_NO_OF_FTP_CLIENTS ;idx++)
      {
        if(clientSockets[idx] == nullptr)
        {
          clientSockets[idx] = clientSocket;
          break;
        }
      }



      FtpClientProcess_c* newClientTask = new FtpClientProcess_c(clientSocket,idx);

    }
    else if(event.code == SocketEvent_st::SOCKET_EVENT_DELCLIENT)
    {
      SocketTcp_c* clientSocket = (SocketTcp_c*)event.socket;

      int idx = 0;

      for(idx = 0;idx< MAX_NO_OF_FTP_CLIENTS ;idx++)
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
      #if DEBUG_FTP == 1
      printf("FreeRTOS_accept error \n");
      #endif
    } 
  }
}

FtpClientProcess_c::FtpClientProcess_c(SocketTcp_c* socket_,uint8_t procIdx)
{
  socket = socket_;

  char taskName[12];
  sprintf(taskName,"FTP_con_%d",procIdx);

  xTaskCreate( FtpClientTaskWrapper,
               taskName,
               512,
               ( void * ) this,
               tskIDLE_PRIORITY+2,
               NULL );



}

FtpClientProcess_c::~FtpClientProcess_c(void)
{
  if(currentFile != nullptr)
  { 
    currentFile->Close();
    //ff_fclose(currentFile);
    currentFile = nullptr;
  }


  if(prevCmdArg != nullptr)
  {
    delete[] prevCmdArg;
    prevCmdArg = nullptr;
  }
  if(cmdBuffer != nullptr)
  {
    delete[] cmdBuffer;
    cmdBuffer = nullptr;
  }
  if(dataBuffer != nullptr)
  {
    delete[] dataBuffer;
    dataBuffer = nullptr;
  }



}



void FtpClientProcess_c::SetState(FtpClientState_et newState)
{
  #if DEBUG_FTP == 2
  if(connectionState > FTP_CLIENT_INCOMING_DATA) { connectionState = FTP_CLIENT_IDLE; }
  printf("FTP conState change %s->%s\n",conStateStr[connectionState],conStateStr[newState]);
  #endif
  connectionState = newState;

}

void FtpClientProcess_c::MainLoop(void)
{
  strcpy(currentDirectory,"/");
  connectionType = CONTYPE_ASCII;
  SetState(FTP_CLIENT_IDLE);
  dataSocket = nullptr;
  prevCmd = FTP_COM_UNKN;
  prevCmdArg = nullptr;

  cmdBuffer = new char[FTP_CMD_BUFFER_SIZE];
  dataBuffer = nullptr;

  fileOffset = 0;
  fileRestartOffset = 0;
  currentFile = nullptr;
  socket->SetTask();


  //Ethernet_c::RegisterToNetworkDownEvent(socket);
  #if DEBUG_FTP == 1
  printf("FtpClientProcess_c created %X\n", socket);
  #endif

  SendAnswer(FTP_ANS_220_READY_FOR_NEW_USER,' ',"LON Welcome");

  while(1)
  { 

    #if DEBUG_FTP == 2
    printf("ctrlSocket messages = %d\n",uxQueueMessagesWaiting(socket->rxEventQueue));
    if(dataSocket != nullptr)
    {
      printf("dataSocket messages = %d\n",uxQueueMessagesWaiting(dataSocket->rxEventQueue));
    }
    #endif

    SocketEvent_st event;
    socket->WaitForEvent(&event,portMAX_DELAY);

    if(event.socket == socket)
    {
      if(event.code == SocketEvent_st::SOCKET_EVENT_RX)
      {
        int receivedBytes;
        
        do
        {
          receivedBytes = socket->Receive((uint8_t*)cmdBuffer, FTP_CMD_BUFFER_SIZE,portMAX_DELAY);
          if(receivedBytes > 0)
          {
            #if DEBUG_FTP == 2
            printf("FtpClientProcess_c received %d bytes: \n",receivedBytes);
            #endif
            FetchCommand(receivedBytes);
          }
        } while(receivedBytes > 0);
      }
      else if(event.code == SocketEvent_st::SOCKET_EVENT_CLOSE)
      {
        #if DEBUG_FTP == 2
        printf("FTP CtrlSocket Event \n");
        #endif
        if(dataSocket != nullptr)
        {              
          DeleteDataSocket();
        }
        Close();
      }
    }
    else if((dataSocket != nullptr) && (event.socket == dataSocket))
    {
      if(event.code == SocketEvent_st::SOCKET_EVENT_RX)
      {
        int receivedBytes;

        int bytesToRead;
        uint8_t* bufferPtr;

        
        do
        {

          bytesToRead = DATA_BUFFER_SIZE - bytesInBuffer;
          bufferPtr = dataBuffer + bytesInBuffer; 

          receivedBytes = dataSocket->Receive( bufferPtr, bytesToRead, portMAX_DELAY);
          if(receivedBytes > 0)
          {
            #if DEBUG_FTP == 2
            printf("FtpClientData received %d bytes: \n",receivedBytes);
            #endif
            bytesInBuffer += receivedBytes;
            HandleData();
          }
        } while(receivedBytes > 0);
      }
      else if(event.code == SocketEvent_st::SOCKET_EVENT_CLOSE)
      {
        #if DEBUG_FTP == 2
        printf("FTP DataSocket Event \n");
        #endif
        HandleDataEvent();

      }

    }
  }  
}

void FtpClientProcess_c::Close(void)
{

  socket->Shutdown();

  int timeout = 100;
  while(( socket->Recv( (uint8_t*)&cmdBuffer, FTP_CMD_BUFFER_SIZE, 250 ) >= 0)  && (timeout > 0))
  {
      /* Wait for shutdown to complete */
      timeout--;

  }
  socket->Close();
  #if DEBUG_FTP == 1
  printf("FtpClientProcess_c end task \n");
  #endif
  delete this;
  FileSystem_c::CleanTask();

  vTaskDelete(NULL);
}




void FtpClientProcess_c::CtrlSocketKeepAliveCheck(void)
{
  TickType_t actTickVal = xTaskGetTickCount( );

  if((actTickVal - lastCtrlActivityTick) > pdTICKS_TO_MS(2500))
  {
    #if DEBUG_FTP == 1
    printf("FTP forced keep-alive \n");
    #endif

  //  socket->u.xTCP.bits.bSendKeepAlive = pdTRUE_UNSIGNED;
  //  socket->u.xTCP.usTimeout = ( ( uint16_t ) pdMS_TO_TICKS( 2500U ) );
  //  socket->u.xTCP.xLastAliveTime = xTaskGetTickCount();

    lastCtrlActivityTick = actTickVal;


  }

}
/*********************************  DATA SOCKET  ***************************************/

void FtpClientProcess_c::CreateDataSocket(void)
{
   int newDataPort = 2000 + (RngUnit_c::GetRandomVal() & 0x3FF);

   dataBuffer = nullptr;
   bytesInBuffer = 0;

   dataSocket = new SocketTcp_c(newDataPort,nullptr);  
   dataSocket->ReuseListenSocket();
   dataSocket->RedirectEvent(socket);
   dataSocket->SetRxBufferSize(FTP_DATA_SOCKET_BUFFER_SIZE);
   dataSocket->Listen(1);
   dataSocket->SetTask();
   #if DEBUG_FTP == 1
   printf("ftp dataSocket created P=%X\n",dataSocket);
   #endif

  SetState(FTP_CLIENT_PASVEST);
}



void FtpClientProcess_c::DeleteDataSocket(void)
{
  if(dataSocket != nullptr)
  {
    #if DEBUG_FTP == 1
    printf("ftp datasocket delete \n");
    #endif
    
    if((connectionState == FTP_CLIENT_INCOMING_DATA) && (currentFile != nullptr))
    {  
      #if ZERO_COPY_STOR == 1
      #if DEBUG_FTP == 2
      printf("FTP_write %d bytes\n",bytesInBuffer);
      #endif
      //ff_fwrite(dataBuffer,bytesInBuffer,1, currentFile);
      currentFile->Write(dataBuffer,bytesInBuffer);
      bytesInBuffer = 0;
      #endif

      //ff_fclose(currentFile);
      currentFile->Close();
      #if DEBUG_FTP == 2
      printf("FTP_fileclose\n"); 
      #endif 
      currentFile = nullptr;

      SendAnswer(FTP_ANS_226_CLOSING_DATA_CON,'-',nullptr);
    }
    
    dataSocket->Shutdown();


    dataSocket->Close();
    dataSocket = nullptr;

  }

  if(dataBuffer != nullptr)
  {
    delete[] dataBuffer;
    dataBuffer = nullptr;
  }

  SetState(FTP_CLIENT_READY);
}

/********************************** COMMANDS *********************************************************/
void FtpClientProcess_c::SendAnswer(FtpAnswer_et answer,char separator,const char* additionalString)
{
  char* answerStr;
  int answerCode = (int) answer;
  if(additionalString == NULL)
  {
    answerStr = new char[8];
    sprintf(answerStr,"%u \r\n",answerCode);
  }
  else
  {
    answerStr = new char[strlen(additionalString) + 8];
    sprintf(answerStr,"%u%c%s\r\n",answerCode,separator,additionalString);
  }  
  socket->Send((uint8_t*)answerStr,strlen(answerStr),10000);
  //delete answerStr;
  lastCtrlActivityTick = xTaskGetTickCount( );
}

void FtpClientProcess_c::FetchCommand(int receivedBytes)
{
  char* argPtr = nullptr;

  char* separatorPos = strchr(cmdBuffer,' ');
  char* endPos = strchr(cmdBuffer,'\r');

  FtpCommand_et command = FTP_COM_UNKN;
  if((endPos != nullptr) && (endPos - cmdBuffer <= receivedBytes))
  {
    *endPos = 0;
    if((separatorPos != nullptr) && ((separatorPos+1) - cmdBuffer < receivedBytes))
    {
      *separatorPos = 0;
      argPtr = separatorPos+1;

    }  

    int commandIdx = 0;

    while(commandIdx < FTP_COM_NOOFCOM)
    {
      if(strcmp(cmdBuffer,ftpCommand[commandIdx].commandStr) == 0)
      {
        command = (FtpCommand_et)commandIdx;
        break;
      }
      else
      {
        commandIdx++;
      }
    }


  }

  #if DEBUG_FTP == 1
  printf("Fetched command = %s, arg= %s \n",ftpCommand[command].commandStr,argPtr == nullptr? "": argPtr);
  #endif
  /* seqence check */

  if((connectionState == FTP_CLIENT_IDLE) && (command != FTP_COM_USER))
  {
     SendAnswer(FTP_ANS_503_BAD_SEQUENCE,' ',nullptr);
     return;
  }

  if((connectionState == FTP_CLIENT_AUTH) && (command != FTP_COM_PASS))
  {
     SendAnswer(FTP_ANS_503_BAD_SEQUENCE,' ',nullptr);
     return;
  }

  if(connectionState == FTP_CLIENT_INCOMING_DATA)
  {
    /* ignore commands in this state */
    return;

  }

  if((prevCmd == FTP_COM_RNFR) && (command != FTP_COM_RNTO) )
  {
    if(prevCmdArg != nullptr)
    {  
      delete prevCmdArg;
    }
    SendAnswer(FTP_ANS_503_BAD_SEQUENCE,' ',nullptr);
  }
  else if((prevCmd == FTP_COM_REST) && (command != FTP_COM_STOR) )
  {
    if(prevCmdArg != nullptr)
    {  
      delete prevCmdArg;
    }
    SendAnswer(FTP_ANS_503_BAD_SEQUENCE,' ',nullptr);
  }
  else
  {
    switch(command)
    {
      case FTP_COM_USER:
        SetState (FTP_CLIENT_AUTH);
        SendAnswer(FTP_ANS_331_USER_OK_NEED_PASS,' ',nullptr);
        break;
      case FTP_COM_PASS:
        SetState (FTP_CLIENT_READY);
        fileRestartOffset = 0;
        SendAnswer(FTP_ANS_230_USER_LOGGED,' ',nullptr);
        break;    
      case FTP_COM_FEAT:
        SendAnswer(FTP_ANS_502_COMMAND_NOT_IMPLEMENTED,' ',nullptr);
        break;
      case FTP_COM_PWD:
        {
        char* additionalString = new char[256];
        additionalString[0] = '\"';
        FileSystem_c::GetCwd(additionalString+1,256);

        strcat(additionalString,"\" is the current directory");
        SendAnswer(FTP_ANS_257_PATH,' ',additionalString);
        delete[] additionalString;
        }
        break;
      case FTP_COM_TYPE:
        HandleTYPE(argPtr);
        break;
      case FTP_COM_PASV:
        HandlePASV(argPtr);
        break;
      case FTP_COM_LIST:
        HandleLIST(argPtr);
        break;
      case FTP_COM_CWD:
        HandleCWD(argPtr);
        break;
      case FTP_COM_PORT:
        SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);
      break;
        case FTP_COM_RETR:
        HandleRETR(argPtr);
        break;
      case FTP_COM_MKD:
        HandleMKD(argPtr);
        break;
      case FTP_COM_RMD:
        HandleRMD(argPtr);
        break;
      case FTP_COM_RNFR:
        HandleRNFR(argPtr);
        break;
      case FTP_COM_RNTO:
        HandleRNTO(argPtr);
        break;
      case FTP_COM_STOR:
        HandleSTOR(argPtr);
        break;
      case FTP_COM_NOOP:
        SendAnswer(FTP_ANS_200_COMMAND_OK,' ',nullptr);
        break;
      case FTP_COM_REST:
        HandleREST(argPtr);
        break;
      case FTP_COM_DELE:
        HandleDELE(argPtr);
        break;
     case FTP_COM_ALLO:    
        SendAnswer(FTP_ANS_202_NOT_IMPL_SUPERFLUOUS,' ',nullptr);
        break;
        

      case FTP_COM_SIZE:
      case FTP_COM_MDTM:
      case FTP_COM_QUIT:
      case FTP_COM_APPE:
      


        SendAnswer(FTP_ANS_502_COMMAND_NOT_IMPLEMENTED,' ',nullptr);
        break;


      case FTP_COM_SYST:
      default:
      SendAnswer(FTP_ANS_502_COMMAND_NOT_IMPLEMENTED,' ',nullptr);
      break;
    }
  }
  prevCmd = command;
}

void FtpClientProcess_c::HandleTYPE(char* arg)
{
  if(arg[0] == 'A')
  {
    connectionType = CONTYPE_ASCII;
    SendAnswer(FTP_ANS_200_COMMAND_OK,' ',nullptr);
  }
  else if(arg[0] == 'I')
  {
    connectionType = CONTYPE_BINARY;
    SendAnswer(FTP_ANS_200_COMMAND_OK,' ',nullptr);
  }
  else
  {
    SendAnswer(FTP_ANS_504_COMM_NOT_IMPL_FOR_PARAM,' ',nullptr);
  }
}

void FtpClientProcess_c::HandleCWD(char* arg)
{

  int r = FileSystem_c::ChDir(arg);

  if(r == 0 )
  {      
    SendAnswer(FTP_ANS_200_COMMAND_OK,' ',nullptr);
  }
  else
  {
    SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);
  }
}
void FtpClientProcess_c::HandleLIST(char* arg)
{
  
  if((connectionState == FTP_CLIENT_PASVEST) && (dataSocket != nullptr))
  {
    FindData_c* pxFindStruct = new FindData_c;
    char* lineBuf;
    
    int entriesCnt = 0;

     SendAnswer(FTP_ANS_125_STARTING_TRANSFER,' ',nullptr);
    /* The first parameter to ff_findfist() is the directory being searched.  Do
    not add wildcards to the end of the directory name. */

    bool res;
    if(pxFindStruct->FindFirst((char*)"") == true)
    { 
      do
      {
        lineBuf = new char[128];
        char* lineBufPointer = lineBuf;
        /* insert entry type */
        if( pxFindStruct->entryData.attributes & FF_FAT_ATTR_DIR )
        {
          strcpy(lineBuf,"d");
        }
        else
        {
          strcpy(lineBuf,"-");
        }
        lineBufPointer++;

        /* insert file rights */
        strcpy(lineBufPointer,"rw-rw-r--");
        lineBufPointer += 9;

        /*insert some shit */
        strcpy(lineBufPointer, "    1 freertos plusfat" );
        lineBufPointer += 22;

        /* insert file size */
        sprintf(lineBufPointer, " %13u",pxFindStruct->entryData.fileSize);
        lineBufPointer += strlen(lineBufPointer);

        /* insert fileTime*/
        //sprintf(lineBufPointer, " Jul 17 12:45"); 

        InsertTimeString(lineBufPointer,&pxFindStruct->entryData.modificationTime);
        /*
        sprintf(lineBufPointer,"%s %2u %02u:%02u", 
        monthNames[pxFindStruct-> xDirectoryEntry.xModifiedTime.Month-1],
        pxFindStruct->xDirectoryEntry.xModifiedTime.Day,
        pxFindStruct->xDirectoryEntry.xModifiedTime.Hour,
        pxFindStruct->xDirectoryEntry.xModifiedTime.Minute);*/

  /*Jul 17 12:45*/
  /*TODO Apr 10  1997*/

       lineBufPointer += strlen(lineBufPointer);

        /* insert fileName and end of line*/
        sprintf(lineBufPointer," %s\r\n",pxFindStruct->entryData.name);



        res = pxFindStruct->FindNext();

        if(res == false)
        {
          static const TickType_t xReceiveTimeOut = portMAX_DELAY;
          dataSocket->CloseAfterSend();
        }


        uint16_t bytesSent = dataSocket->Send((uint8_t*)lineBuf,strlen(lineBuf),10000);
        entriesCnt++;
        CtrlSocketKeepAliveCheck();

      } while(res == true );
      
    }

    /* Free the allocated FF_FindData_t structure. */
    delete pxFindStruct ;
    lineBuf = new char[128];
    //DeleteDataSocket();
    sprintf(lineBuf,"Options: -l\r\n226-%u matches total\r\n226 Total 6 KB (0 %% free)",entriesCnt);
    SendAnswer(FTP_ANS_226_CLOSING_DATA_CON,'-',lineBuf);


    delete[] lineBuf;

  }
  else
  {
    SendAnswer(FTP_ANS_501_SYNTAX_ERROR,' ',nullptr);
  }

  
}


void FtpClientProcess_c::InsertTimeString(char* buf,SystemTime_st* timeStruct)
{
  uint32_t actTime =  TimeUnit_c::MkSystemTime();

  /* check if time is correct */
  bool timeOk = true;

  if(timeStruct->Hour > 23) { timeOk=false;  }
  else if(timeStruct->Minute>59) { timeOk=false;  }
  else if((timeStruct->Day<1) || (timeStruct->Day>31)) { timeOk=false;  }
  else if((timeStruct->Month<1) || (timeStruct->Month>12)) { timeOk=false;  }
  else if((timeStruct->Year<1900) || (timeStruct->Year>2100)) { timeOk=false;  }

  if(timeOk == true)
  {

    uint32_t fileTime = TimeUnit_c::MkTime(timeStruct);

    if((actTime > fileTime) && ( actTime - fileTime > (150*24*60*60))) 
    {
      /*Apr 10  1997*/

      sprintf(buf," %s %2u %04u", 
      monthNames[timeStruct->Month-1],
      timeStruct->Day,
      timeStruct->Year);
    }
    else
    {
      /*Jul 17 12:45*/

      sprintf(buf," %s %2u %02u:%02u", 
      monthNames[timeStruct->Month-1],
      timeStruct->Day,
      timeStruct->Hour,
      timeStruct->Minute);
    } 
  }
  else
  {
    /* dummy time */
    sprintf(buf, " Jan 01 1900"); 
  }
}

void FtpClientProcess_c::HandlePASV(char* arg)
{
  if(connectionState == FTP_CLIENT_READY)
  {
    CreateDataSocket();
  }


  if((connectionState == FTP_CLIENT_PASVEST) && (dataSocket != nullptr))
  {
    SocketAdress_st sockAddr;

    dataSocket->GetLocalAddress(&sockAddr);

    uint8_t ipAdr[4];
    uint8_t port[2];

    ipAdr[3] = sockAddr.ip>>24; //192;
    ipAdr[2] = sockAddr.ip>>16; //168; 
    ipAdr[1] = sockAddr.ip>>8 ; //0;
    ipAdr[0] = sockAddr.ip;     //109;
    port[1] =  sockAddr.port >> 8;
    port[0] =  sockAddr.port;


    char* additionalString = new char[128];
    sprintf(additionalString,"Entering Passive Mode (%u,%u,%u,%u,%u,%u)",ipAdr[3],ipAdr[2],ipAdr[1],ipAdr[0],port[1],port[0]);
    SendAnswer(FTP_ANS_227_ENTERING_PASSIVE_MODE,' ',additionalString);
    delete[] additionalString;
  }
  else
  {
    SendAnswer(FTP_ANS_503_BAD_SEQUENCE,' ',nullptr);
  }  
}

void FtpClientProcess_c::HandleRETR(char* arg)
{
 /* char* fileName = new char[CURRENT_DIRECTORY_SIZE + strlen(arg)+1];
  strcpy(fileName,currentDirectory);
  if(strlen(currentDirectory) > 1)
  {
    strcat(fileName,"/");
  }
  strcat(fileName,arg);*/

  //FF_FILE * file = ff_fopen( fileName,"r");
  File_c* file = FileSystem_c::OpenFile( arg,"r");

  if(file != nullptr)
  {
    int fileSize = file->GetSize();

    int dataToSend = fileSize;

    bool failDuringSending = false;

    if((connectionState == FTP_CLIENT_PASVEST) && (dataSocket != nullptr))
    {
      SendAnswer(FTP_ANS_125_STARTING_TRANSFER,' ',nullptr);


      while(dataToSend > 0)
      {
        if(dataToSend > READ_DATA_BUFFER_SIZE)
        {
          //ff_fread(dataBuffer,1,READ_DATA_BUFFER_SIZE,file);
          dataBuffer = new uint8_t[READ_DATA_BUFFER_SIZE];
          file->Read(dataBuffer,READ_DATA_BUFFER_SIZE);
          if(dataSocket->Send(dataBuffer,READ_DATA_BUFFER_SIZE,10000) != READ_DATA_BUFFER_SIZE)
          {
            failDuringSending = true;
            break;
          }
          dataBuffer = nullptr;
          dataToSend -= READ_DATA_BUFFER_SIZE ;
        }
        else
        {
          //ff_fread(dataBuffer,1,dataToSend,file);
          dataBuffer = new uint8_t[READ_DATA_BUFFER_SIZE];
          file->Read(dataBuffer,dataToSend);
          static const TickType_t xReceiveTimeOut = portMAX_DELAY;
          dataSocket->CloseAfterSend();
          if(dataSocket->Send(dataBuffer,dataToSend,10000) != dataToSend)
          {
            failDuringSending = true;
            break;          
          }  
          dataBuffer = nullptr;  
          dataToSend = 0;
        }
        CtrlSocketKeepAliveCheck();
      }

      if(failDuringSending)
      {
        SendAnswer(FTP_ANS_426_CONNECTION_CLOSED,'-',nullptr);
      }
      else
      {
        SendAnswer(FTP_ANS_226_CLOSING_DATA_CON,'-',nullptr);
      }

    }
    else
    {
      /* not connected */
      SendAnswer(FTP_ANS_501_SYNTAX_ERROR,' ',nullptr);
    }
    //ff_fclose(file);
    file->Close();
  }
  else
  { 
    /* file not found */
    SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);
  }
  //delete[] fileName;
}

void FtpClientProcess_c::HandleMKD(char* arg)
{
  bool r = FileSystem_c::MkDir(arg);
  if(r == true)
  {
    SendAnswer(FTP_ANS_257_PATH,' ',nullptr);
  }
  else
  {
    SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);
  }

}


void FtpClientProcess_c::HandleRMD(char* arg)
{

  bool r = FileSystem_c::RmDir(arg);
  if(r == true)
  {
    SendAnswer(FTP_ANS_250_RE_FILE_OK,' ',nullptr);
  }
  else
  {
    SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);
  }

}

void FtpClientProcess_c::HandleRNFR(char* arg)
{
  if(prevCmdArg == nullptr) 
  {
    prevCmdArg = new char[CURRENT_DIRECTORY_SIZE];
    strncpy(prevCmdArg,arg,CURRENT_DIRECTORY_SIZE);
  }  
  SendAnswer(FTP_ANS_350_REQ_FILE_PENING,' ',nullptr);

}
void FtpClientProcess_c::HandleRNTO(char* arg)
{
  if(prevCmd != FTP_COM_RNFR)
  {
    SendAnswer(FTP_ANS_503_BAD_SEQUENCE,' ',nullptr);
  }
  else
  {
    if(prevCmdArg != nullptr)
    {

      //r = ff_rename(prevCmdArg,arg,pdFALSE);
      bool r = FileSystem_c::Rename(prevCmdArg,arg);
      if(r == true)
      {
        SendAnswer(FTP_ANS_250_RE_FILE_OK,' ',nullptr);
      }
      else
      {
        SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);
      }

      delete[] prevCmdArg;
      prevCmdArg = nullptr;
    }
    else
    {
       SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);
    }
  }
}

uint32_t startTime;

uint32_t accWorkTime;
uint32_t startWorkTime;
uint32_t accSDWriteTime;
uint32_t startSDWriteTime;

uint32_t CalcTimeDiff(uint32_t startTime, uint32_t stopTime)
{
  uint32_t timeDiff = 0;
  if(startTime < stopTime)
  { 
    timeDiff = stopTime - startTime;
  } 
  else
  {
    timeDiff = stopTime + (0xFFFFFFFF - startTime);
  }
  return timeDiff;
}

void FtpClientProcess_c::HandleSTOR(char* arg)
{
  bool ok = true;
  if(currentFile != nullptr)
  {
    currentFile->Close();
    currentFile = nullptr;
  }


  if(fileRestartOffset > 0)
  {
    /*restart transfer file */
    currentFile = FileSystem_c::OpenFile( arg, "a" );
    fileOffset = fileRestartOffset;
    fileRestartOffset = 0; 
    sendBytes = fileRestartOffset;
    
    if((currentFile == nullptr ) || (currentFile->GetSize() < fileRestartOffset))
    {
      SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);  
      ok = false;
    }
    else 
    {      
      int r = currentFile->Seek(fileOffset,File_c::SEEK_MODE_SET);
      if(r != 0)
      {
        /* invalid length */
        SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);  
        ok = false;
      }
    }

  }
  else
  {
    /* start transfer file */
    #if DEBUG_FTP == 2
    printf("FTP_fileopen [%s]\n",arg);  
    #endif
    
    currentFile = FileSystem_c::OpenFile( arg, "w" );
    fileOffset = 0;
    sendBytes = 0;
    if(currentFile == nullptr )
    {
      SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);  
      ok = false;
    }
  }

  if(ok)
  {
    if((connectionState == FTP_CLIENT_PASVEST) && (dataSocket != nullptr))
    {
      dataBuffer = new uint8_t[DATA_BUFFER_SIZE];
      SendAnswer(FTP_ANS_150_ABOUT_TO_DATA_CON,' ',nullptr);
      SetState (FTP_CLIENT_INCOMING_DATA);

      startTime = xTaskGetTickCount(); 
      accSDWriteTime = 0;
      accWorkTime = 0;

    }
    else
    {
      SendAnswer(FTP_ANS_425_CANT_OPEN_DATACON,' ',nullptr);
      ok = false;
    }
  }

  if((ok == false) && ( currentFile != nullptr))
  {
    currentFile->Close();
    currentFile = nullptr;
  }
  if(ok == false)
  {
    SetState (FTP_CLIENT_READY);
  }

}

void FtpClientProcess_c::HandleData(void)
{

  startWorkTime = xTaskGetTickCount(); 

  if((connectionState == FTP_CLIENT_INCOMING_DATA) && (currentFile != nullptr))
  {
  #if ZERO_COPY_STOR == 1
    if(bytesInBuffer == DATA_BUFFER_SIZE)
    {
      #if DEBUG_FTP == 2
      printf("FTP_write %d bytes, total %d\n",DATA_BUFFER_SIZE,sendBytes);
      #endif
      sendBytes += DATA_BUFFER_SIZE;
      //ff_fwrite(dataBuffer,DATA_BUFFER_SIZE,1, currentFile);
      startSDWriteTime = xTaskGetTickCount(); 
      currentFile->Write(dataBuffer,DATA_BUFFER_SIZE);
      accSDWriteTime += CalcTimeDiff(startSDWriteTime,xTaskGetTickCount());
      bytesInBuffer = 0;
    }

  #else
    #if DEBUG_FTP == 2
    printf("FTP_write %d bytes, total %d\n",bytesInBuffer,sendBytes);
    #endif
    sendBytes += bytesInBuffer;
    ff_fwrite(dataBuffer,bytesInBuffer,1, currentFile);
    bytesInBuffer = 0;

  #endif

    

  }
  accWorkTime += CalcTimeDiff(startWorkTime,xTaskGetTickCount());





  CtrlSocketKeepAliveCheck();
}

void FtpClientProcess_c::HandleDataEvent(void)
{
  startWorkTime = xTaskGetTickCount(); 
  if(connectionState == FTP_CLIENT_INCOMING_DATA)
  {
    if(currentFile != nullptr)
    {
      #if ZERO_COPY_STOR == 1
      #if DEBUG_FTP == 2
      printf("FTP_write last %d bytes\n",bytesInBuffer);
      #endif
      //ff_fwrite(dataBuffer,bytesInBuffer,1, currentFile);
      startSDWriteTime = xTaskGetTickCount();
      currentFile->Write(dataBuffer,bytesInBuffer);
      accSDWriteTime += CalcTimeDiff(startSDWriteTime,xTaskGetTickCount());
      bytesInBuffer = 0;
      #endif

      //ff_fclose(currentFile);
      currentFile->Close();
      #if DEBUG_FTP == 2
      printf("FTP_fileclose\n");  
      #endif
      currentFile = nullptr;
    }
    SendAnswer(FTP_ANS_226_CLOSING_DATA_CON,'-',nullptr);

    
  }
  DeleteDataSocket();
  accWorkTime += CalcTimeDiff(startWorkTime,xTaskGetTickCount());

  uint32_t totalTime = CalcTimeDiff(startTime,xTaskGetTickCount());
  #if DEBUG_FTP > 0
  printf("total time=%d, workTime=%d, sdWorkTime = %d\n",totalTime,accWorkTime,accSDWriteTime);
  #endif

}


void FtpClientProcess_c::HandleREST(char* arg)
{
  int offset = 0;

  while( *arg != 0 )
  {
    if((*arg >= '0') && (*arg <='9'))
    {
      offset = offset * 10;
      offset += (*arg - '0');
    }
    else
    {
      SendAnswer(FTP_ANS_501_SYNTAX_ERROR,' ',nullptr);      
    }
    arg++;
  }
  fileRestartOffset = offset;
  SendAnswer(FTP_ANS_350_REQ_FILE_PENING,' ',nullptr);
}

void FtpClientProcess_c::HandleDELE(char* arg)
{
  char* fileName = new char[CURRENT_DIRECTORY_SIZE + strlen(arg)+1];
  if(arg[0] != '/')
  {
    strcpy(fileName,currentDirectory);
    if(strlen(currentDirectory) > 1)
    {
      strcat(fileName,"/");
    }
    strcat(fileName,arg);
  }
  else
  {
    strcpy(fileName,arg);
  }

  //int result = ff_remove(fileName);
  bool result = FileSystem_c::Remove(fileName);

  if(result == true)
  {
    SendAnswer(FTP_ANS_250_RE_FILE_OK,' ',nullptr);
  }
  else
  {
    SendAnswer(FTP_ANS_550_REQ_NOT_TAKEN,' ',nullptr);
  }
  delete[] fileName;
}