#ifndef COMMAND_H
#define COMMAND_H

#include "GeneralConfig.h"
#include "CommandConfig.hpp"
#include "SignalList.hpp"
//#include "stm32f4xx_rtc.h"
#if COMMAND_USE_TELNET == 1

#endif
class TelnetClientProcess_c;
class UartTerminalProcess_c;
#define PRINT_BUFFER_SIZE 1024

class Command_c;

enum comResp_et
{
  COMRESP_OK,
  COMRESP_FORMAT,
  COMRESP_VALUE,
  COMRESP_NOPARAM,
  COMRESP_FAILED,
  COMRESP_UNKNOWN

};

class CommandHandler_c : public SignalLayer_c
{
  //static RTC_TimeTypeDef restartTime;
  //static RTC_DateTypeDef restartDate;
  //static uint8_t restartCause;



  enum paramFetchResult_et
  {
    PARFETCH_NOTFOUND,
    PARFETCH_FOUND,
    PARFETCH_FORMAT,
    PARFETCH_NOTNUMBER


  };

  char* buffer;
  uint16_t size;

  uint16_t argStringPos[MAX_NO_OF_ARGUMENTS];
  uint16_t argValPos[MAX_NO_OF_ARGUMENTS];
  uint8_t argsCnt;

  char* printBuffer;
  uint16_t printBufferIdx;

  bool CommandSplit(void); 


  char paramString[MAX_ARGUMENT_SIZE] ;

  Command_c* FetchCommandName(char* buf);



  



  void PrintCommandsList(void);

  public:

  #if COMMAND_USE_TELNET == 1
  TelnetClientProcess_c* telnetHandler;
  #endif
  #if COMMAND_USE_UART == 1
  UartTerminalProcess_c* uartHandler;
  #endif
  

  void PrintBuf(char* buffer);
  void PrintBuf(const char* buffer);

  static void SendToAll(char* buffer);

  void SendBuffer(char* buffer);

  void ParseCommand(void);

  CommandHandler_c(char* buffer_, uint16_t size_) : buffer(buffer_), size(size_)
  {
    #if COMMAND_USE_TELNET == 1
    telnetHandler = nullptr;
    #endif
    #if COMMAND_USE_UART == 1
    uartHandler = nullptr;
    #endif
  }

  //static const 
  //static void SetRestartRime(void);


};

struct CommandData_st
{
  char* buffer;
  uint8_t argsCnt;
  uint16_t* argStringPos;
  uint16_t* argValPos;
  CommandHandler_c* commandHandler;
};

class Command_c
{
  static Command_c* first;
  Command_c* next;  

  protected:

  bool FetchParameterValue(CommandData_st* comData,const char* parString, uint32_t* value, uint32_t min, uint32_t max);
  int FetchParameterString(CommandData_st* comData,const char* parString);
  bool FetchParameterTime(CommandData_st* comData,const char* parString, uint32_t* hour, uint32_t* min, uint32_t* sec);
  bool FetchParameterIp(CommandData_st* comData,const char* parString, uint32_t* ip);

  void Print(CommandHandler_c* commandHandler,char* buffer);
  void Print(CommandHandler_c* commandHandler,const char* buffer);



  public:

  Command_c(void);


  Command_c* GetNext(void) { return next; }
  static Command_c* GetFirst(void) { return first; }

  virtual char* GetComString(void) = 0;
  virtual void PrintHelp(CommandHandler_c* commandHandler ) = 0;
  virtual comResp_et Handle(CommandData_st* comData_) = 0;


};

class CommandGroup_c
{



};




#endif