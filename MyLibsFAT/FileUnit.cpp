
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "DirUnit.hpp"
#include "FileSystem.hpp"
#include "FileUnit.hpp"
#include "CwdUnit.hpp"
#include "GeneralConfig.h"

extern FileSystem_c fileSystem;

File_c::File_c(void)
{
   modeRead = false;
   modeWrite = false;
   createIfNotExists = false;
   append = false;
   truncate = false;

   fileModified = false;

   filePosition = 0;

   fileEntry = nullptr;
   dirUnit = nullptr;

   storedScanCluster = CLUSTER_NOT_VALID;
   storedScanClusterNr = CLUSTER_NR_NOT_VALID;

}

File_c::~File_c(void)
{
  if(fileEntry!= nullptr)
  {
    delete fileEntry;
  }
  if(dirUnit != nullptr)
  {
    delete dirUnit;
  }

}

void File_c::DecodeMode(const char* modeStr)
{


    while( *modeStr != '\0' )
    {
        switch( *modeStr )
        {
            case 'r': /* Allow Read. */
            case 'R':
                modeRead = true;
                break;

            case 'w': /* Allow Write. */
            case 'W':
                modeWrite = true;
                createIfNotExists = true;
                truncate = true;
                break;

            case 'a': /* Append new writes to the end of the file. */
            case 'A':
                modeWrite = true;
                append = true;
                createIfNotExists = true;
                break;

            case '+':                        /* Update the file, don't Append! */
                modeRead = true;
                modeWrite = true; /* RW Mode. */
                break;

            default:
                break;
        }

        modeStr++;
    }
}

bool File_c::Open( const char * file, const char * mode )
{
  CwdUnit_c* cwd_p = CwdUnit_c::GetCwdPtr();
  char* absPath = cwd_p->GetAbsolutePath(file);

  char* fileSeparator = strrchr(absPath,'/');

  char* fileName = fileSeparator+1;
  *fileSeparator = 0;

  disk_p = fileSystem.GetDisk(absPath);
  if(disk_p == nullptr)
  {
    delete[] absPath;
    printf("FILESYS: disk null\n");
    return false;
  }
  dirUnit = new DirUnit_c(disk_p);

  DecodeMode(mode);



  uint32_t workingDirCluster = dirUnit->GetDirStartCluster(absPath,createIfNotExists);
  

  if(workingDirCluster == CLUSTER_NOT_VALID)
  {
    delete dirUnit;
    dirUnit = nullptr;
    delete[] absPath;
    printf("FILESYS: workingDirCluster null\n");
    return false;
  }

  

  fileEntry = dirUnit->FindEntry(fileName,  workingDirCluster);

  if(fileEntry == nullptr)
  {
    if(createIfNotExists == false)
    {
      delete dirUnit;
      dirUnit = nullptr;
      delete[] absPath;
      printf("FILESYS: fileEntry null\n");
      return false;
    }
    else
    {
      /* create file entry */
      fileEntry = new EntryData_st;
      
      fileEntry->fileSize = 0;
      fileEntry->firstCluster = CLUSTER_NOT_VALID;
      fileEntry->attributes = 0;
      

      dirUnit->CreateNewFileEntry(workingDirCluster,fileName,fileEntry,true);

      filePosition = 0;
      fileSize = 0;
    }
  }
  else
  {
    fileSize = fileEntry->fileSize;
    if(append)
    {
      filePosition = fileEntry->fileSize;      
    }
    else
    {
      filePosition = 0;
    }

    if(truncate)
    {
      fileSize = 0;
      filePosition = 0;
    }

  }

  GetClusterFromChain(CLUSTER_NR_NOT_VALID); /* update last cluster and no ofClusters */
  delete[] absPath;
  return true;
}

void File_c::Close(void)
{
  #if FAT_DEBUG >= 2
  printf("file close\n");
  #endif

  if(fileModified)
  {
    /* update entry ?*/
    #if FAT_DEBUG >= 2
    printf("last entry update\n");
    #endif

    dirUnit->UpdateFileEntry(fileEntry);

  }

  /* update last access date in entry? */

  delete this;

}
uint32_t File_c::GetSize(void)
{
  if(fileEntry != nullptr)
  {
    return fileEntry->fileSize;
  }
  else
  {
    return 0;
  }

}

bool File_c::Seek(int offset,SEEK_et seekMode)
{
  int newPosition;
  switch(seekMode)
  {
    case SEEK_MODE_CUR:
      newPosition = filePosition + offset;
    break;
    case SEEK_MODE_END:
       newPosition = fileSize + offset;
    break;
    case SEEK_MODE_SET:
      newPosition = offset;
    break;

    default:
      return false;
  }

  if(newPosition < 0 || newPosition > fileSize)
  {
    return false;
  }

  filePosition = newPosition;



  return true;
}

uint32_t File_c::GetClusterFromChain(uint32_t clusterNr)
{
  uint32_t prevCluster = fileEntry->firstCluster;
  uint32_t scanCluster = prevCluster;
  uint32_t scanNr = 0;

  if((clusterNr >= storedScanClusterNr) && (storedScanClusterNr != CLUSTER_NR_NOT_VALID))
  {
    scanCluster = storedScanCluster;
    scanNr = storedScanClusterNr;
  }

  while(1)
  {  
    if(scanCluster == CLUSTER_NOT_VALID)
    {
      lastCluster = prevCluster;
      noOfClusters = scanNr;

      storedScanCluster = CLUSTER_NOT_VALID;
      storedScanClusterNr = CLUSTER_NR_NOT_VALID;
      return CLUSTER_NOT_VALID;
    }
    
    if (scanNr == clusterNr)
    {
      storedScanCluster = scanCluster;
      storedScanClusterNr = scanNr;
      return scanCluster;
    }
    prevCluster = scanCluster; 
    scanNr++;
    scanCluster = disk_p->fatUnit.GetNextCluster(scanCluster);
  }
  return CLUSTER_NOT_VALID;

}

uint32_t File_c::FilePos2Sector(uint32_t pos)
{  
  uint32_t positionCluster = GetClusterFromChain(pos / (disk_p->fatUnit.GetSectorsPerCluster()*512));
  uint8_t  positionInCluster = (pos / 512)  % (disk_p->fatUnit.GetSectorsPerCluster());

  if(positionCluster != CLUSTER_NOT_VALID)
  {
    uint32_t sector = disk_p->fatUnit.ClusterToSector(positionCluster);
    return sector + positionInCluster;
  }
  else
  {
    return 0xFFFFFFFF;
  } 
}

bool File_c::Read(uint8_t* buffer,uint16_t bufferToRead)
{


  if(modeRead == false)
  {
    return false;
  }

  uint16_t bytesReady = 0;

  bool endOfClusterChain = false;
  bool cont = true;

  while((bytesReady < bufferToRead) && cont)
  {
    uint32_t positionInSector = filePosition % 512;

    uint32_t bytesLeftInFile = fileEntry->fileSize - filePosition;
    uint32_t bytesLeftInBuffer = bufferToRead - bytesReady;

    

    if(positionInSector != 0)
    {
      /* read start part of data */

      uint8_t* sectorBuffer = new uint8_t[512];
      
      uint32_t sector = FilePos2Sector(filePosition);  

      bool r = disk_p->interface_p->Read(sectorBuffer,sector, 1);

      uint16_t bytesToRead = 512-positionInSector;
      
      if(bytesToRead > bytesLeftInFile) { bytesToRead = bytesLeftInFile; }
      
      if(bytesToRead > bytesLeftInBuffer) { bytesToRead = bytesLeftInBuffer; }

      memcpy( &buffer[bytesReady], &sectorBuffer[positionInSector], bytesToRead);

      bytesReady += bytesToRead;
      filePosition += bytesToRead;

      //printf(" read start %d bytes\n",bytesToRead);

      delete[] sectorBuffer;

    }
    else if(bytesLeftInFile < 512 || bytesLeftInBuffer < 512)
    {
      /* read end part of data */
      uint32_t bytesToRead = bytesLeftInBuffer;
      uint32_t sector = FilePos2Sector(filePosition);

      uint8_t* sectorBuffer = new uint8_t[512];
      
      if(bytesToRead > bytesLeftInFile)
      {
        bytesToRead = bytesLeftInFile;                   
      }          

      bool r = disk_p->interface_p->Read(sectorBuffer,sector, 1);

      memcpy( &buffer[bytesReady], &sectorBuffer[0], bytesToRead);

      delete[] sectorBuffer;

      bytesReady += bytesToRead;
      filePosition += bytesToRead;
      //printf(" read end %d bytes\n",bytesToRead);
      cont = false; 
    }
    else
    { 
      /* read full sectors */

      /* get longest possible chain */

      uint32_t bytesToRead = bytesLeftInBuffer;
      if(bytesToRead > bytesLeftInFile)
      {
        bytesToRead = bytesLeftInFile;                   
      }

      uint32_t maxWantedSectors = bytesToRead/512;
      bool unalignedRead = false;

      uint8_t alignMask = disk_p->GetAlignMask();
      if( ((uint32_t)(buffer+bytesReady) & alignMask) != 0)
      {
        unalignedRead = true;

        if(maxWantedSectors > FILE_BUFFER_SECTORS)
        {
          maxWantedSectors = FILE_BUFFER_SECTORS;
        }
      }      

      bool chainContinous = true;
      uint32_t countedSectors = 0;
      uint32_t tmpFilePos = filePosition;

      uint32_t firstSector = FilePos2Sector(filePosition);

      uint32_t scanSector = firstSector;
      while(countedSectors < maxWantedSectors && chainContinous)
      {
        countedSectors++;

        tmpFilePos += 512;
        uint32_t prevScanSector = scanSector;
        scanSector = FilePos2Sector(tmpFilePos);

        if(prevScanSector + 1 != scanSector)
        {
          chainContinous = false;
        }
      }

      bool r;
      if(unalignedRead)
      {
        uint8_t* readBuffer = new uint8_t[countedSectors * 512];        
        r = disk_p->interface_p->Read(readBuffer,firstSector, countedSectors);
        memcpy(&buffer[bytesReady],readBuffer,countedSectors * 512);
        delete[] readBuffer;
      }
      else
      {
        r = disk_p->interface_p->Read(&buffer[bytesReady],firstSector, countedSectors);
      }

      //printf(" read %d sectors\n",countedSectors);

      bytesReady += countedSectors*512;
      filePosition += countedSectors*512;

      if(endOfClusterChain)
      {
        cont = false;
      }
    }
  }
  #if FAT_DEBUG >= 1
    printf("FAT fileRead %d bytes\n",bytesReady);
  #endif

  return true;
}


uint32_t File_c::FileSizeToClusters(uint32_t fileSize)
{
  uint32_t clusters;

  uint32_t clusterSize = 512 * disk_p->fatUnit.GetSectorsPerCluster();

  clusters = fileSize / clusterSize;
  if(fileSize % clusterSize != 0)
  {
    clusters++;
  }
  return clusters;
};


bool File_c::Write(uint8_t* buffer,uint16_t bytesToWrite)
{
  #if FAT_DEBUG >= 1
    printf("FAT fileWrite %d bytes\n",bytesToWrite);
    //printf("currentDataCluster at start = %d\n",currentDataCluster);
  #endif

  if(modeWrite == false)
  {
    return false;
  }

  /* seize clusters for whole buffer */

  uint32_t wantedClusters = FileSizeToClusters(filePosition + bytesToWrite);

  int clustersToSeize = wantedClusters - noOfClusters;

  uint32_t* clustersArray = nullptr;
   
  
  if(clustersToSeize > 0)
  {
    clustersArray = new uint32_t[clustersToSeize];
    disk_p->fatUnit.GetNewClusters(clustersArray,clustersToSeize,lastCluster);
    lastCluster = clustersArray[clustersToSeize-1];
    noOfClusters += clustersToSeize ;

    if(fileEntry->firstCluster == CLUSTER_NOT_VALID)
    {
      fileEntry->firstCluster = clustersArray[0]; 
    }
  }

  /* write data */

  uint32_t bytesWritten = 0;

  while(bytesWritten < bytesToWrite)
  {

    uint32_t positionInSector = filePosition % 512;

    uint32_t bytesLeftToWrite = bytesToWrite - bytesWritten;

    if(positionInSector != 0)
    {
      /* write start part of data */
      #if FAT_DEBUG >= 2
      printf("write start\n");
      #endif

      uint16_t bytesWrittenTmp = 512 - positionInSector;
      if(bytesWrittenTmp > bytesLeftToWrite) { bytesWrittenTmp = bytesLeftToWrite; }

      uint8_t* sectorBuffer = new uint8_t[512];

      uint32_t sector = FilePos2Sector(filePosition);   

      bool r = disk_p->interface_p->Read(sectorBuffer,sector, 1);

      memcpy(sectorBuffer+positionInSector,buffer,bytesWrittenTmp);

      bool r2 = disk_p->interface_p->Write(sectorBuffer,sector, 1);


      delete[] sectorBuffer;

      bytesWritten += bytesWrittenTmp;
      filePosition += bytesWrittenTmp;
    }
    else if(bytesLeftToWrite < 512) 
    {
      /* write end part of data */
      #if FAT_DEBUG >= 2
      printf("write end\n");
      #endif

      uint8_t* sectorBuffer = new uint8_t[512];

      uint32_t sector = FilePos2Sector(filePosition);

      bool r = disk_p->interface_p->Read(sectorBuffer,sector, 1);

      memcpy(sectorBuffer,buffer+bytesWritten,bytesLeftToWrite);

      bool r2 = disk_p->interface_p->Write(sectorBuffer,sector, 1);

      delete[] sectorBuffer;

      bytesWritten += bytesLeftToWrite;
      filePosition += bytesLeftToWrite;
    }
    else 
    {
      /* write full sectors */
      #if FAT_DEBUG >= 2
      printf("write full\n");
      #endif
      uint32_t maxWantedSectors = bytesLeftToWrite/512;

      bool unalignedWrite = false;

      uint8_t alignMask = disk_p->GetAlignMask();
      if( ((uint32_t)(buffer+bytesWritten) & alignMask) != 0)
      {
        unalignedWrite = true;

        if(maxWantedSectors > FILE_BUFFER_SECTORS)
        {
          maxWantedSectors = FILE_BUFFER_SECTORS;
        }

      }

      bool chainContinous = true;
      uint32_t countedSectors = 0;
      uint32_t tmpFilePos = filePosition;

      uint32_t firstSector = FilePos2Sector(filePosition);

      uint32_t scanSector = firstSector;
      while(countedSectors < maxWantedSectors && chainContinous)
      {
        countedSectors++;

        tmpFilePos += 512;
        uint32_t prevScanSector = scanSector;
        scanSector = FilePos2Sector(tmpFilePos);

        if(prevScanSector + 1 != scanSector)
        {
          chainContinous = false;
        }
      }

      bool r;
      if(unalignedWrite)
      {
        uint8_t* writeBuffer = new uint8_t[countedSectors * 512];
        memcpy(writeBuffer,&buffer[bytesWritten],countedSectors * 512);
        r = disk_p->interface_p->Write(writeBuffer,firstSector, countedSectors);
        delete[] writeBuffer;
      }
      else
      {
        r = disk_p->interface_p->Write(&buffer[bytesWritten],firstSector, countedSectors);
      }

      

      bytesWritten += countedSectors * 512;
      filePosition += countedSectors *512;

    }
  }

  /* update file entry  (size and firstCluster */

  if(clustersArray != nullptr)
  {
    delete[] clustersArray;
  }  

  fileEntry->fileSize = filePosition;
  fileModified = true;
  #if FAT_DEBUG >= 2
  printf("update file entry, size = %d\n",filePosition);
  #endif
  dirUnit->UpdateFileEntry(fileEntry);

  return true;
}