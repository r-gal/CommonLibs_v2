#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"



#include "GeneralConfig.h"
#include "LoggingUnit.hpp"

#if CONF_USE_LOGGING == 1
#include "FileSystem.hpp"
#include "TimeClass.hpp"
#endif


void LoggingUnit_c::Log(LOG_LEVEL_et level,char* format,  ... )
{
  #if CONF_USE_LOGGING == 1

  va_list args;
  va_start(args, format);
  /*04-06-2025 13:24:56 Lv0 */ /* 24 chars left */

  logSig_c* sig_p = new logSig_c;

  TimeUnit_c::GetSystemTime(&sig_p->time);
  
  snprintf(sig_p->text+24,LOG_TEXT_LENGTH-24,format,args);
  va_end(args);

  sig_p->level = level;
  sig_p->Send();

  #endif
}



#if CONF_USE_LOGGING == 1


void LoggingUnit_c::HandleLog(logSig_c* sig_p)
{

  char fileName[32] =  "LOGS/LOG.TXT";
  File_c* currentFile = FileSystem_c::OpenFile(fileName, "a" );

  char* text;
  int size;
  text = sig_p->text;


  sprintf(text,"%02d-%02d-%04d %02d:%02d:%02d Lv%d",
  sig_p->time.Day,
  sig_p->time.Month,
  sig_p->time.Year,
  sig_p->time.Hour,
  sig_p->time.Minute,
  sig_p->time.Second,
  sig_p->level);

  text[23] = ' '; /* overwrite end of line to split strings */

  size = strlen(text);
   
  if(size > 0)
  {
    if(currentFile != nullptr)
    {
      int fileSize = currentFile->GetSize();
      currentFile->Write((uint8_t*)text,size);
      currentFile->Close();

      if(fileSize > 1024*128)
      {
        char newFileName[32];
        int idx = 1;
        bool ok = false;

        while (idx< 1000)
        {
          sprintf(newFileName,"LOGS/LOG_%d.TXT",idx);
          File_c* checkFile = FileSystem_c::OpenFile(newFileName, "r" );
          if(checkFile != nullptr)
          {
            checkFile->Close();
          }
          else
          {
            ok = true;
            break;
          }
          idx++;
        }

        if(ok)
        {
          FileSystem_c::Rename(fileName,newFileName);
        }   
      }      
    }
  }
}

void LoggingUnit_c::Init(void)
{


}

#endif
