#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SignalList.hpp"
#include "commonSignal.hpp"

#include "task.h"
#include "queue.h"

 #if STORE_MEMORY_ID == 1
 #include "HeapManager.hpp"

 #endif

QueueHandle_t SignalLayer_c::queueHandler[HANDLE_NO_OF];
TaskHandle_t SignalLayer_c::taskHandler[HANDLE_NO_OF];
#if TRACE_SIGNALS == 1
SignalsStatistics_st SignalLayer_c::signalStatistics[HANDLE_NO_OF];
#endif

void ProcessWrapper(void *pvParameters)
{
  ((process_c*)pvParameters)->main();
}

process_c::process_c(uint16_t stackSize, uint8_t priority, uint8_t queueSize, HANDLERS_et procId,const char* taskName)
{
  TaskHandle_t xTask;
  xTaskCreate(ProcessWrapper,taskName,stackSize,this,priority,&xTask);
  setTaskHandler(procId,xTask);
 /*
  registerTaskHandler(xTask,TRACE_PROCESS_STACK_SIZE,PROC_TRACE);
  */
  QueueHandle_t QueueHandle;
  QueueHandle =  xQueueCreate( queueSize,sizeof(Sig_c*) );
  setHandler(procId, QueueHandle);
  ownProcId = procId;


}

void process_c::RecSig(void)
{
  recSig_p = NULL;
  QueueHandle_t QueueHandle = GetHandler(ownProcId);
  xQueueReceive(QueueHandle,&recSig_p,portMAX_DELAY );

}

void Sig_c::Send(void)
{
  QueueHandle_t QueueHandle = GetHandler(handler);
  Sig_c* sig_p = this;
  if(xQueueSend(QueueHandle,&sig_p ,( TickType_t ) 0 ) != pdTRUE )
  {
    /*
    Sig_c* recSig_p;
    while(xQueueReceive(QueueHandle,&recSig_p,( TickType_t ) 0 ) == pdTRUE )
    {



    }*/
    printf("send failed\n");
    #if TRACE_SIGNALS == 1
    signalStatistics[handler].noOfRejectedSignals++;
    #endif
  }
  #if TRACE_SIGNALS == 1
  else
  {
    uint16_t signalsInQueue = uxQueueMessagesWaiting(QueueHandle);
    if(signalStatistics[handler].maxQueueUsage < signalsInQueue)
    {
       signalStatistics[handler].maxQueueUsage = signalsInQueue;       
    }
    signalStatistics[handler].noOfSendSignals++;
  }
  #endif
}

void Sig_c::SendISR(void)
{
  BaseType_t  xHigherPriorityTaskWoken = pdFALSE;
  QueueHandle_t QueueHandle = GetHandler(handler);
  Sig_c* sig_p = this;
  if(xQueueSendFromISR( QueueHandle, &sig_p, &xHigherPriorityTaskWoken ) != pdTRUE )
  {
    printf("send isr failed\n");
    #if TRACE_SIGNALS == 1
    signalStatistics[handler].noOfRejectedSignals++;
    #endif
  }
  #if TRACE_SIGNALS == 1
  else
  {
    uint16_t signalsInQueue = uxQueueMessagesWaitingFromISR(QueueHandle);
    if(signalStatistics[handler].maxQueueUsage < signalsInQueue)
    {
       signalStatistics[handler].maxQueueUsage = signalsInQueue;       
    }
    signalStatistics[handler].noOfSendSignals++;
  }
  #endif
}

Sig_c::Sig_c(uint8_t sigNo_in,HANDLERS_et handler_in) 
{
  sigNo = sigNo_in; 
  handler = handler_in;
  #if STORE_MEMORY_ID == 1
//  HeapManager_c::SetId(this, sigNo_in);
  #endif
 }

bool SignalLayer_c::CheckSig(Sig_c** recsig_pp,HANDLERS_et handle)
{
  *recsig_pp = NULL;
   return xQueueReceive( GetHandler(handle),recsig_pp,0 );
}



