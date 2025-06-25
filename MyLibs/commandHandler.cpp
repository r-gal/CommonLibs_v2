#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#if CONF_USE_COMMANDS == 1


/*#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rtc.h"
#include "stm32f4xx_pwr.h"*/
//#include "misc.h"

//#include "common.hpp"
//#include "database.hpp"
//#include "device_class.hpp"
//#include "FaultHandler.hpp"



#include "commandHandler.hpp"

  #if COMMAND_USE_TELNET == 1
    #include "TELNET_ServerProcess.hpp"

  #endif

  #if COMMAND_USE_UART == 1
    #include "UartTerminal.hpp"
  #endif

Command_c* Command_c::first;// = nullptr;

const char* errorCodeString[] = 
{
  "",
  "Format error",
  "Not number",
  "Too large",
  "Too small"
};

//RTC_TimeTypeDef command_c::restartTime;
//RTC_DateTypeDef command_c::restartDate;
//uint8_t command_c::restartCause;

bool CommandHandler_c::CommandSplit(void)
{
  argsCnt = 0;
  uint8_t phase = 0; /* 0 - fetch command, 1 - fetch argStr, 2 - fetch argVal */

  int length;

  char* endptr = strchr(buffer,COMMAND_END_CHAR);
  

  if(endptr != nullptr) 
  {
    length = (endptr - buffer) + 1;
  }
  else
  {
    return false;
  }


  int prevSeparatorPos = 0;

  for(int i=0;i<length;i++)
  {
    if(buffer[i] == COMMAND_SEPARATOR_CHAR)
    {
      if(phase == 0 && i>0)
      {
        /*command name OK */
        phase = 1;
        buffer[i] = 0;
        prevSeparatorPos = i;
        argStringPos[argsCnt] = i+1;        
      }
      else
      {
        return false;
      }
    }
    else if(buffer[i] == ARG_EQUAL_CHAR )
    {
      if(phase == 1 && (i-prevSeparatorPos > 1))
      {
        if(argsCnt >= MAX_NO_OF_ARGUMENTS) { return false; }
        /*argument name OK */
        phase = 2;
        buffer[i] = 0;
        prevSeparatorPos = i;
        argValPos[argsCnt] = i+1;
        argsCnt++;
      }
      else
      {
        return false;
      }

    }
    else if(buffer[i] == ARGUMENTS_SEPARATOR_CHAR )
    {
      if(phase == 2 && (i-prevSeparatorPos > 1))
      {
        /*argument name OK */
        phase = 1;
        buffer[i] = 0;
        prevSeparatorPos = i;
        argStringPos[argsCnt] = i+1;

      }
      else
      {
        return false;
      }
    }
    else if(buffer[i] == COMMAND_END_CHAR )
    {
      if(phase == 2 && (i-prevSeparatorPos > 1))
      {
        /*argument value OK */
        phase = 1;
        buffer[i] = 0;
        prevSeparatorPos = i;
        return true;      
      }
      else if(phase == 0)
      {
        buffer[i] = 0;
        return true;
      }
      else
      {
        return false;
      }

    }
  }
  return false;
}



Command_c* CommandHandler_c::FetchCommandName(char* buf)
{
  Command_c* command = Command_c::GetFirst();
  while(command != nullptr)
  {
    if( strcmp(buf,command->GetComString())== 0)
    {
      return command;
    }
    command = command->GetNext();
  }
  return nullptr;
}



void CommandHandler_c::ParseCommand(void)
{
  Command_c* command = nullptr; 
  comResp_et resp = COMRESP_UNKNOWN;

  printBuffer = nullptr;
  printBufferIdx = 0;

  if(CommandSplit() == false)
  {
    resp = COMRESP_FORMAT;
  }
  else
  {
    if(buffer[0] == '?')
    {
      if(size == 1)
      {
        PrintCommandsList();
        resp = COMRESP_OK;
      }
      else
      {
        command = FetchCommandName(buffer+1);
        if(command !=nullptr)
        {
          printBuffer = new char[PRINT_BUFFER_SIZE];
          command->PrintHelp(this);
          resp = COMRESP_OK;
        }
        else
        {
          resp = COMRESP_UNKNOWN;
        }
      }


    }
    else
    {
      command = FetchCommandName(buffer);
      if(command != nullptr)
      {
        printBuffer = new char[PRINT_BUFFER_SIZE];
        CommandData_st commandData;
        commandData.buffer = buffer;
        commandData.argsCnt = argsCnt;
        commandData.argStringPos = argStringPos;
        commandData.argValPos = argValPos;
        commandData.commandHandler = this;
        resp = command -> Handle(&commandData);
      }
      else
      {
        resp = COMRESP_UNKNOWN;
      }
    }
  }

  if(printBuffer == nullptr)
  {
    printBuffer = new char[32];
  }

  switch(resp)
  {
    case COMRESP_OK:
      PrintBuf("Accepted\n");
      break;
    case COMRESP_FORMAT:
      PrintBuf("Format error\n");
      break;
    case COMRESP_UNKNOWN:
      PrintBuf("Unknown command\n");
      break;
    case COMRESP_VALUE:
      PrintBuf("Illegal value\n");
      break;
    case COMRESP_NOPARAM:
      PrintBuf("Missing parameter\n");
      break;
    case COMRESP_FAILED:
      PrintBuf("Failed\n");
      break;
    default:
      PrintBuf("Unknown error\n");
      break;
  }

  if(printBuffer != nullptr)
  {
    SendBuffer(printBuffer);
    //delete printBuffer; 
  }


}

void CommandHandler_c::PrintCommandsList(void)
{



}

void CommandHandler_c::PrintBuf(char* buffer)
{
  PrintBuf((const char*) buffer);
}
void CommandHandler_c::PrintBuf(const char* buffer)
{
  uint16_t len = strlen(buffer);
  if(printBufferIdx + len >= PRINT_BUFFER_SIZE)
  {
    SendBuffer(printBuffer);
    printBuffer = new char[PRINT_BUFFER_SIZE];
    printBufferIdx = 0;
  }
  strcpy(printBuffer+printBufferIdx,buffer);
  printBufferIdx+= len;

}

void CommandHandler_c::SendBuffer(char* buffer)
{


  #if COMMAND_USE_UART == 1
    if(uartHandler != nullptr)
    {
      uartHandler->SendBuffer((uint8_t*)buffer, strlen(buffer));
    }
    
  #endif

  #if COMMAND_USE_TELNET == 1
    if(telnetHandler != nullptr)
    {
      telnetHandler->Send((uint8_t*)buffer, strlen(buffer));
    }
  #else
    /* tcp stack will release buffer after sending*/
    delete[] buffer;
  #endif




}

void CommandHandler_c::SendToAll(char* buffer)
{
 #if COMMAND_USE_TELNET == 1
    TelnetProcess_c::SendToAllClients((uint8_t*)buffer, strlen(buffer));    
  #endif

  #if COMMAND_USE_UART == 1
    UartTerminalProcess_c::SendToAll((uint8_t*)buffer, strlen(buffer));    
  #endif
}

int Command_c::FetchParameterString(CommandData_st* comData,const char* parString)
{

  for(int i =0;i< comData->argsCnt; i++)
  {
    if(strcmp(& comData->buffer[comData->argStringPos[i]],parString) == 0)
    {
      printf("Found [%s] at idx %d\n",parString,i);
      return i;
    }
  }
  return -1;

}

bool Command_c::FetchParameterValue(CommandData_st* comData,const char* parString, uint32_t* value, uint32_t min, uint32_t max)
{
  int argIdx = FetchParameterString(comData,parString);

  if(argIdx == -1) { return false; }

  char* valueStr = &comData->buffer[comData->argValPos[argIdx]];

  uint8_t errorCode = 0;

  bool result = true;


  uint8_t parLength = strlen(valueStr);
  if(parLength == 0)
  { 
    errorCode = 1;
    result = false;
  }
  else if(( parLength > 2) && ( (strncmp(valueStr,"0x",2) == 0) || (strncmp(valueStr,"0X",2) == 0)))
  {
    /*fetch hex */  
    valueStr +=2;
    parLength -=2;
    *value = 0;
    for(int i=0; i<parLength; i++)
    {
      char c = *valueStr;
      uint8_t cVal;

      if(( c >= '0') && ( c<= '9'))
      {
        cVal = c - '0';
        *value = (*value)*16 + cVal;
      }
      else if(( c >= 'A') && ( c<= 'F'))
      {
        cVal = (c - 'A') + 10;
        *value = (*value)*16 + cVal;
      }
      else if(( c >= 'a') && ( c<= 'f'))
      {
        cVal = (c - 'a') + 10;
        *value = (*value)*16 + cVal;
      }
      else
      {
        errorCode = 2;
        result = false;
        break;
      }
      valueStr++;

    }

  }
  else
  {
    /* fetch dec */
    *value = 0;
    for(int i=0; i<parLength; i++)
    {
      char c = *valueStr;
      uint8_t cVal;
      if(( c >= '0') && ( c<= '9'))
      {
        cVal = c - '0';
        *value = (*value)*10 + cVal;
      }
      else
      {
        errorCode = 2;
        result = false;
        break;
      }
      valueStr++;

    }
  }


  if(result)
  {
    if(*value > max) 
    {
      result = false;
      errorCode = 3;
    }
    if(*value < min)
    {
      result = false;
      errorCode = 4;
    }
  }
  if(errorCode > 0)
  {
    char strBuf[64];  
    sprintf(strBuf,"ERROR: parameter %s %s\n",parString,errorCodeString[errorCode]);
    Print(comData->commandHandler,strBuf);

  }
  printf("Fetched value %d\n",result);

  return result;
}

bool  Command_c::FetchParameterTime(CommandData_st* comData,const char* parString, uint32_t* hour, uint32_t* min, uint32_t* sec)
{
  int argIdx = FetchParameterString(comData,parString);

  if(argIdx == -1) { return false; }

  char* valueStr = &comData->buffer[comData->argValPos[argIdx]];

  uint8_t errorCode = 0;

  bool result = true;


  uint8_t parLength = strlen(valueStr);
  if((parLength != 5) && (parLength != 8))
  { 
    errorCode = 1;
    result = false;
  }
  else
  {
    for(int i=parLength-1;(i>=0) && result;i--)
    {
      switch(i)
      {
        case 2:
        case 5:
        {
          if(valueStr[i] != ':')
          {
            errorCode = 1;
            result = false;
          }
          break;
        }
        default:
        {
          if(valueStr[i] < '0' || valueStr[i] > '9')
          {
            errorCode = 1;
            result = false;
          }
          break;
        }
      }
    }
    if(result)
    {
      for(int i=parLength-1;(i>=0) && result;i--)
      {
        switch(i)
        {
          case 7:
            *sec = valueStr[i]-'0'; break;
          case 6:
            *sec += (valueStr[i]-'0')*10; break;
          case 4:
            *min = valueStr[i]-'0'; break;
          case 3:
            *min += (valueStr[i]-'0')*10; break;
          case 1:
            *hour = valueStr[i]-'0'; break;
          case 0:
            *hour += (valueStr[i]-'0')*10; break;
          default:
            break;                          
        }
      }
      if(((*hour == 24) && (*min >0)) || (*hour > 24)) { errorCode = 1; result = false; }
      if(*min > 59) { errorCode = 1; result = false; }
      if((parLength ==8) && (*sec > 59)) { errorCode = 1; result = false; }
    }
  }

  if(errorCode > 0)
  {
    char strBuf[64];  
    sprintf(strBuf,"ERROR: parameter %s %s\n",parString,errorCodeString[errorCode]);
    Print(comData->commandHandler,strBuf);

  }

  return result;
}

bool  Command_c::FetchParameterIp(CommandData_st* comData,const char* parString, uint32_t* ip)
{
  int argIdx = FetchParameterString(comData,parString);

  if(argIdx == -1) { return false; }

  char* valueStr = &comData->buffer[comData->argValPos[argIdx]];


  uint8_t ipDigitIdx = 0;
  uint8_t parLength = strlen(valueStr);
  if(parLength > 15) { return false; }
  *ip = 0;

  uint16_t tmpVal = 0;
  for(int i =0;i<parLength;i++)
  {
    if(valueStr[i] == '.')
    {
      if(ipDigitIdx>3) { return false; }

      *ip |= (tmpVal&0xFF) << 8*(3-ipDigitIdx);

      tmpVal = 0;
      ipDigitIdx++;

    }
    else if((valueStr[i] >= '0') && (valueStr[i] <= '9'))
    {
      tmpVal *= 10;
      tmpVal += valueStr[i]-'0';
      if(tmpVal> 255) { return false; }
    }
    else
    {
      return false;
    }

  }
  if(ipDigitIdx != 3) { return false;}
  *ip |= (tmpVal&0xFF) ;

  return true;
}

void Command_c::Print(CommandHandler_c* commandHandler,char* buffer)
{
  commandHandler->PrintBuf(buffer);
}
void Command_c::Print(CommandHandler_c* commandHandler,const char* buffer)
{
  commandHandler->PrintBuf(buffer);
}

Command_c::Command_c(void)
{
  if(first != nullptr) 
  {
    next = first;      
  }
  else
  {
    next = nullptr;
  }
  first = this;
}

#endif


