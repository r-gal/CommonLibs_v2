#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "FatUnit.hpp"
#include "FileSystem.hpp"
#include "CwdUnit.hpp"
#include "GeneralConfig.h"

FileSystem_c fileSystem;

void FileSystem_c::MountDisk(Disk_c* disk_p_)
{
  disk_p = disk_p_;
  disk_p->fatUnit.Init(disk_p->interface_p);
}
void FileSystem_c::UnmountDisk(Disk_c* disk_p_)
{
  disk_p = nullptr;
}

Disk_c* FileSystem_c::GetDisk(const char* diskPath)
{
  return disk_p;
}

Disk_c::Disk_c(void)
{
  alignMask = ALIGN_32;
}

bool Disk_c::ReadCluster(uint8_t* clusterBuffer,uint32_t cluster)
{
  uint32_t sector = fatUnit.ClusterToSector(cluster);
  interface_p->Read(clusterBuffer,sector,fatUnit.GetSectorsPerCluster());

  return true;

}

bool Disk_c::WriteCluster(uint8_t* clusterBuffer,uint32_t cluster)
{
  uint32_t sector = fatUnit.ClusterToSector(cluster);
  interface_p->Write(clusterBuffer,sector,fatUnit.GetSectorsPerCluster());

  return true;

}

bool Disk_c::ReadSectorInCluster(uint8_t* sectorBuffer,uint32_t cluster, uint8_t sectorInClusterIdx)
{
  uint32_t sector = fatUnit.ClusterToSector(cluster);
  interface_p->Read(sectorBuffer,sector+sectorInClusterIdx,1);

  return true;

}

DirUnit_c* FileSystem_c::CreateDirUnit(char* absPath)
{
  DirUnit_c* dirUnit = nullptr;
  Disk_c* disk_p = GetDisk(absPath);
  if(disk_p != nullptr)
  {
    dirUnit = new DirUnit_c(disk_p);
  }
  return dirUnit;
}

void FileSystem_c::CleanTask(void)
{
  CwdUnit_c* cwd_p = (CwdUnit_c*) pvTaskGetThreadLocalStoragePointer(NULL,TASK_LOCAL_STORAGE_CWD_IDX);

  if(cwd_p != nullptr)
  {
     delete cwd_p;
  }
}





char * FileSystem_c::GetCwd( char * buffer,size_t bufLen )
{
  CwdUnit_c* cwd_p = CwdUnit_c::GetCwdPtr();
  return cwd_p->GetCwd(buffer,bufLen);
}
int FileSystem_c::ChDir( const char *dirName )
{
  CwdUnit_c* cwd_p = CwdUnit_c::GetCwdPtr();
  return cwd_p->ChDir(dirName);
}

bool FileSystem_c::MkDir( const char *dirName )
{
  bool res = false;
  CwdUnit_c* cwd_p = CwdUnit_c::GetCwdPtr();
  char* absPath = cwd_p->GetAbsolutePath(dirName);

  DirUnit_c* dirUnit = fileSystem.CreateDirUnit(absPath);
  if(dirUnit != nullptr)
  {
    res = dirUnit->MkDir(absPath);
    delete dirUnit;
  }
  delete[] absPath;
  return res;  
}
bool FileSystem_c::RmDir( const char *dirName )
{
  bool res = false;
  CwdUnit_c* cwd_p = CwdUnit_c::GetCwdPtr();
  char* absPath = cwd_p->GetAbsolutePath(dirName);

  DirUnit_c* dirUnit = fileSystem.CreateDirUnit(absPath);
  if(dirUnit != nullptr)
  {
    res = dirUnit->RmDir(absPath);
    delete dirUnit;
  }
  delete[] absPath;
  return res;  
}

File_c* FileSystem_c::OpenFile( const char * file, const char * mode )
{
  File_c* fileHandler = new File_c;
  bool openOk = fileHandler->Open(file,mode);
  if(openOk)
  {
    return fileHandler;
  }
  else
  {
    delete fileHandler;
    return nullptr;
  }
}

bool FileSystem_c::FileExists(const char * pcFile)
{
  printf("Not impelemented! \n");
  return false;

}

bool FileSystem_c::Remove( const char * fileName)
{
  bool res = false;
  CwdUnit_c* cwd_p = CwdUnit_c::GetCwdPtr();
  char* absPath = cwd_p->GetAbsolutePath(fileName);

  DirUnit_c* dirUnit = fileSystem.CreateDirUnit(absPath);
  if(dirUnit != nullptr)
  {
    res = dirUnit->RemoveFile(absPath);
    delete dirUnit;
  }
  delete[] absPath;
  return res;  

}

bool FileSystem_c::Rename( const char * oldName, const char * newName)
{
  bool res = false;
  CwdUnit_c* cwd_p = CwdUnit_c::GetCwdPtr();
  char* absPathOld = cwd_p->GetAbsolutePath(oldName);
  char* absPathNew = cwd_p->GetAbsolutePath(newName);

  DirUnit_c* dirUnit = fileSystem.CreateDirUnit(absPathOld);
  if(dirUnit != nullptr)
  {
    res = dirUnit->Rename(absPathOld,absPathNew);
    delete dirUnit;
  }

  delete[] absPathOld;
  delete[] absPathNew;
  return res;
}