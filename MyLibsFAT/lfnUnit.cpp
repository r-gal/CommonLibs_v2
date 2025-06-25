#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lfnUnit.hpp"

const char dosNameSpecialChars[] = {'!', '#', '$', '%', '&', 0x27, '(', ')', '-', '@', '^', '_', '`', '{', '}', '~'};
const uint8_t dosNameSpecialCharsCount = 16;



bool LfnUnit_c::IsCharAllowed(char c)
{
  if(c>='0' && c <='9') { return true; }
  if(c>='A' && c <='Z') { return true; }

  for(int i=0;i<dosNameSpecialCharsCount;i++)
  {
    if(c == dosNameSpecialChars[i])
    {
      return true;
    }
  }
  return false;
}


char LfnUnit_c::ConvertCharToAllowed(char c)
{
  if(IsCharAllowed(c))
  {
    return c;
  }
  else if(c>='a' && c<='z')
  {
    return c - ('a' - 'A');
  }
  else
  {
    return '_';
  }

}

uint8_t LfnUnit_c::GetNoOfWantedEntries(char* fileName)
{
  char* extension = strrchr(fileName,'.');  
  
  uint8_t nameLenTotal = strlen(fileName);
  uint8_t nameLen = nameLenTotal; 
  uint8_t extLen = 0;
  
  if(extension != nullptr)
  {
    extLen = strlen(extension + 1);
    nameLen --; /* dot */
    nameLen -= extLen;
    extension++;
  }

  bool lfnNeeded = false;
  if(extLen>3 || nameLen>8) { lfnNeeded = true; }
  else
  {
    /* check if name nad extension have only DOS allowed characters */
    for(int i=0;(i<8 && i<nameLen);i++)
    {
      if(IsCharAllowed(fileName[i]) == false)
      {
        lfnNeeded = true;
        break;
      }
    }
    if(lfnNeeded == false)
    {
      for(int i=0;(i<3 && i<extLen);i++)
      {
        if(IsCharAllowed(extension[i]) == false)
        {
          lfnNeeded = true;
          break;
        }
      }
    }  
  } 

  if(lfnNeeded == false)
  {
    return 1;
  }
  else if(nameLenTotal%13 == 0)
  {
     return (nameLenTotal/13) + 1;
  }
  else
  {
    return (nameLenTotal/13) + 2;
  }
}

char* LfnUnit_c::GenerateDosName(char* fileName, bool validDosName)
{
  char* extension = strrchr(fileName,'.');
  
  
  uint8_t nameLenTotal = strlen(fileName);
  uint8_t nameLen = nameLenTotal; 
  uint8_t extLen = 0;
  
  if(extension != nullptr)
  {
    extLen = strlen(extension + 1);
    nameLen --; /* dot */
    nameLen -= extLen;
    extension++;
  }

  for(int i=0;i<8;i++)
  {
    if(i<nameLen)
    {
      dosName[i] = ConvertCharToAllowed(fileName[i]);
    }
    else
    {
      dosName[i] = ' ';
    }
  }

  for(int i=0;i<3;i++)
  {
    if(i<extLen)
    {
      dosName[8+i] = ConvertCharToAllowed(extension[i]);
    }
    else
    {
      dosName[8+i] = ' ';
    }
  }

  if(validDosName == false)
  {

    char numStr[8];
    sprintf(numStr,"~%d",dosNameNumber);

    uint8_t numStrLen = strlen(numStr);

    for(int i=0;i<numStrLen;i++)
    {
      dosName[i+ 8-numStrLen ] = numStr[i];
    }

    dosNameNumber++;
  }
  return dosName;
}