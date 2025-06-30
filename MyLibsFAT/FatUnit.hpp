
#ifndef FATUNIT_C
#define FATUNIT_C

#include "FatCommonDef.hpp"
#include "semphr.h"

#define CLUSTER_NOT_VALID 0xFFFFFFFF
#define CLUSTER_NR_NOT_VALID 0xFFFFFFFF

struct PartitionEntry_st
{
  uint8_t state;
  uint8_t beginHead;
  uint16_t beginCylinder; 
  uint8_t type;
  uint8_t endHead;
  uint16_t endCylinder;
  uint32_t sectorsOffset;
  uint32_t sectorsInPartition;
};

struct EFIPartitionHeader_st
{
  char signature[8];
  uint32_t revision;
  uint32_t headerSize;
  uint32_t crcHeader;
  uint32_t res1;
  uint64_t currentLBA;
  uint64_t backupLBA;
  uint64_t firstLBA;
  uint64_t lastLBA;
  uint8_t diskGuid[8];
  uint64_t startingLBA;
  uint32_t numOfPartitionEntries;
  uint32_t sizeOfPrtitionEntry;
  uint32_t crcPartitionEntries;
};

struct EFIPartitionEntry_st
{
  uint8_t partitionTypeGUID[16];
  uint8_t uniquePartitionGUID[16];
  uint64_t firstLBA;
  uint64_t lastLBA;
  uint64_t attributeFlags;
  char partitionName[72];
};

class FatUnit_c
{
  
  FileSystemInterface_c* interface_p;
  uint32_t* sBuffer;
  uint32_t sectorIdx;

  uint16_t bytesPerSector;
  uint8_t sectorsPerCluster;
  uint32_t noOfSectors;
  uint8_t noOfFats;
  uint32_t sectorsPerFat;  
  uint16_t rootEntries;
  uint32_t rootStart;

  uint32_t partitionStartSector;
  uint32_t fsiSector;
  uint32_t fatStartSector;
  uint32_t rootStartSector;

  uint32_t noOfClusters;

  uint32_t firstFreeCluster;

  SemaphoreHandle_t fatMutex;


  bool FillPartitionData(uint8_t* sectorBuf);

  uint32_t FindNextFreeCluster(uint32_t startCluster);

  public:

  FatUnit_c(void);
  ~FatUnit_c(void);

  bool Init( FileSystemInterface_c* interface_p_);

  uint32_t GetSectorsPerCluster(void) { return sectorsPerCluster; }

  uint32_t ClusterToSector(uint32_t cluster);
  uint32_t GetRootDirCluster(void) { return 2; }
  uint32_t GetNextCluster(uint32_t actCluster);
  uint32_t GetClusterFromChain(uint32_t firstCluster, uint32_t clusterNr);

  bool GetNewClusters(uint32_t* clustersArray, uint32_t noOfClusters, uint32_t actChainEndCluster);
  bool ReleaseClusters(uint32_t firstCluster, uint32_t wantedChainEndCluster);

  int ReadClustersChain(uint32_t startCluster, uint32_t* clustersChain, uint32_t size);
};










#endif