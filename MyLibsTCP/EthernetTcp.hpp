#ifndef ETHERNETTCP_H
#define ETHERNETTCP_H

//#include "common.hpp"
#include "EthernetCommon.hpp"
#include "EthernetSocket.hpp"
#include "EthernetIp.hpp"

#include "semphr.h"
#include "message_buffer.h"
#include "stream_buffer.h"

#include "TcpWindow.hpp"
#include "SignalList.hpp"



#define TCP_SYN_FLAG 0x0002
#define TCP_ACK_FLAG 0x0010
#define TCP_FIN_FLAG 0x0001
#define TCP_RST_FLAG 0x0004
#define TCP_PSH_FLAG 0x0008


#define TCP_RX_BUFFER_SIZE 1024
#define TCP_TX_BUFFER_SIZE 1024

#define MAX_NO_OF_CLIENTS 8

#ifdef ETH_TEST
#define TCP_ALIVE_TIMER 50
#define TCP_TXACK_TIMER 5
#else
#define TCP_ALIVE_TIMER 50000
#define TCP_TXACK_TIMER 500
#endif
#define TCP_TX_SEQSTART 0

#define ACK_TIMER_TOP 5

#define TCP_OPTION_NOP 0x01
#define TCP_OPTION_WINSCALE 0x03
#define TCP_OPTION_MAXSEGMENT 0x02




class SocketTcp_c : public Socket_c
{
  enum State_et
  {
    INIT,
    LISTEN,
    SYN_SENT,
    SYN_RECEIVED,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSE_WAIT,
    CLOSING,
    LAST_ACK,
    TIME_WAIT,
    CLOSED
  } state;

  uint32_t clientIp;
  uint16_t clientPort;
  uint8_t clientMac[6];

  uint32_t rxBufferSize;

  TaskHandle_t acceptWaitingTask;

  TcpWindowRx_c rxWindow;
  TcpWindowTx_c txWindow;


  bool closeAfterSent;
  bool deleteWhenPossible;
  uint8_t timeWait;
  uint16_t keepAliveCnt;
  uint8_t keepAliveProbesCnt;

  bool reuseListenSocket;

  bool ackToSent;

  TaskHandle_t task;

  void SetState( State_et newState);

  void HandleRoutedPacket(uint8_t* packet_p,uint16_t packetSize);
  uint32_t GetTcpHeaderSize(void) { return sizeof(TcpHeader_st); }
  void FillTcpHeader(Packet_st* packet, uint16_t flags, uint32_t setNo);
  void SendAck(uint16_t flags);
  void SendSynAck(void);
  void SendFinAck(void);
  void SendProbe(void);
  void ResetConnection(void);

  int GetOption(Packet_st* packet, uint8_t optionKind);

  SocketTcp_c* GetChildSocket(uint32_t clientIp, uint16_t clientPort);

  void PrintSocketInfo(void);  

  
  void ResetKeepAliveCounter(void);
  void Tick(void);
    
  void TxQueueTick(void); 

  ~SocketTcp_c(void);
  void HandlePacket(uint8_t* packet_p,uint16_t packetSize);

  void ShutdownInternal(void);
  void CloseEvents(void);


  public:

  /* external functions, may be called only from tcpProcess */

  void FillDataHeaders(Packet_st* packet,uint16_t dataSize, uint32_t seqNo, uint16_t flags);
  void LoopOverChildList( uint8_t oCode);
  static SocketTcp_c* GetTcpSocket(uint32_t clientIp,uint16_t clientPort, uint16_t port);
  void SendFromQueue(void);
  void Request(socketTcpRequestSig_c* recSig_p);  
  void HandleDataSend(socketSendReqSig_c* recSig_p);
  void HandleDataReceive(socketReceiveReqSig_c* recSig_p);

  /* user functions, may be called from user task */

  SocketTcp_c(uint16_t port, Socket_c* parentSocket);
  
  SocketTcp_c* Accept(void);
  void Listen(uint8_t clientMax_); 
  int Send(uint8_t* buffer,uint32_t dataSize,uint32_t delay);
  int Recv(uint8_t* buffer,uint32_t dataSize,uint32_t delay);
  int Receive(uint8_t* buffer,uint32_t dataSize,uint32_t delay);
  int RecvCount(void);

  void ReuseListenSocket(void);
  void CloseAfterSend(void);
  void Shutdown(void);
  void Close(void);
  void GetLocalAddress(SocketAdress_st* socketAdress);
  void SetRxBufferSize(uint32_t bufferSize);
  void SetTask(void);

  void DisconnectAllChild(void);

  /* ***************************************************/

  void PrintInfo(char* buffer);
  
  


  
};


































#endif
