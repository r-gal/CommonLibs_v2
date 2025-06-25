#ifndef LFN_UNIT_C
#define LFN_UNIT_C

#include "FatCommonDef.hpp"


class LfnUnit_c
{
  uint8_t dosNameNumber;

  char dosName[16];

  bool IsCharAllowed(char c);
  char ConvertCharToAllowed(char c);

  public:

  LfnUnit_c(void) {dosNameNumber = 1;}


  uint8_t GetNoOfWantedEntries(char* fileName);

  char* GenerateDosName(char* fileName, bool validDosName);
  char* GetDosName(void) { return dosName; }


};


#endif