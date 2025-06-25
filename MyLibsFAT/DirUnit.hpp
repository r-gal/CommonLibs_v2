
#ifndef DIRUNIT_C
#define DIRUNIT_C

#include "FatCommonDef.hpp"

#include "TimeClass.hpp"

/* Dirent Attributes. */
#define FF_FAT_ATTR_READONLY          0x01
#define FF_FAT_ATTR_HIDDEN            0x02
#define FF_FAT_ATTR_SYSTEM            0x04
#define FF_FAT_ATTR_VOLID             0x08
#define FF_FAT_ATTR_DIR               0x10
#define FF_FAT_ATTR_ARCHIVE           0x20
#define FF_FAT_ATTR_LFN               0x0F



#define ENTRIES_PER_SECTOR 16
#define ENTRY_SIZE 32
class Disk_c;
class DirUnit_c;

struct DirEntry_st
{
  union
  {
  struct
  {
  char name[8];
  char nameExtension[3];
  uint8_t attributes;
  uint8_t type;
  uint8_t creationTimeMs;
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessDate;
  uint16_t clusterH;
  uint16_t modificationTime;
  uint16_t modificationDate;
  uint16_t clusterL;
  uint32_t size;
  };
  uint8_t raw[32];
  };
};

struct EntryLocation_st
{
  uint32_t dirStartCluster;
  uint16_t entryStartIdx;
  uint8_t  entryLength;
};

struct EntryData_st
{
  char name[MAX_FILE_NAME];
  uint32_t firstCluster;
  uint8_t attributes;
  uint32_t fileSize;

  SystemTime_st creationTime;
  SystemTime_st modificationTime;
  SystemTime_st accessTime;

  EntryLocation_st entryLocation;
};

class FindData_c
{
  
  DirUnit_c* dirUnit;

  int actEntryIdx;
  uint32_t currentDirStartCluster;
 

  public:

  EntryData_st entryData;

  FindData_c(void);
  ~FindData_c(void);

  bool FindFirst(char* path);
  bool FindNext(void);


};






class DirUnit_c 
{
  Disk_c* disk_p;
  uint8_t* clusterBuffer;
  uint32_t clusterBufferIdx; /* cluster in clusterBuffer */

  uint8_t GetDosNameChecksum(uint8_t* name);
  uint32_t NoOfEntriesPerCluster(void);

  void FillEntryTime(uint16_t* time,uint16_t* date, SystemTime_st* timeStruct);
  void GetEntryTime(uint16_t* time,uint16_t* date, SystemTime_st* timeStruct);

  

  public:

  DirUnit_c(Disk_c* disk_p_);
  ~DirUnit_c(void);
  EntryData_st* FindEntry(char* name, uint32_t currentDirStartCluster);
  uint32_t GetDirStartCluster(char* path,bool createIfNotExists);
  int GetNextEntry(EntryData_st* entryBuf, uint32_t currentDirStartCluster, int startEntryIdx);

  bool CheckIfDosNameExists(uint32_t currentDirCluster, char* dosName);

  bool CreateNewFileEntry(uint32_t currentDirStartCluster,char* name,EntryData_st* entryData,bool newFile);
  bool RemoveFileEntry(uint32_t currentDirStartCluster, EntryData_st* entryData);
  bool UpdateFileEntry(EntryData_st* entryData);

  uint32_t CreateDirectory(uint32_t workingDirCluster,  char* dirName );

  bool MkDir( char *dirName );
  bool RmDir( char *dirName );
  bool RemoveFile( char *fileName );
  bool Rename( char * oldName, char * newName);
  

};











#endif