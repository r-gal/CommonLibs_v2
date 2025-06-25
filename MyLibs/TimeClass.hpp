

#ifndef TIME_CLASS_C
#define TIME_CLASS_C

#include "GeneralConfig.h"

struct SystemTime_st
{
    uint16_t Year;   /* Year	(e.g. 2009). */
    uint8_t Month;  /* Month	(e.g. 1 = Jan, 12 = Dec). */
    uint8_t Day;    /* Day	(1 - 31). */
    uint8_t Hour;   /* Hour	(0 - 23). */
    uint8_t Minute; /* Min	(0 - 59). */
    uint8_t Second; /* Second	(0 - 59). */
    uint8_t WeekDay;/* WeekDay (1 - 7).  */
    uint8_t SubSeconds; /* sub seconds, 0-255 */
    uint8_t SummerTimeInd; /* 0-1  */
};

struct TimeLight_st
{
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

class TimeEvent_c
{

  public:

  static TimeEvent_c* first;
  TimeEvent_c* next;

  TimeEvent_c(void);

  virtual void Action(SystemTime_st* time) = 0;

};

class TimeUnit_c
{
  

  static bool IsYearLeap(uint16_t year);

  static SystemTime_st restartTime;

  static bool SetTime( SystemTime_st * pxTime );
  

  public:

  static RTC_HandleTypeDef* GetHrtc(void);

  void Init(void);

  static bool GetSystemTime(SystemTime_st* time  );
  static bool SetSystemTime( SystemTime_st * time );

  static bool GetSystemTime(TimeLight_st* time  );

  static uint32_t MkTime(SystemTime_st* timeStruct);
  static uint32_t MkDay(SystemTime_st* timeStruct);
  static void GmTime(SystemTime_st* timeStruct, uint32_t time);
  static uint32_t MkSystemTime(void);
  //static uint32_t GetRandomVal(void);
  static void MkPrecisonUtcTime(uint32_t* sec,uint32_t* subSec);
  static void SetUtcTime(uint32_t sec,uint32_t subSec);
  static void ShiftTime(bool diffSign, uint8_t shiftVal);

  static void PrintTime(char* strBuf, SystemTime_st* timeStruct);

  static SystemTime_st* GetRestartTime(void  ) { return &restartTime; }

  static int GetTimeZoneOffset(void);

  static bool TimeIsSummer(SystemTime_st* pxTime,bool currDDS);

  static void SummerTimeCheck(SystemTime_st* timeStruct);

  static uint8_t GetMonthLength(uint16_t year, uint8_t month);


};

#endif