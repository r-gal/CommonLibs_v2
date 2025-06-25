 #include <stdio.h>
#include <stdlib.h>
#include <cstring>


#include "SignalList.hpp"
#include "GeneralConfig.h"


#include "TimeClass.hpp"


extern RTC_HandleTypeDef hrtc;

SystemTime_st TimeUnit_c::restartTime;

#define DAYS_BEFORE_1970  719527

const uint16_t normalYearDays[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
const uint16_t leapYearDays[]   = {0,31,60,91,121,152,182,213,243,273,305,335,366};

const uint16_t normalYearMonthLength[] = {31 , 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const uint16_t leapYearMonthLength[]   = {31 , 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


const char* weekDayStr[] = {"MON","TUE","WEN","THU","FRI","SAT","SUN","UNN"};

TimeEvent_c* TimeEvent_c::first = nullptr;

RTC_HandleTypeDef* TimeUnit_c::GetHrtc(void)
{
  return &hrtc;
}

bool TimeUnit_c::GetSystemTime(SystemTime_st* time  )
{
  RTC_TimeTypeDef rtcTime;
  RTC_DateTypeDef rtcDate;
  HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN);

  time->Hour = ( uint16_t ) rtcTime.Hours;
  time->Minute = ( uint16_t ) rtcTime.Minutes;
  time->Second = ( uint16_t ) rtcTime.Seconds;
  time->Day = ( uint16_t ) rtcDate.Date;
  time->Month = ( uint16_t ) rtcDate.Month;
  time->Year = ( uint16_t ) rtcDate.Year + 2000;
  time->WeekDay =  ( uint16_t ) rtcDate.WeekDay;
  time->SubSeconds = rtcTime.SubSeconds;
  #ifdef STM32H7
  uint32_t flag = (hrtc.Instance->CR & RTC_CR_BKP);
  #endif
  #ifdef STM32F4
  uint32_t flag = HAL_RTC_DST_ReadStoreOperation(&hrtc);
  #endif
  if(flag == 0)
  {
    time->SummerTimeInd = 0;
  }
  else
  {
    time->SummerTimeInd = 1;
  }   

  return true;
}

bool TimeUnit_c::GetSystemTime(TimeLight_st* time  )
{
  RTC_TimeTypeDef rtcTime;
   HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
  time->hour = ( uint16_t ) rtcTime.Hours;
  time->minute = ( uint16_t ) rtcTime.Minutes;
  time->second = ( uint16_t ) rtcTime.Seconds;
  return true;
}

bool TimeUnit_c::SetTime( SystemTime_st * pxTime )
{
  RTC_TimeTypeDef rtcTime;
  RTC_DateTypeDef rtcDate;

  rtcTime.Hours = pxTime->Hour;
  rtcTime.Minutes = pxTime->Minute;
  rtcTime.Seconds = pxTime->Second;
  rtcTime.SubSeconds = pxTime->SubSeconds;

  rtcDate.Date = pxTime->Day;
  rtcDate.Month = pxTime->Month;
  rtcDate.Year = pxTime->Year - 2000; 
  rtcDate.WeekDay = pxTime->WeekDay ;

  rtcTime.DayLightSaving = 0;
  rtcTime.StoreOperation = 0;

  //HAL_PWR_EnableBkUpAccess();

  HAL_RTC_SetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
  HAL_RTC_SetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN);

  //HAL_PWR_DisableBkUpAccess();

  return true;
}

bool TimeUnit_c::SetSystemTime( SystemTime_st * pxTime )
{
  uint32_t timeSec = MkTime(pxTime);

  uint32_t days = timeSec / (24*60*60);
  uint32_t weekDay = ((days + 3) % 7) + 1; /* 1-1-1970 was thursday */
  pxTime->WeekDay = weekDay ;
  bool summerTime = TimeIsSummer(pxTime,false);

  SetTime(pxTime);

  if(summerTime)
  {
    #ifdef STM32H7
    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);                  
    MODIFY_REG(hrtc.Instance->CR, RTC_CR_BKP , RTC_STOREOPERATION_SET); 
    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);    
    #endif
    #ifdef STM32F4
    HAL_RTC_DST_SetStoreOperation(&hrtc);
    #endif
  }
  else
  {
    #ifdef STM32H7
    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);                  
    MODIFY_REG(hrtc.Instance->CR, RTC_CR_BKP , RTC_STOREOPERATION_RESET); 
    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);    
    #endif
    #ifdef STM32F4
    HAL_RTC_DST_ClearStoreOperation(&hrtc);
    #endif
  }

  return true;
} 


void TimeUnit_c::Init()
{
 /*
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  RTC_AlarmTypeDef sAlarm = {0};

  #ifdef STM32H7
  __HAL_RCC_RTC_CLK_ENABLE();
  #endif
  #ifdef STM32F4
  __HAL_RCC_PWR_CLK_ENABLE();
  #endif
  HAL_PWR_EnableBkUpAccess();
  
  memset(&hrtc,0,sizeof(RTC_HandleTypeDef));
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 128-1;
  hrtc.Init.SynchPrediv = 256-1;
  if (READ_BIT(hrtc.Instance->ISR, RTC_ISR_INITS) == 0U)
  {
    #ifdef STM32F4
    HAL_PWR_EnableBkUpAccess();
    #endif
    HAL_RTC_Init(&hrtc);


    sTime.Hours = 0x0;
    sTime.Minutes = 0x0;
    sTime.Seconds = 0x0;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
    {
      Error_Handler();
    }
    sDate.WeekDay = RTC_WEEKDAY_WEDNESDAY;
    sDate.Month = RTC_MONTH_JANUARY;
    sDate.Date = 0x1;
    sDate.Year = 0x25;

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
    {
      Error_Handler();
    }

    #ifdef STM32F4
    HAL_PWR_DisableBkUpAccess(); 
    #endif
  }

  sAlarm.Alarm = RTC_ALARM_A;
  sAlarm.AlarmMask  =RTC_ALARMMASK_ALL;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_NONE;
  sAlarm.AlarmTime.SubSeconds = 100;

  EXTI_HandleTypeDef hexti;
  hexti.Line = EXTI_LINE_17;

  EXTI_ConfigTypeDef pExtiConfig;
  pExtiConfig.Line = EXTI_LINE_17;
  pExtiConfig.Mode = EXTI_MODE_INTERRUPT;
  pExtiConfig.Trigger = EXTI_TRIGGER_RISING;

  HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 12, 0);
  HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);

  HAL_EXTI_SetConfigLine(&hexti, &pExtiConfig);

  HAL_RTC_SetAlarm_IT(&hrtc,&sAlarm,RTC_FORMAT_BCD);


  */



  GetSystemTime(&restartTime);




}

bool TimeUnit_c::IsYearLeap(uint16_t year)
{
  if((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)))
  {
    return true;
  }
  else
  {
    return false;
  } 
}

uint32_t TimeUnit_c::MkDay(SystemTime_st* timeStruct)
{

  if(timeStruct->Year < 1970) { return 0; }
  /* full years */
  uint32_t year = timeStruct->Year ;

  uint32_t days = (year) * 365 ;
  days += (year-1) / 4;
  days -= (year-1) / 100;
  days += (year-1) / 400;
  days -= DAYS_BEFORE_1970;

  if( IsYearLeap(year) )
  {
    days += leapYearDays[timeStruct->Month -1];
  }
  else
  {
    days += normalYearDays[timeStruct->Month -1];
  }

  days += timeStruct->Day-1;

  return days;
}

uint32_t TimeUnit_c::MkTime(SystemTime_st* timeStruct)
{  

  uint32_t days = MkDay(timeStruct);

  uint32_t result = days * 24;
  result += timeStruct->Hour;
  result *= 60;
  result += timeStruct->Minute;
  result *= 60;
  result += timeStruct->Second;

  return result;
}
void TimeUnit_c::GmTime(SystemTime_st* timeStruct, uint32_t time)
{
  uint32_t days = time / (24*60*60);

  uint32_t rest = time - (days * (24*60*60));

  timeStruct->Hour = rest / (60*60);
  rest = rest - timeStruct->Hour*(60*60);
  timeStruct->Minute = rest / 60;
  rest = rest - timeStruct->Minute*60;
  timeStruct->Second = rest;


  uint32_t dayCounter = 0;
  uint16_t yearCnt = 1970;

  uint16_t leap400cnt = 370;
  uint8_t leap100cnt = 70;
  uint8_t leap4cnt= 2;

  while(1)
  {
     uint16_t actYearDays = 365;

    if(leap4cnt >= 4) {leap4cnt = 0; actYearDays++;}
    if(leap100cnt >= 100) {leap100cnt = 0; actYearDays--;}
    if(leap400cnt >= 400) {leap400cnt = 0; actYearDays++;}

    if(dayCounter + actYearDays > days)
    {
      break;
    }
    else
    {
      dayCounter += actYearDays;
      leap400cnt++;
      leap100cnt++;
      leap4cnt++;
      yearCnt++;
    }
  }

  timeStruct->WeekDay = (days+3) %7 + 1;

  days -= dayCounter;

  timeStruct->Year = yearCnt;

  

  const uint16_t* yearLen;

  if(IsYearLeap(yearCnt))
  {
    yearLen = &leapYearDays[0];
  }
  else
  {
    yearLen = &normalYearDays[0];
  }

  uint8_t monthCnt = 1;

  while(1)
  {
    if(days < yearLen[monthCnt])
    {
      break;
    }
    monthCnt++;
  }

  days -= yearLen[monthCnt-1];

  timeStruct->Month = monthCnt;
  timeStruct->Day = days + 1;




  



}

uint32_t TimeUnit_c::MkSystemTime(void)
{
  SystemTime_st timeStruct;
  GetSystemTime(&timeStruct);
  return MkTime(&timeStruct);
}

void TimeUnit_c::PrintTime(char* strBuf,SystemTime_st* pxTime)
{
  sprintf(strBuf,"TIME: %c %02d-%02d-%04d %s %02d:%02d:%02d.\n",
  pxTime->SummerTimeInd == 0 ? 'W' : 'S',
  pxTime->Day,
  pxTime->Month,
  pxTime->Year,
  weekDayStr[(pxTime->WeekDay -1 < 7) ? pxTime->WeekDay-1 : 7],
  pxTime->Hour,
  pxTime->Minute,
  pxTime->Second);
}

void TimeUnit_c::MkPrecisonUtcTime(uint32_t* sec,uint32_t* subSec)
{

  SystemTime_st timeStruct;
  GetSystemTime(&timeStruct);

  uint32_t time = MkTime(&timeStruct);

  *sec =  time - GetTimeZoneOffset();
  *subSec = (0xFF-timeStruct.SubSeconds)<<24;

}
void TimeUnit_c::SetUtcTime(uint32_t sec,uint32_t subSec)
{
  uint32_t time = sec + 3600; /* use local time and check if summer later */ // GetTimeZoneOffset();
  uint32_t frac = 0xFF - (subSec>>24);

  SystemTime_st timeStruct;
  GmTime(&timeStruct, time);
  timeStruct.SubSeconds = frac;

  bool summerTime = TimeIsSummer(&timeStruct,false);

  SetTime(&timeStruct);

  if(summerTime)
  {
    #ifdef STM32H7
    __HAL_RTC_DAYLIGHT_SAVING_TIME_ADD1H(&hrtc,RTC_STOREOPERATION_SET);
    #endif
    #ifdef STM32F4
    HAL_RTC_DST_SetStoreOperation(&hrtc);
    HAL_RTC_DST_Add1Hour(&hrtc);
    #endif
  }
  else
  {
    #ifdef STM32H7
    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);                  
    MODIFY_REG((&hrtc)->Instance->CR, RTC_CR_BKP , RTC_STOREOPERATION_RESET); 
    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);    
    #endif
    #ifdef STM32F4
    HAL_RTC_DST_ClearStoreOperation(&hrtc);
    #endif
  }

}

void TimeUnit_c::ShiftTime(bool diffSign, uint8_t shiftVal)
{
  if(diffSign==false)
  {
    //hrtc.Instance->SHIFTR = 20;
    HAL_RTCEx_SetSynchroShift(&hrtc,RTC_SHIFTADD1S_RESET,20);
  }
  else
  {
    //hrtc.Instance->SHIFTR = (0xFF-20) | RTC_SHIFTADD1S_SET;
    HAL_RTCEx_SetSynchroShift(&hrtc,RTC_SHIFTADD1S_SET,0xFF-20);    
  }

}

void TimeUnit_c::SummerTimeCheck(SystemTime_st* timeStruct)
{
  #ifdef STM32H7
  uint32_t dstFlag = (&hrtc)->Instance->CR & RTC_CR_BKP;
  #endif
  #ifdef STM32F4
  uint32_t dstFlag = HAL_RTC_DST_ReadStoreOperation(&hrtc);
  #endif
  

  bool wantedDST = TimeIsSummer(timeStruct,timeStruct->SummerTimeInd != 0);
  bool actDST = (dstFlag != 0 );
  
  if(( wantedDST == false) && (actDST == true))
  {
    #ifdef STM32H7
    __HAL_RTC_DAYLIGHT_SAVING_TIME_SUB1H(&hrtc,RTC_STOREOPERATION_RESET);
    #endif
    #ifdef STM32F4
    HAL_RTC_DST_ClearStoreOperation(&hrtc);
    HAL_RTC_DST_Sub1Hour(&hrtc);
    #endif
  }
  else if(( wantedDST == true) && (actDST == false))
  {
    #ifdef STM32H7
    __HAL_RTC_DAYLIGHT_SAVING_TIME_ADD1H(&hrtc,RTC_STOREOPERATION_SET);
    #endif
    #ifdef STM32F4
    HAL_RTC_DST_SetStoreOperation(&hrtc);
    HAL_RTC_DST_Add1Hour(&hrtc);
    #endif
  }
}

int TimeUnit_c::GetTimeZoneOffset(void)
{
  #ifdef STM32H7
  uint32_t dstFlag = (&hrtc)->Instance->CR & RTC_CR_BKP;
  #endif
  #ifdef STM32F4
  uint32_t dstFlag = HAL_RTC_DST_ReadStoreOperation(&hrtc);
  #endif
  if(dstFlag == 0)
  {
    return 3600; 
  }
  else
  {
    return 2*3600; 
  }
}

bool TimeUnit_c::TimeIsSummer(SystemTime_st* pxTime, bool currDDS)
{

  switch(pxTime->Month)
  {
    case 1:
    case 2:
    case 11:
    case 12:
    return false;

    case 3:
    if(pxTime->Day <25) { return false;}
    else if(pxTime->WeekDay == 7)
    {
      if(pxTime->Hour < (currDDS ? 3 : 2)) 
      {
        return false;
      }
      else
      {
        return true;
      }
    }
    else if(pxTime->Day - pxTime->WeekDay > 24)
    {
      return true;
    }
    else
    {
      return false;
    }

    break;

    case 10:
    if(pxTime->Day <25) { return true;}
    else if(pxTime->WeekDay == 7)
    {
      if(pxTime->Hour < (currDDS ? 3 : 2)) 
      {
        return true;
      }
      else
      {
        return false;
      }
    }
    else if(pxTime->Day - pxTime->WeekDay > 24)
    {
      return false;
    }
    else
    {
      return true;
    }

    break;


    default:
    return true;


  }

}

uint8_t TimeUnit_c::GetMonthLength(uint16_t year, uint8_t month)
{
 if(IsYearLeap(year))
 {
   return normalYearMonthLength[month-1];
 }
 else
 {
   return leapYearMonthLength[month-1];
 }

}

TimeEvent_c::TimeEvent_c(void)
{
  next = first;
  first = this;
}


#ifdef __cplusplus
 extern "C" {
#endif

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
  SystemTime_st timeStruct;
  TimeUnit_c::GetSystemTime(&timeStruct);

  if(timeStruct.Second + timeStruct.Minute == 0)
  {
    TimeUnit_c::SummerTimeCheck(&timeStruct);
  }

  TimeEvent_c* tmp = TimeEvent_c::first;

  while(tmp != nullptr)
  {
    tmp->Action(&timeStruct);
    tmp = tmp->next;
  }
  
}



#ifdef __cplusplus
}
#endif




