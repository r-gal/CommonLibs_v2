

#ifndef RUNTIME_CLASS_C
#define RUNTIME_CLASS_C

#include "SignalList.hpp"
#include "GeneralConfig.h"
#include "FreeRTOS.h"
#include "task.h"

#if CONF_USE_COMMANDS == 1
#include "commandHandler.hpp"
#endif

class RunTime_c
{
  TaskHandle_t taskList[configRUNTIME_MAX_NO_OF_TASKS];
  uint32_t actTicksCnt[configRUNTIME_MAX_NO_OF_TASKS];
  uint32_t maxLoad[configRUNTIME_MAX_NO_OF_TASKS];
  uint32_t age[configRUNTIME_MAX_NO_OF_TASKS];

  uint16_t history[configRUNTIME_MAX_NO_OF_TASKS][10];
  uint8_t historyIdx;

  TIM_HandleTypeDef        htim;
  
  public:

  static RunTime_c* ownPtr;
  RunTime_c(void);

  void Init(void);


  static void Start(void);

  void CreateTask(TaskHandle_t task);
  void DeleteTask(TaskHandle_t task);

  void Tick( void);

  bool Print(char* buffer,uint8_t idx);

};

#if CONF_USE_COMMANDS == 1

class Com_runtime : public Command_c
{
  public:
  char* GetComString(void) { return (char*)"runtime"; }
  void PrintHelp(CommandHandler_c* commandHandler ){}
  comResp_et Handle(CommandData_st* comData_);
};

#endif

#endif