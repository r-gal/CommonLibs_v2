#ifndef COMMONSIGNAL_H
#define COMMONSIGNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "FreeRTOS.h"

#include "queue.h"

#define TRACE_SIGNALS 1

#if TRACE_SIGNALS == 1

struct SignalsStatistics_st
{
  uint32_t noOfSendSignals;
  uint32_t noOfRejectedSignals;
  uint16_t maxQueueUsage;

};

#endif

class SignalList_c;
class Sig_c;

class SignalLayer_c : public SignalList_c
{
  

  private: static QueueHandle_t queueHandler[HANDLE_NO_OF];
  private: static TaskHandle_t taskHandler[HANDLE_NO_OF];

  #if TRACE_SIGNALS == 1
  public: static SignalsStatistics_st signalStatistics[HANDLE_NO_OF];
  #endif 


  public:
  static  void setHandler(HANDLERS_et id,QueueHandle_t handle) { queueHandler[id] = handle; } 
  static  QueueHandle_t GetHandler(HANDLERS_et id) { return queueHandler[id]; }

  static void setTaskHandler(HANDLERS_et id,TaskHandle_t handle) { taskHandler[id] = handle; } 
  static  TaskHandle_t GetTaskHandler(HANDLERS_et id) { return taskHandler[id]; }

  bool CheckSig(Sig_c** recsig_pp,HANDLERS_et handle);
};




class Sig_c : public SignalLayer_c
{
  protected:
  
  HANDLERS_et handler;
  uint8_t sigNo; 



  public: 
  Sig_c(uint8_t sigNo_in,HANDLERS_et handler_in) ;

  public: uint8_t GetSigNo(void) { return sigNo; }

  public: void Send(void);
  public: void SendISR(void);

};

class process_c : public SignalLayer_c
{
  HANDLERS_et ownProcId;

  protected:

  Sig_c* recSig_p;
  bool releaseSig;

  public :


  process_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId,const char* taskName);

  virtual void main(void) = 0;

  void RecSig(void);

};




#endif