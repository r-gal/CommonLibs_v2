#ifndef LOGGING_H
#define LOGGING_H

#include "SignalList.hpp"

enum LOG_LEVEL_et
{
  LOG_FATAL_ERROR,
  LOG_ERROR,
  LOG_TIME_EVENT,
  LOG_DEVICE_EVENT,
  LOG_USER_EVENT


};



class LoggingUnit_c
{




  public:

  #if CONF_USE_LOGGING == 1

  void Init(void);

  void HandleLog(logSig_c* sig_p);


  #endif

  static void Log(LOG_LEVEL_et level,char* text, ...);




};




#endif




