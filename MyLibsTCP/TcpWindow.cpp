#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"

#include "TcpWindow.hpp"

#include "EthernetTcp.hpp"

TcpWindow_c::TcpWindow_c(void)
{ 

  //semaphore = xSemaphoreCreateBinaryStatic( &xSemaphoreBuffer );
  //xSemaphoreGive(semaphore);
}


/******************************* RX WINDOW ***********************************/

TcpWindowRx_c::TcpWindowRx_c(void)
{ 
  buffer = nullptr;
  
}
TcpWindowRx_c::~TcpWindowRx_c(void) 
{ 
  if(buffer != nullptr) 
  { 
    #if SET_SOCKET_BUFFERIN_CCM == 1
    vPortFreeStack(buffer);
    #else
    delete[] buffer;
    #endif

    
  }
}

void TcpWindowRx_c::InitWindow(uint32_t startSeqNo, uint32_t bufferSize)
{
  startSeqNo++;
  SEQ_lastAcked = startSeqNo;
  SEQ_lastAcceptable = startSeqNo + bufferSize;
  SEQ_lastHandled = startSeqNo;
  SEQ_toAccept = startSeqNo;
  prevSentWindowSize = 0;
  finished = false;

  bufferMasc = bufferSize-1;
  #if SET_SOCKET_BUFFERIN_CCM == 1
  buffer = (uint8_t*)pvPortMallocStack(bufferSize);
  #else
  buffer = new uint8_t[bufferSize];
  #endif

}



bool TcpWindowRx_c::AckData(void)
{
  bool ackNeeded = false;

  if(SEQ_lastAcked != SEQ_toAccept)
  {
    ackNeeded = true;
    SEQ_lastAcked = SEQ_toAccept;
  }   

  if(prevSentWindowSize < (SEQ_lastAcceptable - SEQ_lastAcked))
  {
    ackNeeded = true;
  }
    

  return ackNeeded;
}

bool TcpWindowRx_c::InsertData(uint8_t* dataStart,uint16_t dataSize,uint32_t seqNo,bool lastPacket)
{
  uint32_t seqSize = dataSize;

  if(lastPacket) { seqSize++; finished = true;}

  if(seqSize == 0)
  {
    return false;
  }

  if(seqNo + seqSize > SEQ_lastAcceptable)
  {
    /* discard */
    return false;
  }

  if(seqNo < SEQ_toAccept)
  {
    /* discard */
    return true;
  }

  if(seqNo == SEQ_toAccept)
  {
    SEQ_toAccept = seqNo + seqSize;
  }

  if(dataSize > 0)
  {
    uint32_t startWriteIdx = seqNo & bufferMasc;
    uint32_t stopWriteIdx = (seqNo+dataSize) & bufferMasc;

    if(stopWriteIdx > startWriteIdx)
    {
      memcpy(buffer+startWriteIdx,dataStart,dataSize);
    }
    else
    {
      uint16_t sizePart1 = (bufferMasc+1) - startWriteIdx;
      uint16_t sizePart2 = dataSize - sizePart1;

      memcpy(buffer+startWriteIdx,dataStart,sizePart1);
      memcpy(buffer,dataStart+sizePart1,sizePart2);
    }
  }
  return true;

}

int TcpWindowRx_c::GetWindowSize(void)
{
  int windowSize = 0;

  windowSize = SEQ_lastAcceptable - SEQ_lastAcked;
  prevSentWindowSize = windowSize;


  return windowSize;
}

int TcpWindowRx_c::GetUsage(void)
{
  int usage = 0;

  int bytesWaiting = SEQ_toAccept - SEQ_lastHandled;
  usage = (100 * bytesWaiting) / (bufferMasc + 1);

  return usage;

}

int TcpWindowRx_c::GetNoOfBytesWaiting(void)
{
  int bytesWaiting = 0;

  bytesWaiting = SEQ_toAccept - SEQ_lastHandled;
  if(finished) {bytesWaiting--;}

  return bytesWaiting;
}
int TcpWindowRx_c::ReadData(uint8_t* userBuffer, uint16_t bufferCapacity)
{
  int bytesToRead = 0;

  bytesToRead = SEQ_toAccept - SEQ_lastHandled;
  if(finished)
  {
    bytesToRead--;
    if(bytesToRead == 0)
    {
      bytesToRead = -1;
    }
  }
  if(bufferCapacity < bytesToRead)
  {
    bytesToRead = bufferCapacity;
  }

  if(bytesToRead > 0)
  {
    uint32_t startReadIdx = SEQ_lastHandled & bufferMasc;
    uint32_t stopReadIdx = (SEQ_lastHandled+bytesToRead) & bufferMasc;

    if(stopReadIdx > startReadIdx)
    {
      memcpy(userBuffer,buffer+startReadIdx,bytesToRead);      
    }
    else
    {
      uint16_t sizePart1 = (bufferMasc+1) - startReadIdx;
      uint16_t sizePart2 = bytesToRead - sizePart1;
      memcpy(userBuffer,buffer+startReadIdx,sizePart1);   
      memcpy(userBuffer+sizePart1,buffer,sizePart2);
    }
    SEQ_lastHandled += bytesToRead;
    SEQ_lastAcceptable = SEQ_lastHandled + bufferMasc + 1;
  }

  return bytesToRead;
}



/******************************* TX WINDOW ***********************************/

TcpWindowTx_c::TcpWindowTx_c(void)
{
  windowSize = 0;
  windowScale = 0;
  bufferLimit = 8192;
}

TcpWindowTx_c::~TcpWindowTx_c(void) 
{ 
  while(firstFrame != nullptr)
  {
    TcpSocketDescriptor_st* tcpDesc = firstFrame;
    firstFrame = tcpDesc->next;
    if(firstFrame == nullptr)
    {
      lastFrame = nullptr;
    }
    bytesBuffered -= tcpDesc->dataLength;
    if(tcpDesc->data_p != nullptr)
    {
      delete[] tcpDesc->data_p;
    }
    delete tcpDesc; 
  }  
}

void TcpWindowTx_c::InitWindow(uint32_t startSeqNo, uint32_t bufferSize)
{

  firstFrame = nullptr;
  lastFrame = nullptr;
  bytesBuffered = 0;
  SEQ_lastInserted = startSeqNo;

}



void TcpWindowTx_c::AckData(uint32_t ackedSeq)
{
  
  while(firstFrame != nullptr)
  {
    TcpSocketDescriptor_st* tcpDesc = firstFrame;
    if((tcpDesc->SEQ_start + tcpDesc->seqLength) <= ackedSeq)
    {
      #if DEBUG_TX_WINDOW == 1
      printf("del txDesc, dataSize=%d, seqSize=%d, seqStart=%d\n",tcpDesc->dataLength,tcpDesc->seqLength,tcpDesc->SEQ_start);
      #endif
      firstFrame = tcpDesc->next;
      if(firstFrame == nullptr)
      {
        lastFrame = nullptr;
      }
      bytesBuffered -= tcpDesc->dataLength;
      if(tcpDesc->bufferToRelease != nullptr)
      {
        delete[] tcpDesc->bufferToRelease;
      }
      delete tcpDesc;
    }
    else
    {
      break;
    }
  }
  #if DEBUG_TX_WINDOW > 1
  printf("TX ack, ackedSeq=%d, bytesBuffered=%d, SEQ_lastInserted=%d\n",ackedSeq,bytesBuffered,SEQ_lastInserted);
  #endif


}

void TcpWindowTx_c::UpdateWindowSize(uint32_t newWindowSize)
{

  //printf("Set new window size = %d\n",newWindowSize);
  windowSize = newWindowSize;

}

void TcpWindowTx_c::SetWindowScale(uint16_t newWindowScale)
{
  windowScale = newWindowScale;

}

uint32_t TcpWindowTx_c::GetWindowSize(void)
{
  uint32_t scaledWindowSize = windowSize << windowScale;
  return scaledWindowSize;

}

void TcpWindowTx_c::StepSeq(void)
{

  SEQ_lastInserted++;

}

int TcpWindowTx_c::GetNoOfBytesWaiting(void)
{
  int bytesWaiting = 0;

  bytesWaiting = bytesBuffered;

  return bytesWaiting;
}

uint32_t TcpWindowTx_c::GetBufferCapacity(void)
{
  uint32_t clientLimit = GetWindowSize() - bytesBuffered;
  uint32_t ownLimit = bufferLimit - bytesBuffered;

  if(ownLimit < clientLimit)
  { 
    return ownLimit;
  }
  else
  {
    return clientLimit;
  }

   
}

int TcpWindowTx_c::InsertData(uint8_t* userBuffer, uint16_t dataLength, uint16_t flag, bool noCopy)
{
  if((dataLength == 0) && (flag == 0))
  {
    return 0;
  }

  uint32_t freeSpace = GetWindowSize() - bytesBuffered;
  if(dataLength > freeSpace)
  {
    return 0;
  }

  uint16_t bytesSent = 0;


  while((bytesSent < dataLength) || (flag != 0))
  { 
    uint16_t bytesLeft = dataLength - bytesSent; 

    uint16_t packetSize = bytesLeft;

    if(TCP_MAX_PAYLOAD_SIZE < packetSize)
    {
      packetSize = TCP_MAX_PAYLOAD_SIZE;
    }

    TcpSocketDescriptor_st* tcpDesc = new TcpSocketDescriptor_st;

    tcpDesc->SEQ_start = SEQ_lastInserted;
    tcpDesc->dataLength = packetSize;
    tcpDesc->retransmitionCnt = 0;
    tcpDesc->timeToRetransmition = 0;
    tcpDesc->seqLength = packetSize;
    tcpDesc->bufferToRelease = nullptr;

    if(packetSize > 0)
    {
      uint8_t* userBufferTmp = userBuffer + bytesSent;
      if(noCopy)
      {
        tcpDesc->data_p = userBufferTmp;
        if(packetSize == bytesLeft)
        {
          tcpDesc->bufferToRelease = userBuffer;
        }
      }
      else if(packetSize <= SMALL_PACKET_SIZE)
      {
        tcpDesc->data_p = nullptr;
        memcpy( tcpDesc->packet.tcpPayload, userBufferTmp, packetSize);
      }
      else
      {
        tcpDesc->data_p = new uint8_t[packetSize];
        memcpy( tcpDesc->data_p, userBufferTmp, packetSize);
        tcpDesc->bufferToRelease = tcpDesc->data_p;
      } 
    }
    else
    {
      tcpDesc->data_p = nullptr;
    }

    if(packetSize == bytesLeft)
    {
      /* last packet, add flags */
      if(flag & TCP_FIN_FLAG)
      {
        tcpDesc->seqLength ++;
      }

      if(flag & TCP_SYN_FLAG)
      {
        tcpDesc->seqLength ++;
      }
      tcpDesc->extraFlags  = flag; 

      flag = 0;
      
    }
    else
    {
      tcpDesc->extraFlags  = 0; 
    }

    bytesBuffered += packetSize;
    SEQ_lastInserted += tcpDesc->seqLength;
    bytesSent += packetSize;

           
    #if DEBUG_TX_WINDOW > 1
    printf("new TxDesc : size=%d, seqSize = %d, seqStart = %d, bytesBuffered = %d, seq-lastInserted=%d\n",      
    packetSize,
    tcpDesc->seqLength,
    tcpDesc->SEQ_start,
    bytesBuffered, 
    SEQ_lastInserted);
    #endif

    tcpDesc->next = nullptr;

    if(lastFrame != nullptr)
    {
      lastFrame->next = tcpDesc;        
    }
    else
    {
      firstFrame = tcpDesc;
    } 
    lastFrame = tcpDesc;
  }
  
  return bytesSent;
}

bool TcpWindowTx_c::SendFromQueue(SocketTcp_c* socket)
{
  TcpSocketDescriptor_st* tcpDesc = firstFrame;
  bool ackSent = false;
  
  while( tcpDesc != nullptr)
  {
    if(tcpDesc->timeToRetransmition == 0)
    {
      uint16_t flags = TCP_ACK_FLAG | tcpDesc->extraFlags;
      uint32_t dataLength = tcpDesc->dataLength;

      

      socket->FillDataHeaders(&tcpDesc->packet,dataLength,tcpDesc->SEQ_start,flags );

      if(tcpDesc->data_p == nullptr)
      {
        tcpDesc->packet.packetLength += dataLength;
      }

      socket->SendPacket((uint8_t*)&tcpDesc->packet,tcpDesc->packet.packetLength,tcpDesc->data_p,tcpDesc->dataLength);
      ackSent = true;



      tcpDesc->timeToRetransmition = TCP_SEND_TIMEOUT;
      tcpDesc->retransmitionCnt++;
    }



    tcpDesc = tcpDesc->next;
  }
  return ackSent;

}

void TcpWindowTx_c::ScanQueue(void)
{
  TcpSocketDescriptor_st* prevTcpDesc = nullptr;
  TcpSocketDescriptor_st* tcpDesc = firstFrame;
  
  while( tcpDesc != nullptr)
  {
    if(tcpDesc->retransmitionCnt > TCP_SEND_RETRANSMITIONS)
    {
      /* remove from queue */
      TcpSocketDescriptor_st* removedTcpDesc = tcpDesc;
      if(prevTcpDesc != nullptr)
      {
        prevTcpDesc->next = tcpDesc->next;
      }
      else
      {  
        firstFrame = tcpDesc->next;
      }
      if(lastFrame ==  tcpDesc)
      {
        lastFrame = prevTcpDesc;
      }     

      tcpDesc = tcpDesc->next;

      if(removedTcpDesc->bufferToRelease != nullptr)
      {
        delete[] removedTcpDesc->bufferToRelease;
      }
      delete removedTcpDesc;
    }
    else
    { 
      if(tcpDesc->timeToRetransmition > 0)
      {
        tcpDesc->timeToRetransmition--;
      }
      prevTcpDesc = tcpDesc;
      tcpDesc = tcpDesc->next;
    }




  }

}

int TcpWindowTx_c::ReadData(uint8_t** sendPtr)
{

  return 0;

}