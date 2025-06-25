
#ifndef FILE_UNIT_C
#define FILE_UNIT_C

#include "FatCommonDef.hpp"



class File_c
{

  DirUnit_c* dirUnit;
  Disk_c* disk_p;
  EntryData_st* fileEntry;

  bool modeRead;
  bool modeWrite;
  bool createIfNotExists;
  bool append;
  bool truncate;

  bool fileModified;

  uint32_t filePosition;
  uint32_t fileSize;
  
  //uint32_t currentDataCluster;
  //uint32_t currentDataClusterNr; /* idx in clusters chain */

  uint32_t lastCluster;
  uint32_t noOfClusters;
  
  void DecodeMode(const char* modeStr);
  uint32_t FileSizeToClusters(uint32_t fileSize);

  uint32_t FilePos2Sector(uint32_t pos);
  uint32_t GetClusterFromChain(uint32_t clusterNr); 
  uint32_t storedScanCluster;
  uint32_t storedScanClusterNr;

  public:

  enum SEEK_et
  {
    SEEK_MODE_CUR,
    SEEK_MODE_SET,
    SEEK_MODE_END
  };

  File_c(void);
  ~File_c(void);

  bool Open( const char * file, const char * mode );
  void Close(void);
  uint32_t GetSize(void);

  bool Seek(int offset,SEEK_et seekMode);

  bool Read(uint8_t* buffer,uint16_t bufferSize);
  bool Write(uint8_t* buffer,uint16_t bytesToWrite);

};

#endif