#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "FatUnit.hpp"

uint16_t Get16Bit(uint8_t* buf, uint16_t pos)
{
  uint16_t retVal = buf[pos+1] << 8 | buf[pos];
  return retVal;
}
uint32_t Get32Bit(uint8_t* buf, uint16_t pos)
{
  uint32_t retVal = buf[pos+3] << 24 | buf[pos+2] << 16 | buf[pos+1] << 8 | buf[pos];
  return retVal;
}

FatUnit_c::FatUnit_c(void)
{
  sBuffer = nullptr;
  
}

FatUnit_c::~FatUnit_c(void)
{
  if(sBuffer != nullptr)
  {
    delete[] sBuffer;
  }
}

bool FatUnit_c::Init(FileSystemInterface_c* interface_p_)
{
  interface_p = interface_p_;
  bool result = false;
  /* read sector 0 * (MBR) */

  sBuffer = new uint32_t[128];
  fatMutex = xSemaphoreCreateRecursiveMutex();
  xSemaphoreTakeRecursive( fatMutex, ( TickType_t )  portMAX_DELAY );

  interface_p->Read((uint8_t*)sBuffer,0,1);
  sectorIdx = 0;

  uint8_t* sectorBuffer = (uint8_t*) sBuffer;

  if(sectorBuffer[510] == 0x55 && sectorBuffer[511] == 0xAA)
  {
    /*valid MBR */
       PartitionEntry_st* partitionEntry = (PartitionEntry_st*) ( &sectorBuffer[0x1BE]);
    uint8_t partitionType = partitionEntry->type;

    interface_p->Read(sectorBuffer,partitionEntry->sectorsOffset,1);
    sectorIdx = partitionEntry->sectorsOffset; 

    if(partitionType == 0xEE)
    {
      
      EFIPartitionHeader_st* efiHeader = (EFIPartitionHeader_st*)sectorBuffer;

      uint32_t numOfPartitionEntries = efiHeader->numOfPartitionEntries;
      uint32_t currentLBA = efiHeader->currentLBA;

      interface_p->Read(sectorBuffer,currentLBA+1,1);
      sectorIdx = currentLBA+1;

      EFIPartitionEntry_st* efiEntry = (EFIPartitionEntry_st*) (sectorBuffer);


      partitionStartSector = efiEntry->firstLBA;

      interface_p->Read(sectorBuffer,partitionStartSector,1);
      sectorIdx = partitionStartSector;

      /* at this momet only entry 0 is handled */
      if(FillPartitionData(sectorBuffer) == true)
      {
        interface_p->Read(sectorBuffer,fatStartSector,1);
        sectorIdx = fatStartSector;

        result =  true;
      }
    }
    else if(partitionType == 0x0B) /* FAT32 */
    {
      if(FillPartitionData(sectorBuffer) == true)
      {
        firstFreeCluster = FindNextFreeCluster(2);
        result =  true;
      }
    }
    else if(partitionType == 0x0C) /* FAT32 */
    {
      if(FillPartitionData(sectorBuffer) == true)
      {
        firstFreeCluster = FindNextFreeCluster(2);
        result =  true;
      }
    }

  }
  xSemaphoreGiveRecursive(fatMutex);
  return result;
}

bool FatUnit_c::FillPartitionData(uint8_t* sectorBuffer)
{
  bool fat32 = false;
  bytesPerSector = Get16Bit(sectorBuffer,0x0B);
  sectorsPerCluster = sectorBuffer[0x0D];
  noOfSectors = Get16Bit(sectorBuffer,0x13);
  if (noOfSectors == 0) noOfSectors = Get32Bit(sectorBuffer,0x20);
  noOfFats = sectorBuffer [0x10];
  sectorsPerFat= Get16Bit(sectorBuffer,0x16);
  if (sectorsPerFat == 0) sectorsPerFat = Get32Bit(sectorBuffer,0x24);
  rootEntries = Get16Bit(sectorBuffer,0x11);
  rootStart = Get32Bit(sectorBuffer,0x2C);
  uint16_t noOfReservedSectors = Get16Bit(sectorBuffer,0x0E);
  uint16_t fsiOffset =  Get16Bit(sectorBuffer,0x30);

  firstFreeCluster = 2;

  if(strncmp("FAT32   ",(char*)sectorBuffer+0x52,8) == 0) 
  {
    fat32 = true;
  }

  
  uint32_t sectorsBeforeRoot = sectorsPerFat * noOfFats + noOfReservedSectors;
  uint32_t clustersStart = sectorsBeforeRoot + (rootEntries*32)/512;
  if((rootEntries*32)%512) { clustersStart++; }
  noOfClusters = 2 + (noOfSectors - clustersStart)/ sectorsPerCluster;

  fsiSector = partitionStartSector + fsiOffset;
  fatStartSector = partitionStartSector + noOfReservedSectors;
  rootStartSector = fatStartSector + noOfFats*sectorsPerFat;


  if(sectorsPerCluster > MAX_SUPPORTED_SECTORS_PER_CLUSTER)
  {
    return false;
  }
  if(fat32 == false)
  {
    return false;
  }


  return true;
}

uint32_t FatUnit_c::ClusterToSector(uint32_t cluster)
{
  uint32_t sector = rootStartSector + ( (cluster-2) * sectorsPerCluster );
  return sector;
}

uint32_t FatUnit_c::GetNextCluster(uint32_t actCluster)
{
  xSemaphoreTakeRecursive( fatMutex, ( TickType_t )  portMAX_DELAY );
  uint32_t wantedSector = actCluster/128;
  wantedSector += fatStartSector;

  if(sectorIdx != wantedSector)
  {
    interface_p->Read((uint8_t*)sBuffer,wantedSector,1);
    sectorIdx = wantedSector;
  }
  uint32_t newCluster = sBuffer [actCluster % 128];

  xSemaphoreGiveRecursive(fatMutex);
  if((newCluster & 0x0FFFFFFF) >= 0x0FFFFFF0)
  {
    return CLUSTER_NOT_VALID;
  }
  else
  {
    return newCluster;
  }

}

uint32_t FatUnit_c::GetClusterFromChain(uint32_t firstCluster, uint32_t clusterNr)
{
  #if FAT_DEBUG >= 2
  printf("GetClusterFromChain, first=%d, clusterNr=%d\n",firstCluster, clusterNr);
  #endif

  uint32_t retCluster = firstCluster;

  if(clusterNr > 0)
  {
    xSemaphoreTakeRecursive( fatMutex, ( TickType_t )  portMAX_DELAY );    

    while(clusterNr > 0)
    {

      uint32_t wantedSector = retCluster/128;
      wantedSector += fatStartSector;

      if(sectorIdx != wantedSector)
      {
        interface_p->Read((uint8_t*)sBuffer,wantedSector,1);
        sectorIdx = wantedSector;
      }
      retCluster = sBuffer [retCluster % 128];

      clusterNr --;
    }


    xSemaphoreGiveRecursive(fatMutex);
  }
  #if FAT_DEBUG >= 2
  printf("GetClusterFromChain end\n");
  #endif

  if((retCluster & 0x0FFFFFFF) >= 0x0FFFFFF0)
  {
    return CLUSTER_NOT_VALID;
  }
  else
  {
    return retCluster;
  }

}

uint32_t FatUnit_c::FindNextFreeCluster(uint32_t startCluster)
{
  #if FAT_DEBUG >= 2
  printf("FindNextFreeCluster, first=%d, \n",startCluster);
  #endif
  for(uint32_t cluster = startCluster;cluster<noOfClusters;cluster++)
  {
    uint32_t wantedSector = cluster/128;
    wantedSector += fatStartSector;

    if(sectorIdx != wantedSector)
    {
      interface_p->Read((uint8_t*)sBuffer,wantedSector,1);
      sectorIdx = wantedSector;
    }

    if(sBuffer[cluster % 128] == 0)
    {
      #if FAT_DEBUG >= 2
  printf("FindNextFreeCluster end, cluster = %d\n",cluster);
  #endif
      return cluster;
    }
  }
  #if FAT_DEBUG >= 2
  printf("FindNextFreeCluster failed\n");
  #endif
  return CLUSTER_NOT_VALID;
}

bool FatUnit_c::GetNewClusters(uint32_t* clustersArray, uint32_t noOfWantedClusters, uint32_t actChainEndCluster)
{
  xSemaphoreTakeRecursive( fatMutex, ( TickType_t )  portMAX_DELAY );
  #if FAT_DEBUG >= 2
    printf("FAT GetNewClusters, act=%d\n",actChainEndCluster);
  #endif

  uint32_t cluster = firstFreeCluster;

  uint32_t noOfClustersFetched = 0;
  int noOfClustersAlreadyExists = 0;

  bool resOk = true;

  /* prepare reservation chain */
  uint32_t prescanCluster = actChainEndCluster;

  while(noOfClustersFetched < noOfWantedClusters)
  {
    if(prescanCluster != CLUSTER_NOT_VALID)
    {
      prescanCluster = GetNextCluster(prescanCluster);

    }

    if(prescanCluster != CLUSTER_NOT_VALID)
    {
      cluster = prescanCluster;
      noOfClustersAlreadyExists++;
    }
    else
    {
      cluster = FindNextFreeCluster(cluster);
    }

    

    if(cluster != CLUSTER_NOT_VALID)
    {
      clustersArray[noOfClustersFetched] = cluster;
      noOfClustersFetched++;
      cluster++;
      if(cluster >= noOfClusters)
      {
        resOk = false;
        break;
      }
    }
    else
    {
      resOk = false;
      break;
    }
  }

  if(resOk)
  {


    /* execute reservation */

    bool actChainEndUpdated = false;
    bool sectorToWrite = false;

    if(actChainEndCluster == CLUSTER_NOT_VALID)
    {
      actChainEndUpdated = true;
    }

    for(int i=noOfWantedClusters-1;i>=noOfClustersAlreadyExists;i--)
    {
      if(actChainEndUpdated == false)
      {
        if(sectorIdx == actChainEndCluster/128)
        {
          sectorToWrite = true;
          actChainEndUpdated = true;
          sBuffer[actChainEndCluster%128] = clustersArray[0];
        }
      }

      uint32_t wantedSector = clustersArray[i]/128;
      wantedSector += fatStartSector;
      if(sectorIdx != wantedSector)
      {
        if(sectorToWrite)
        {
          /* write to both fats */
          interface_p->Write((uint8_t*)sBuffer,sectorIdx,1);
          interface_p->Write((uint8_t*)sBuffer,sectorIdx+sectorsPerFat,1);

          sectorToWrite = false;
        }
        interface_p->Read((uint8_t*)sBuffer,wantedSector,1);
        sectorIdx = wantedSector;
      }

      if(i == noOfWantedClusters-1)
      {
        sBuffer[clustersArray[i]%128] = 0x0FFFFFFF;
      }
      else
      {
        sBuffer[clustersArray[i]%128] = clustersArray[i+1];
      }
      sectorToWrite =true;
    }

    if(sectorToWrite)
    {
      /* write to both fats */
      interface_p->Write((uint8_t*)sBuffer,sectorIdx,1);
      interface_p->Write((uint8_t*)sBuffer,sectorIdx+sectorsPerFat,1);

      sectorToWrite = false;
    }

    if(actChainEndUpdated == false)
    {
      uint32_t wantedSector = actChainEndCluster/128;
      wantedSector += fatStartSector;
      interface_p->Read((uint8_t*)sBuffer,wantedSector,1);
      sectorIdx = wantedSector;
      sBuffer[actChainEndCluster%128] = clustersArray[0];

      /* write to both fats */
      interface_p->Write((uint8_t*)sBuffer,sectorIdx,1);
      interface_p->Write((uint8_t*)sBuffer,sectorIdx+sectorsPerFat,1);
    }

  }
  firstFreeCluster = FindNextFreeCluster(firstFreeCluster);
  xSemaphoreGiveRecursive(fatMutex);
  #if FAT_DEBUG >= 2
    printf("FAT fetched %d clusters: ",noOfWantedClusters);
    for(int i=0;i<noOfWantedClusters;i++) { printf("%d, ",clustersArray[i]); }
    printf("\n");     
  #endif
  return resOk;

}

bool FatUnit_c::ReleaseClusters(uint32_t firstCluster, uint32_t wantedChainEndCluster)
{
  #if FAT_DEBUG >= 2
    printf("FAT releaseClusters \n");
  #endif
  xSemaphoreTakeRecursive( fatMutex, ( TickType_t )  portMAX_DELAY );

  uint32_t cluster = firstCluster;
  uint32_t nextCluster;
  int clustersCnt = 0;

  bool actChainEndUpdated = false;
  bool sectorToWrite = false;

  if(wantedChainEndCluster == CLUSTER_NOT_VALID)
  {
    actChainEndUpdated = true;
  }

  while(cluster < 0x0FFFFFF0 &&  cluster>=2)
  {
    if(actChainEndUpdated == false)
    {
      if(sectorIdx == wantedChainEndCluster/128)
      {
        sectorToWrite = true;
        actChainEndUpdated = true;
        sBuffer[wantedChainEndCluster%128] = 0x0FFFFFFF;
      }
    }


    uint32_t wantedSector = cluster/128;
    wantedSector += fatStartSector;
    if(sectorIdx != wantedSector)
    {
      if(sectorToWrite)
      {
        /* write to both fats */
        interface_p->Write((uint8_t*)sBuffer,sectorIdx,1);
        interface_p->Write((uint8_t*)sBuffer,sectorIdx+sectorsPerFat,1);

        sectorToWrite = false;
      }
      interface_p->Read((uint8_t*)sBuffer,wantedSector,1);
      sectorIdx = wantedSector;
    }
    nextCluster = sBuffer[cluster%128];
    sBuffer[cluster%128] = 0;
    if(firstFreeCluster > cluster) { firstFreeCluster = cluster; }
    cluster = nextCluster;
    sectorToWrite = true;
    clustersCnt++;
  }

  if(sectorToWrite)
  {
    /* write to both fats */
    interface_p->Write((uint8_t*)sBuffer,sectorIdx,1);
    interface_p->Write((uint8_t*)sBuffer,sectorIdx+sectorsPerFat,1);

    sectorToWrite = false;
  }

  if(actChainEndUpdated == false)
  {
    uint32_t wantedSector = wantedChainEndCluster/128;
    wantedSector += fatStartSector;
    interface_p->Read((uint8_t*)sBuffer,wantedSector,1);
    sectorIdx = wantedSector;
    sBuffer[wantedChainEndCluster%128] = 0x0FFFFFFF;

    /* write to both fats */
    interface_p->Write((uint8_t*)sBuffer,sectorIdx,1);
    interface_p->Write((uint8_t*)sBuffer,sectorIdx+sectorsPerFat,1);
  }


  xSemaphoreGiveRecursive(fatMutex);
  #if FAT_DEBUG >= 2
    printf("FAT released %d clusters  \n",clustersCnt);
  #endif
  return true;
}



int FatUnit_c::ReadClustersChain(uint32_t firstCluster, uint32_t* clustersChain, uint32_t size)
{
  int clusters = 0;
  

  xSemaphoreTakeRecursive( fatMutex, ( TickType_t )  portMAX_DELAY );


  uint32_t cluster = firstCluster;


  while(cluster < 0x0FFFFFF0 &&  cluster>=2)
  {
    clustersChain[clusters] = cluster;
    clusters++;
    uint32_t wantedSector = cluster/128;
    wantedSector += fatStartSector;
    if(sectorIdx != wantedSector)
    {
      interface_p->Read((uint8_t*)sBuffer,wantedSector,1);
      sectorIdx = wantedSector;
    }
    cluster = sBuffer [cluster % 128];

    
    if(clusters >= size) 
    {
      break;
    }
  }



  xSemaphoreGiveRecursive(fatMutex);

  return clusters;
}