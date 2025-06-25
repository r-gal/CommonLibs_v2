
#ifndef CWD_UNIT_C
#define CWD_UNIT_C

#include "FatCommonDef.hpp"

class CwdUnit_c
{

  char currentDirectory[MAX_PATH_LENGTH];


  public:

  CwdUnit_c(void);
  ~CwdUnit_c(void);

  char * GetCwd( char * buffer,size_t bufLen );
  int ChDir( const char *dirName );


  char* GetAbsolutePath(const char* pathIn);

  static CwdUnit_c* GetCwdPtr(void);










};

#endif