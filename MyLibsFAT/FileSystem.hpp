
#ifndef FILESYSTEM_C
#define FILESYSTEM_C

#include "FatUnit.hpp"
#include "DirUnit.hpp"
#include "FileUnit.hpp"

class Disk_c
{
  

  uint8_t alignMask;
  
  public:

  Disk_c(void);

  enum AlignMask_et
  {
    ALIGN_8 = 0x00,
    ALIGN_16 = 0x01,
    ALIGN_32 = 0x03,
  };

  FatUnit_c fatUnit;

  char* path;
  FileSystemInterface_c* interface_p;

  bool ReadCluster(uint8_t* clusterBuffer,uint32_t cluster);
  bool ReadSectorInCluster(uint8_t* sectorBuffer,uint32_t cluster, uint8_t sectorInClusterIdx);

  bool WriteCluster(uint8_t* clusterBuffer,uint32_t cluster);

  void SetAlignMask(AlignMask_et mask) {alignMask = mask; }
  uint8_t GetAlignMask(void) { return alignMask; }

};

class FileSystem_c 
{
  Disk_c* disk_p;

  public:

  void MountDisk(Disk_c* disk_p_);
  void UnmountDisk(Disk_c* disk_p_);

  Disk_c* GetDisk(const char* diskPath);
  DirUnit_c* CreateDirUnit(char* absPath);



  static void CleanTask(void);
  static char * GetCwd( char * buffer,size_t bufLen );
  static int ChDir( const char *dirName );

  static bool MkDir( const char *dirName );
  static bool RmDir( const char *dirName );

  static File_c* OpenFile( const char * pcFile, const char * pcMode );
  static bool Remove( const char * fileName);

  static bool Rename( const char * oldName, const char * newName);

};

#endif