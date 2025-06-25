#ifndef TCPWINDOW_H
#define TCPWINDOW_H

#include "semphr.h"
#include "TcpDataDef.hpp"



#define TCP_SOCKET_SEMPH_TIMEOUT 1000
#define TCP_MAX_PAYLOAD_SIZE 1460

#define TCP_SEND_TIMEOUT 10
#define TCP_SEND_RETRANSMITIONS 3

class TcpWindow_c
{
  //StaticSemaphore_t xSemaphoreBuffer;

  protected:

  //SemaphoreHandle_t semaphore;

  public:

  TcpWindow_c(void);  
};

class TcpWindowRx_c : public TcpWindow_c
{
  uint32_t bufferMasc;

  uint8_t* buffer;

  uint32_t SEQ_lastAcked;
  uint32_t SEQ_lastAcceptable;
  uint32_t SEQ_lastHandled;
  uint32_t SEQ_toAccept;

  uint16_t prevSentWindowSize;
  bool finished;

  public :
    TcpWindowRx_c(void);
  ~TcpWindowRx_c(void);

  void InitWindow(uint32_t startSeqNo, uint32_t bufferSize);

  bool AckData(void);
  int GetWindowSize(void);
  uint32_t GetLastAcked(void) { return SEQ_lastAcked; }

  bool InsertData(uint8_t* dataStart,uint16_t dataSize,uint32_t seqNo,bool lastPacket);
  int GetNoOfBytesWaiting(void);
  int ReadData(uint8_t* buffer, uint16_t bufferCapacity);
  int GetUsage(void);

};

struct TcpSocketDescriptor_st
{
  TcpSocketDescriptor_st* next;
  uint32_t SEQ_start;
  uint16_t dataLength;
  uint16_t seqLength;
  uint8_t retransmitionCnt;
  uint8_t timeToRetransmition;
  uint16_t extraFlags;
  uint8_t* bufferToRelease;

  uint8_t* data_p;
  Packet_st packet;
  




};

class SocketTcp_c;

class TcpWindowTx_c : public TcpWindow_c
{

  TcpSocketDescriptor_st* firstFrame;
  TcpSocketDescriptor_st* lastFrame;


  uint32_t SEQ_lastInserted;
  uint32_t bytesBuffered;
  uint32_t windowSize;
  uint16_t windowScale;
  uint32_t bufferLimit;

  uint32_t GetWindowSize(void);

  public:
  void InitWindow(uint32_t startSeqNo, uint32_t bufferSize);


  TcpWindowTx_c(void);
  ~TcpWindowTx_c(void);
  void UpdateWindowSize(uint32_t newWindowSize);
  void SetWindowScale(uint16_t newWindowScale);

  void StepSeq(void);

  uint32_t GetLastInserted(void) { return SEQ_lastInserted; }
  int GetNoOfBytesWaiting(void);
  uint32_t GetBufferCapacity(void);

  void AckData(uint32_t ackedSeq);

  int InsertData(uint8_t* userBuffer, uint16_t dataLength, uint16_t flag, bool noCopy);

  int ReadData(uint8_t** sendPtr);
  bool SendFromQueue(SocketTcp_c* socket);
  void ScanQueue(void);


};











#endif
