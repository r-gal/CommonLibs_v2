 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "GeneralConfig.h"

#if CONF_USE_RUNTIME == 1

#include "RunTimeStats.hpp"

#include "timers.h"

/* 

this section must be copid into FreeRTOSConfig.h 

 RUNTIME config 
#define configRUNTIME_THREAD_LOCAL_INDEX 3
#define configRUNTIME_TIMER TIM2
#define configRUNTIME_TIMER_CLK_ENABLE __HAL_RCC_TIM2_CLK_ENABLE
#define configRUNTIME_MAX_NO_OF_TASKS 16
 RUNTIME macros 
uint32_t GetRunTimeTimer(void);
extern void CreateTaskWrapper(void* task);
void DeleteTaskWrapper(void* task);
#define traceTASK_SWITCHED_IN() {pxCurrentTCB->pvThreadLocalStoragePointers[configRUNTIME_THREAD_LOCAL_INDEX + 0] = (void*)GetRunTimeTimer(); }
#define traceTASK_SWITCHED_OUT() {pxCurrentTCB->pvThreadLocalStoragePointers[configRUNTIME_THREAD_LOCAL_INDEX + 1] = (void*)((uint32_t)pxCurrentTCB->pvThreadLocalStoragePointers[configRUNTIME_THREAD_LOCAL_INDEX + 1] + GetRunTimeTimer() - (uint32_t)pxCurrentTCB->pvThreadLocalStoragePointers[configRUNTIME_THREAD_LOCAL_INDEX + 0]); }
#define traceTASK_CREATE(pxNewTCB) {CreateTaskWrapper((void*)pxNewTCB); pxNewTCB->pvThreadLocalStoragePointers[configRUNTIME_THREAD_LOCAL_INDEX + 0] = 0; pxNewTCB->pvThreadLocalStoragePointers[configRUNTIME_THREAD_LOCAL_INDEX + 1] = 0; }
#define traceTASK_DELETE(pxTCB) {DeleteTaskWrapper((void*)pxTCB); }

this must be run before nay task creation (in main.cpp)
new RunTime_c ;

this must be run after scheduler start from any task

RunTime_c::Start();

*/


extern TIM_HandleTypeDef configRUNTIME_TIMER; 



#ifdef __cplusplus
 extern "C" {
#endif
uint32_t GetRunTimeTimer(void)
{
  return (configRUNTIME_TIMER.Instance->CNT);
}


RunTime_c* RunTime_c::ownPtr = nullptr;

void CreateTaskWrapper(void* task_)
{
  TaskHandle_t task = (TaskHandle_t) task_;
  RunTime_c::ownPtr->CreateTask(task);
}

void DeleteTaskWrapper(void* task_)
{
  TaskHandle_t task = (TaskHandle_t) task_;
  RunTime_c::ownPtr->DeleteTask(task);  

}

#ifdef __cplusplus
}
#endif

void RunTimeTimerTick( TimerHandle_t xTimer )
{
  //dispActTimSig.Send();
  RunTime_c::ownPtr->Tick();
}



RunTime_c::RunTime_c(void)
{
  ownPtr = this;
  for(int i=0;i<configRUNTIME_MAX_NO_OF_TASKS;i++)
  {
    taskList[i] = nullptr;
  }
  historyIdx = 0;

  Init();
}

void RunTime_c::Start(void)
{
#if CONF_USE_COMMANDS == 1
  new Com_runtime;
#endif

  TimerHandle_t timer = xTimerCreate("RUNTIME",pdMS_TO_TICKS(1000),pdTRUE,( void * ) 0,RunTimeTimerTick);
  xTimerStart(timer,0);
}

void RunTime_c::Init(void)
{
   HAL_TIM_Base_Start(&configRUNTIME_TIMER);


}

void RunTime_c::CreateTask(TaskHandle_t task)
{
  for(int i=0;i<configRUNTIME_MAX_NO_OF_TASKS;i++)
  {
    if(taskList[i] == nullptr)
    {
      taskList[i] = task;
      actTicksCnt[i] = 0;
      maxLoad[i] = 0;
      age[i] = 0;
      for(int j=0;j<10;j++)
      {
         history[i][j] = 0xFFFF;
      }

      //printf("RUNTIME: seize idx %d\n",i);
      return;
    }
  }
}
void RunTime_c::DeleteTask(TaskHandle_t task)
{
  for(int i=0;i<configRUNTIME_MAX_NO_OF_TASKS;i++)
  {
    if(taskList[i] == task)
    {
      taskList[i] = nullptr;
      //printf("RUNTIME: release idx %d\n",i);
      return;
    }
  }
}

void RunTime_c::Tick( void)
{
  uint32_t totalRuntime;

  TaskHandle_t currentTask = xTaskGetCurrentTaskHandle( );

  vTaskSuspendAll();
  totalRuntime = configRUNTIME_TIMER.Instance->CNT;
  /* current operating task need special treatment */
  uint32_t actTaskRunTime = totalRuntime - (uint32_t)pvTaskGetThreadLocalStoragePointer(currentTask, configRUNTIME_THREAD_LOCAL_INDEX + 0);
  actTaskRunTime += (uint32_t)pvTaskGetThreadLocalStoragePointer(currentTask, configRUNTIME_THREAD_LOCAL_INDEX + 1);
  vTaskSetThreadLocalStoragePointer(currentTask, configRUNTIME_THREAD_LOCAL_INDEX + 1,(void*)actTaskRunTime);
  vTaskSetThreadLocalStoragePointer(currentTask, configRUNTIME_THREAD_LOCAL_INDEX + 0,0);

  for(int i=0;i<configRUNTIME_MAX_NO_OF_TASKS;i++)
  {
    if(taskList[i] != nullptr)
    {
      actTicksCnt[i] = (uint32_t) pvTaskGetThreadLocalStoragePointer(taskList[i], configRUNTIME_THREAD_LOCAL_INDEX + 1);
      vTaskSetThreadLocalStoragePointer(taskList[i], configRUNTIME_THREAD_LOCAL_INDEX + 1,0); /* clear accumulated runtime */
    }
  }

  configRUNTIME_TIMER.Instance->CNT = 0;
  ( void ) xTaskResumeAll();

  if(totalRuntime != 0)
  {  
    for(int i=0;i<configRUNTIME_MAX_NO_OF_TASKS;i++)
    {
      if(actTicksCnt[i] != 0)
      {
        actTicksCnt[i] = (actTicksCnt[i]*1000 )/totalRuntime;   
        
        if( actTicksCnt[i] > maxLoad[i]) { maxLoad[i] = actTicksCnt[i]; }
        
      }
      history[i][historyIdx] = actTicksCnt[i];

      if(age[i] < 999999) { age[i] ++; }
    }
  }
  historyIdx++;
  if(historyIdx >= 10)
  {
    historyIdx = 0;
  }
}

bool RunTime_c::Print(char* strBuf, uint8_t i)
{
  char* strBufTmp = strBuf;


  if(taskList[i] != nullptr)
  {
    uint32_t ticksCnt10 = 0;
    uint8_t  probes = 0;
    for(int j=0;j<10;j++)
    {
      if(history[i][j] != 0xFFFF)
      {
        ticksCnt10 += history[i][j];
        probes++;
      }
    }
    if(probes >1)
    {
      ticksCnt10 = ticksCnt10 / probes;
    }

    sprintf(strBufTmp,"%2d: %-12s ",i,pcTaskGetName(taskList[i]));
    strBufTmp += 17;

    sprintf(strBufTmp,"%04d %% ",actTicksCnt[i]);
    strBufTmp[4] = strBufTmp[3];
    strBufTmp[3] = '.';
    strBufTmp +=7;

    sprintf(strBufTmp,"%04d %% ",ticksCnt10);
    strBufTmp[4] = strBufTmp[3];
    strBufTmp[3] = '.';
    strBufTmp +=7;

    sprintf(strBufTmp,"%04d %% ",maxLoad[i]);
    strBufTmp[4] = strBufTmp[3];
    strBufTmp[3] = '.';
    strBufTmp +=7;

    sprintf(strBufTmp," %6d ",age[i]);
    strBufTmp +=7;

    sprintf(strBufTmp,"\n");
    strBufTmp += 1;

    return true;
  }
  else
  {
    return false;
  }


}


int RunTime_c::GetTaskIdx(TaskHandle_t task)
{
  if(ownPtr != nullptr)
  {
    for(int i=0;i<configRUNTIME_MAX_NO_OF_TASKS;i++)
    {
      if( ownPtr->taskList[i] == task)
      {
        return i;
      }
    }
  }
  return -1;
}


#if CONF_USE_COMMANDS == 1

comResp_et Com_runtime::Handle(CommandData_st* comData)
{
  char* strBuf = new char[64];
  
  Print(comData->commandHandler,"RUNTIME\n");
  Print(comData->commandHandler,"IDX TASK_NAME    1s     10s    max    age[s]\n");

  for(int i=0;i<configRUNTIME_MAX_NO_OF_TASKS;i++)
  {

    bool valid = RunTime_c::ownPtr->Print(strBuf,i);
    if(valid)
    {
      Print(comData->commandHandler,strBuf);
    }
  }  

  delete[] strBuf;

  return COMRESP_OK;
}

#endif

#endif