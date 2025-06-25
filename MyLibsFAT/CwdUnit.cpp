
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "DirUnit.hpp"
#include "FileSystem.hpp"
#include "CwdUnit.hpp"


extern FileSystem_c fileSystem;

CwdUnit_c::CwdUnit_c(void)
{

  strcpy(currentDirectory,"/");

}
CwdUnit_c::~CwdUnit_c(void)
{
  
}

CwdUnit_c* CwdUnit_c::GetCwdPtr(void)
{
  CwdUnit_c* cwd_p = (CwdUnit_c*) pvTaskGetThreadLocalStoragePointer(NULL,TASK_LOCAL_STORAGE_CWD_IDX);
  if(cwd_p == nullptr) {cwd_p = new CwdUnit_c; vTaskSetThreadLocalStoragePointer(NULL,TASK_LOCAL_STORAGE_CWD_IDX,(void*)cwd_p);}
  return cwd_p;

}
 

char* CwdUnit_c::GetAbsolutePath(const char* pathIn)
{
  char* newPath = new char[MAX_PATH_LENGTH];

  int curPathLen = strlen(currentDirectory);
  int pathInLen = strlen(pathIn);

  bool resOk = true;

  if(pathIn[0] == '/')
  {
    /*new path is absolute */
    if(pathInLen < MAX_PATH_LENGTH)
    {
      strcpy(newPath,pathIn);
    }
    else
    {
      resOk = false;
    }    
  }
  else
  {
    /*new path is relative */
    if(pathInLen + curPathLen +1 < MAX_PATH_LENGTH)
    {
      strcpy(newPath,currentDirectory);
      if(curPathLen > 1)
      {
        strcat(newPath,"/");  /* not add / if currend directory is root */
      }
      strcat(newPath,pathIn);
    }
    else
    {
      resOk = false;
    }
  }

  if(resOk)
  {
    int newPathLen = strlen(newPath);
    if(newPathLen > 1 && newPath[newPathLen-1] == '/')
    {
      newPath[newPathLen-1] = 0;
      newPathLen--;
    }
  
  /* resolve dotdot directories */
    do
    {
      char* ptr1 = strstr(newPath,"/..");
      if(ptr1 == nullptr)
      {
        break;
      }
      else
      {
        /*found /.. dir, search prev dir to reduce */

        if(ptr1 <= newPath)
        {
          resOk = false;
          break;
        }

        char* scanPtr= ptr1-1;
        while(*scanPtr != '/')
        {
          scanPtr--;
        }
        ptr1 += 3;

        strcpy(scanPtr,ptr1);
      } 
    }
    while(1);

    if(newPath[0] == 0)
    {
      strcpy(newPath,"/");
    }
  }

  
  return newPath;
}

char * CwdUnit_c::GetCwd( char * buffer,size_t bufLen )
{
  if(strlen(currentDirectory) < bufLen)
  {
    strcpy(buffer,currentDirectory);
    return buffer;
  }
  else
  {
    return nullptr;
  }

}
int CwdUnit_c::ChDir( const char *dirName )
{
  int result = 0;
  char* newPath = GetAbsolutePath(dirName);

  //printf("RUN ChDir {%s} -> {%s}\n",dirName,newPath);

  Disk_c* disk_p = fileSystem.GetDisk(newPath);
  if(disk_p == nullptr)
  {
    result = -1;
  }
  else
  {
    DirUnit_c* dirUnit = new DirUnit_c(disk_p);

    uint32_t cluster = dirUnit->GetDirStartCluster(newPath,false);

    if(cluster == CLUSTER_NOT_VALID)
    {
      result = -1;
    }
    else
    {
      strcpy(currentDirectory,newPath);
    }
    delete dirUnit;
  }
  delete[] newPath; 

  return result;
}