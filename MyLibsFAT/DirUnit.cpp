#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "TimeClass.hpp"
#include "GeneralConfig.h"

#include "DirUnit.hpp"
#include "FileSystem.hpp"
#include "CwdUnit.hpp"
#include "lfnUnit.hpp"

const uint8_t lfdPositions[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};


extern FileSystem_c fileSystem;

FindData_c::FindData_c(void)
{
  dirUnit = nullptr;
}
FindData_c::~FindData_c(void)
{
  if(dirUnit != nullptr)
  {
    delete dirUnit;
  }
}

bool FindData_c::FindFirst(char* path)
{
  CwdUnit_c* cwd_p = CwdUnit_c::GetCwdPtr();
  char* absPath = cwd_p->GetAbsolutePath(path);


  Disk_c* disk_p = fileSystem.GetDisk(absPath);
  if(disk_p == nullptr)
  {
    delete[] absPath;
    return false;
  }
  dirUnit = new DirUnit_c(disk_p);

  currentDirStartCluster = dirUnit->GetDirStartCluster(absPath,false);
  delete[] absPath;

  if(currentDirStartCluster != CLUSTER_NOT_VALID)
  {
    actEntryIdx = dirUnit->GetNextEntry(&entryData,currentDirStartCluster,0);
    if(actEntryIdx >= 0)
    {
      return true;
    }
    else
    {
      return false;
    }
    



  }
  else
  {
    return 0;
  }

}

bool FindData_c::FindNext(void)
{
  if(dirUnit == nullptr)
  {
    return false;
  }
  if(currentDirStartCluster != CLUSTER_NOT_VALID)
  {
    actEntryIdx = dirUnit->GetNextEntry(&entryData,currentDirStartCluster,actEntryIdx);
    if(actEntryIdx >= 0)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
    return false;
  }

}

DirUnit_c::DirUnit_c(Disk_c* disk_p_)
{
  disk_p = disk_p_;
  clusterBuffer = new uint8_t[disk_p->fatUnit.GetSectorsPerCluster() * 512];
  clusterBufferIdx = 0;
}


DirUnit_c::~DirUnit_c(void)
{
  delete clusterBuffer;
}

uint32_t DirUnit_c::GetDirStartCluster(char* path,bool createIfNotExists)
{
  char* dirName;
  uint32_t actDirCluster = disk_p->fatUnit.GetRootDirCluster();



  char* dirNameTmp = new char [MAX_FILE_NAME];
  bool pathParsed = false;
  bool resOk = false;

  //printf("RUN ChDir {%s}\n",path);

  while(pathParsed == false)
  {
    char* ptr = strchr(path,'/');

    if(ptr == nullptr)
    {
      strcpy(dirNameTmp,path);
      pathParsed = true;
    }
    else
    {
      strncpy(dirNameTmp,path,ptr-path);
      dirNameTmp[ptr-path] = 0;
      path = ptr+1;

      if(* path == 0)
      {
        pathParsed = true;
      }
    }
 
    //printf("GOTO {%s}\n",dirNameTmp);

    if(dirNameTmp[0] == 0)
    {
      actDirCluster = disk_p->fatUnit.GetRootDirCluster();
    }
    else
    {

      EntryData_st* entryData = FindEntry2(dirNameTmp,actDirCluster);
      if(entryData != nullptr)
      {
        actDirCluster = entryData->firstCluster;
        delete entryData;
      }
      else if(createIfNotExists)
      {
        actDirCluster = CreateDirectory(actDirCluster,dirNameTmp);
      }
      else
      {
        actDirCluster = CLUSTER_NOT_VALID;
        break;
      }
    }

    if(actDirCluster == CLUSTER_NOT_VALID)
    {
      break;
    }

  }

  delete[] dirNameTmp;

  return actDirCluster;

}

EntryData_st* DirUnit_c::FindEntry(char* name, uint32_t currentDirStartCluster)
{
  int entryIdx = 0;

  //printf("FindEntry %s, CL=%d\n",name,currentDirStartCluster);

  bool found = false;
  EntryData_st* entryBuf = new EntryData_st;

  while(found == false)
  {
    entryIdx = GetNextEntry(entryBuf,currentDirStartCluster,entryIdx);

    if(entryIdx >= 0)
    {
      //printf("Found entry (%s)\n",entryBuf->name);
      if(strcmp(name,entryBuf->name) == 0)
      {
        found = true;
        break;
      }

    }
    else
    {
      break;
    }


  }

  if(found == false)
  {
    delete entryBuf;
    entryBuf = nullptr;
  }
  return entryBuf;



 // return entryBuf;
}

EntryData_st* DirUnit_c::FindEntry2(char* name, uint32_t currentDirStartCluster)
{

  int entryIdx = 0;

  //printf("FindEntry2 %s, CL=%d\n",name,currentDirStartCluster);

  bool found = false;
  EntryData_st* entryBuf = new EntryData_st;

  SearchEntryData_c searchData;


  searchData.entryClusterBuffer = new uint8_t[disk_p->fatUnit.GetSectorsPerCluster() * 512];
  searchData.cluster = CLUSTER_NOT_VALID;
  searchData.clusterIdx = 0;  

  while(found == false)
  {
    entryIdx = GetNextEntry2(entryBuf, &searchData,  currentDirStartCluster,entryIdx);

    if(entryIdx >= 0)
    {
      //printf("Found entry (%s)\n",entryBuf->name);
      if(strcmp(name,entryBuf->name) == 0)
      {
        found = true;
        break;
      }

    }
    else
    {
      break;
    }


  }

  delete[] searchData.entryClusterBuffer;

  if(found == false)
  {
    delete entryBuf;
    entryBuf = nullptr;
  }
  return entryBuf;



}

int DirUnit_c::GetNextEntry(EntryData_st* entryBuf, uint32_t currentDirStartCluster, int startEntryNr)
{

  bool entryFetched = false;

  uint32_t currentEntryNr = startEntryNr;

  uint8_t lfnChecksum;
  bool lfnFound = false;

  while(entryFetched == false)
  {
    uint32_t clusterIdx = currentEntryNr / NoOfEntriesPerCluster();
    uint32_t entryIdx = currentEntryNr % NoOfEntriesPerCluster();

    uint32_t cluster = disk_p->fatUnit.GetClusterFromChain(currentDirStartCluster,clusterIdx);

    if(cluster == CLUSTER_NOT_VALID)
    {
      break;
    }
    else if (clusterBufferIdx != cluster)
    {
      disk_p->ReadCluster(clusterBuffer,cluster);
      clusterBufferIdx = cluster;
    }

    DirEntry_st* dirEntry = (DirEntry_st*) &clusterBuffer[32*entryIdx];

    if(dirEntry->name[0] == 0)
    {
      /* end of directory*/
      break;      
    }
    else if(dirEntry->name[0] == 0xE5)
    {
      /* empty entry, skip it*/
    }
    else if(dirEntry->attributes == 0x0F)
    {
      /* long file entry */

      uint8_t lfnNo = dirEntry->raw[0];
      lfnNo &= 0x0F;

      if(dirEntry->raw[0] & 0x40) { entryBuf->name[13*lfnNo] = 0; }

      if(lfnNo > 0 ) { lfnNo--; }      

      for(int i=0;i<13;i++)
      {
        entryBuf->name[i+(13*lfnNo)] = dirEntry->raw[ lfdPositions[i] ];
      } 
      
      lfnChecksum = dirEntry->raw[0x0D];
      lfnFound = true;

      if(dirEntry->raw[0] & 0x40)
      {
        entryBuf->entryLocation.entryStartIdx = currentEntryNr;
        entryBuf->entryLocation.entryLength = 0;
        entryBuf->entryLocation.dirStartCluster = currentDirStartCluster;
      }
      entryBuf->entryLocation.entryLength++;


    }
    else
    {
      /* normal entry */

      uint8_t dosNameChecksum = GetDosNameChecksum(dirEntry->raw);

      if((lfnFound == false)||(dosNameChecksum != lfnChecksum))
      {
        /* use DOS name */
        uint8_t pos = 0;
        for(int i=0;i<8;i++)
        {
          if(dirEntry->name[i] != ' ')
          {
            char c = dirEntry->name[i];
            if((dirEntry->type & 0x08) && (c >= 'A') && (c <= 'Z'))
            {
              c += ('a' - 'A');
            }
            entryBuf->name[i] = c;
            pos++;
          }
          else
          {
            break;
          }
        }
        if(dirEntry->attributes & FF_FAT_ATTR_DIR)
        {
          entryBuf->name[pos] = 0;
        }
        else
        {
          entryBuf->name[pos++] = '.';
          for(int i=0;i<3;i++)
          {
            char c = dirEntry->nameExtension[i];
            if((dirEntry->type & 0x10) && (c >= 'A') && (c <= 'Z'))
            {
              c += ('a' - 'A');
            }
            entryBuf->name[pos++] = c;

          }
          entryBuf->name[pos] = 0;
        }
        entryBuf->entryLocation.entryStartIdx = currentEntryNr;
        entryBuf->entryLocation.entryLength = 0;
        entryBuf->entryLocation.dirStartCluster = currentDirStartCluster;
      }
      entryBuf->entryLocation.entryLength++;

      entryBuf->attributes = dirEntry->attributes;
      entryBuf->fileSize = dirEntry->size;
      entryBuf->firstCluster = dirEntry->clusterH << 16 | dirEntry->clusterL;

      GetEntryTime(&dirEntry->creationTime,&dirEntry->creationDate,&entryBuf->creationTime);
      GetEntryTime(&dirEntry->modificationTime,&dirEntry->modificationDate,&entryBuf->modificationTime);
      GetEntryTime(nullptr,&dirEntry->lastAccessDate,&entryBuf->accessTime);

      entryFetched = true;

    }
    currentEntryNr ++; 
  }

  //uint32_t sectorToOpen = 


  if(entryFetched)
  {
    return currentEntryNr;
  }
  else
  {
    return -1;
  }
}


int DirUnit_c::GetNextEntry2(EntryData_st* entryBuf, SearchEntryData_c* searchData, uint32_t currentDirStartCluster, int startEntryNr)
{

  bool entryFetched = false;

  uint32_t currentEntryNr = startEntryNr;

  uint8_t lfnChecksum;
  bool lfnFound = false;

  while(entryFetched == false)
  {
    uint32_t clusterIdx = currentEntryNr / NoOfEntriesPerCluster();
    uint32_t entryIdx = currentEntryNr % NoOfEntriesPerCluster();

    if((searchData->cluster == CLUSTER_NOT_VALID) || ( searchData->clusterIdx != clusterIdx))
    {
      searchData->cluster = disk_p->fatUnit.GetClusterFromChain(currentDirStartCluster,clusterIdx);
      if(searchData->cluster == CLUSTER_NOT_VALID)
      {
        break;
      }
      else
      {
        searchData->clusterIdx = clusterIdx;
        disk_p->ReadCluster(searchData->entryClusterBuffer,searchData->cluster);
      }
    }
    DirEntry_st* dirEntry = (DirEntry_st*) &searchData->entryClusterBuffer[32*entryIdx];

    if(dirEntry->name[0] == 0)
    {
      /* end of directory*/
      break;      
    }
    else if(dirEntry->name[0] == 0xE5)
    {
      /* empty entry, skip it*/
    }
    else if(dirEntry->attributes == 0x0F)
    {
      /* long file entry */

      uint8_t lfnNo = dirEntry->raw[0];
      lfnNo &= 0x0F;

      if(dirEntry->raw[0] & 0x40) { entryBuf->name[13*lfnNo] = 0; }

      if(lfnNo > 0 ) { lfnNo--; }      

      for(int i=0;i<13;i++)
      {
        entryBuf->name[i+(13*lfnNo)] = dirEntry->raw[ lfdPositions[i] ];
      } 
      
      lfnChecksum = dirEntry->raw[0x0D];
      lfnFound = true;

      if(dirEntry->raw[0] & 0x40)
      {
        entryBuf->entryLocation.entryStartIdx = currentEntryNr;
        entryBuf->entryLocation.entryLength = 0;
        entryBuf->entryLocation.dirStartCluster = currentDirStartCluster;
      }
      entryBuf->entryLocation.entryLength++;


    }
    else
    {
      /* normal entry */

      uint8_t dosNameChecksum = GetDosNameChecksum(dirEntry->raw);

      if((lfnFound == false)||(dosNameChecksum != lfnChecksum))
      {
        /* use DOS name */
        uint8_t pos = 0;
        for(int i=0;i<8;i++)
        {
          if(dirEntry->name[i] != ' ')
          {
            char c = dirEntry->name[i];
            if((dirEntry->type & 0x08) && (c >= 'A') && (c <= 'Z'))
            {
              c += ('a' - 'A');
            }
            entryBuf->name[i] = c;
            pos++;
          }
          else
          {
            break;
          }
        }
        if(dirEntry->attributes & FF_FAT_ATTR_DIR)
        {
          entryBuf->name[pos] = 0;
        }
        else
        {
          entryBuf->name[pos++] = '.';
          for(int i=0;i<3;i++)
          {
            char c = dirEntry->nameExtension[i];
            if((dirEntry->type & 0x10) && (c >= 'A') && (c <= 'Z'))
            {
              c += ('a' - 'A');
            }
            entryBuf->name[pos++] = c;

          }
          entryBuf->name[pos] = 0;
        }
        entryBuf->entryLocation.entryStartIdx = currentEntryNr;
        entryBuf->entryLocation.entryLength = 0;
        entryBuf->entryLocation.dirStartCluster = currentDirStartCluster;
      }
      entryBuf->entryLocation.entryLength++;

      entryBuf->attributes = dirEntry->attributes;
      entryBuf->fileSize = dirEntry->size;
      entryBuf->firstCluster = dirEntry->clusterH << 16 | dirEntry->clusterL;

      GetEntryTime(&dirEntry->creationTime,&dirEntry->creationDate,&entryBuf->creationTime);
      GetEntryTime(&dirEntry->modificationTime,&dirEntry->modificationDate,&entryBuf->modificationTime);
      GetEntryTime(nullptr,&dirEntry->lastAccessDate,&entryBuf->accessTime);

      entryFetched = true;

    }
    currentEntryNr ++; 
  }

  //uint32_t sectorToOpen = 


  if(entryFetched)
  {
    return currentEntryNr;
  }
  else
  {
    return -1;
  }
}

bool DirUnit_c::CheckIfDosNameExists(uint32_t currentDirCluster, char* dosName)
{
  uint32_t currentEntryIdx = 0;
  while(1)
  {

    if (clusterBufferIdx != currentDirCluster)
    {
      disk_p->ReadCluster(clusterBuffer,currentDirCluster);
      clusterBufferIdx = currentDirCluster;
    }

    DirEntry_st* dirEntry = (DirEntry_st*) &clusterBuffer[32*currentEntryIdx];

    if(dirEntry->name[0] == 0)
    {
      /* end of directory*/
      return false;      
    }
    else if(dirEntry->name[0] == 0xE5)
    {
      /* empty entry, skip it*/
    }
    else if(dirEntry->attributes == 0x0F)
    {
      /* long file entry, also skip */
    }
    else
    {
      /* normal entry */
      if(strncmp(dirEntry->name,dosName,11) == 0)
      {
        return true;
      }

    }

    currentEntryIdx ++;

    if(currentEntryIdx >= NoOfEntriesPerCluster())
    {
      currentDirCluster = disk_p->fatUnit.GetNextCluster(currentDirCluster);
      currentEntryIdx = 0;
      if(currentDirCluster == CLUSTER_NOT_VALID)
      {
        return false;
      }
    }
  }
}

uint8_t DirUnit_c::GetDosNameChecksum(uint8_t* name)
{
   int i;
   unsigned char sum = 0;

   for (i = 11; i; i--)
      sum = ((sum & 1) << 7) + (sum >> 1) + *name++;

   return sum;
}

void DirUnit_c::FillEntryTime(uint16_t* time,uint16_t* date, SystemTime_st* timeStruct)
{
  if(time != nullptr)
  {
    *time = (timeStruct->Hour << 11) | (timeStruct->Minute << 5) | (timeStruct->Second >> 1);
  }
  *date = ((timeStruct->Year-1980) << 9) | (timeStruct->Month<<5) | (timeStruct->Day);

}

void DirUnit_c::GetEntryTime(uint16_t* time,uint16_t* date, SystemTime_st* timeStruct)
{

  if(time != nullptr)
  {
    timeStruct->Hour = (*time >> 11) & 0x1F;
    timeStruct->Minute = (*time >> 5) & 0x3F;
    timeStruct->Second = (*time<<1 ) & 0x3F;
  }
  else
  {
    timeStruct->Hour = 0;
    timeStruct->Minute = 0;
    timeStruct->Second = 0;
  }

  timeStruct->Year = (*date  >> 9) + 1980;
  timeStruct->Month =  (*date  >> 5) & 0x0F;
  timeStruct->Day =  (*date) & 0x1F;

}

bool DirUnit_c::CreateNewFileEntry(uint32_t currentDirStartCluster,char* name,EntryData_st* entryData,bool newFile)
{
  #if FAT_DEBUG >= 2
    printf("CreateNewFileEntry, filename = %s\n",name);
  #endif

  LfnUnit_c lfn;
  uint8_t wantedEntries = lfn.GetNoOfWantedEntries(name);

  uint8_t noOfConsequtiveFreeEntries = 0;


  int firstFreeEntryIdx = -1;

  uint32_t currentEntryNr = 0;

  while(1)
  {
    uint32_t clusterIdx = currentEntryNr / NoOfEntriesPerCluster();
    uint32_t entryIdx = currentEntryNr % NoOfEntriesPerCluster();

    uint32_t cluster = disk_p->fatUnit.GetClusterFromChain(currentDirStartCluster,clusterIdx);

    if(cluster == CLUSTER_NOT_VALID)
    {
      uint32_t prevCluster = disk_p->fatUnit.GetClusterFromChain(currentDirStartCluster, (currentEntryNr-1) / NoOfEntriesPerCluster());
      uint32_t newCluster = disk_p->fatUnit.GetNextCluster(prevCluster);



      bool res2 = disk_p->fatUnit.GetNewClusters(&newCluster,1,prevCluster);

      #if FAT_DEBUG >= 2
      printf("New cluster %d added\n",newCluster);
      #endif

      if(res2 == false)
      {
        return false;
      }
      cluster = newCluster;

      memset(clusterBuffer,0,disk_p->fatUnit.GetSectorsPerCluster() * 512);
      disk_p->WriteCluster(clusterBuffer,newCluster);
      clusterBufferIdx = cluster;
    }
    
    else if (clusterBufferIdx != cluster)
    {
      disk_p->ReadCluster(clusterBuffer,cluster);
      clusterBufferIdx = cluster;
    }

    DirEntry_st* dirEntry = (DirEntry_st*) &clusterBuffer[32*entryIdx];

    if((dirEntry->name[0] == 0) || (dirEntry->name[0] == 0xE5))
    {
      /* empty entry, */
      if(noOfConsequtiveFreeEntries == 0)
      {
        firstFreeEntryIdx = currentEntryNr;
      }
      noOfConsequtiveFreeEntries++;

      if(noOfConsequtiveFreeEntries == (wantedEntries+1)) /* left at least one empty entry after currently created */
      {
        break;
      }
    }
    else
    {
      noOfConsequtiveFreeEntries = 0;
      firstFreeEntryIdx = -1;
    }

    currentEntryNr++;
  }

  
  /*write entries */
  bool nameUsed = false;
  uint8_t dosNameCheckSum = 0;
  //char* dosName;

  if(wantedEntries == 1)
  {
    lfn.GenerateDosName(name,true);
  }
  else
  {
    do
    {
      lfn.GenerateDosName(name,false);
      nameUsed = CheckIfDosNameExists(currentDirStartCluster,lfn.GetDosName());
    } while(nameUsed);
    dosNameCheckSum = GetDosNameChecksum((uint8_t*)lfn.GetDosName());
  }

  entryData->entryLocation.entryStartIdx = firstFreeEntryIdx;
  entryData->entryLocation.entryLength = wantedEntries;
  entryData->entryLocation.dirStartCluster = currentDirStartCluster;
  strncpy(entryData->name,name,MAX_FILE_NAME);
  entryData->name[MAX_FILE_NAME-1] = 0;


  currentEntryNr = firstFreeEntryIdx;

  #if FAT_DEBUG >= 2
    printf("Entry start at idx=%d, len = %d\n",firstFreeEntryIdx,wantedEntries);
  #endif

  for(int idx=0;idx<wantedEntries;idx++)
  {
    uint32_t clusterIdx = currentEntryNr / NoOfEntriesPerCluster();
    uint32_t entryIdx = currentEntryNr % NoOfEntriesPerCluster();

    uint32_t cluster = disk_p->fatUnit.GetClusterFromChain(currentDirStartCluster,clusterIdx);

    if(cluster == CLUSTER_NOT_VALID)
    {
      /* should never happen */ 
      return false;
    }
    else if (clusterBufferIdx != cluster)
    {
      disk_p->ReadCluster(clusterBuffer,cluster);
      clusterBufferIdx = cluster;
    }

    DirEntry_st* dirEntry = (DirEntry_st*) &clusterBuffer[32*entryIdx];
    if(idx == wantedEntries-1)
    {
      /* DOS entry */
      memcpy(dirEntry->name,lfn.GetDosName(),11);
      dirEntry->attributes = entryData->attributes;
      dirEntry->clusterH = entryData->firstCluster>>16;
      dirEntry->clusterL = entryData->firstCluster & 0xFFFF;
      dirEntry->size = entryData->fileSize;
      dirEntry->type = 0;

      SystemTime_st actTime;
      TimeUnit_c::GetSystemTime(&actTime);

      if(newFile)
      {
        FillEntryTime(&dirEntry->creationTime,&dirEntry->creationDate,&actTime);
      }
      else
      {
        FillEntryTime(&dirEntry->creationTime,&dirEntry->creationDate,&entryData->creationTime);
      }
      FillEntryTime(&dirEntry->modificationTime,&dirEntry->modificationDate,&actTime);
      FillEntryTime(nullptr,&dirEntry->lastAccessDate,&actTime);


      dirEntry->creationTimeMs = 0;
    }
    else
    {
      /* LFN entry */
      uint8_t pos = wantedEntries - idx -1;
      dirEntry->raw[0] = pos;
      if(idx == 0) { dirEntry->raw[0] |= 0x40; }

      for(int j=0;j<13;j++)
      {
        dirEntry->raw[lfdPositions[j]] = name[ (pos-1)*13 + j];
        dirEntry->raw[lfdPositions[j]+1] = 0;
      }
      dirEntry->attributes = 0x0F;
      dirEntry->type = 0;
      dirEntry->raw[0x0D] = dosNameCheckSum;
      dirEntry->clusterL = 0;
    }

    currentEntryNr ++;

    if((entryIdx+1) >= NoOfEntriesPerCluster())
    {
      disk_p->WriteCluster(clusterBuffer,cluster);
    }
    else if(idx == wantedEntries-1)
    {
      disk_p->WriteCluster(clusterBuffer,cluster);
    }
  }

  return true;
}
bool DirUnit_c::RemoveFileEntry(uint32_t currentDirStartCluster,EntryData_st* entryData)
{
  #if FAT_DEBUG >= 2
    printf("RemoveFileEntry\n");
  #endif
  /* mark entries as free */

  uint32_t entryNr = entryData->entryLocation.entryStartIdx;
  
  for(int i=0;i<entryData->entryLocation.entryLength;i++)
  {
    uint32_t clusterIdx =  entryNr/ NoOfEntriesPerCluster();
    uint32_t entryIdx = entryNr % NoOfEntriesPerCluster();

    uint32_t cluster = disk_p->fatUnit.GetClusterFromChain(currentDirStartCluster,clusterIdx);

    if(cluster == CLUSTER_NOT_VALID)
    {
      /* should never happen */ 
      return false;
    }
    else if (clusterBufferIdx != cluster)
    {
      disk_p->ReadCluster(clusterBuffer,cluster);
      clusterBufferIdx = cluster;
    }

    DirEntry_st* dirEntry = (DirEntry_st*) &clusterBuffer[32*entryIdx];

    dirEntry->raw[0] = 0xE5;

    if(entryNr+1  >= (disk_p->fatUnit.GetSectorsPerCluster()*16))
    {
      disk_p->WriteCluster(clusterBuffer,cluster);
    }
    else if(i == entryData->entryLocation.entryLength -1)
    {
      disk_p->WriteCluster(clusterBuffer,cluster);
    }
    entryNr++;
  }


  /* search last valid entry */

  uint32_t lastUsedIdx = 0;

  entryNr = 0; 

  while(1)
  {
    uint32_t clusterIdx =  entryNr/ NoOfEntriesPerCluster();
    uint32_t entryIdx = entryNr % NoOfEntriesPerCluster();

    uint32_t cluster = disk_p->fatUnit.GetClusterFromChain(currentDirStartCluster,clusterIdx);

    if(cluster == CLUSTER_NOT_VALID)
    {       
      break;
    }
    else if (clusterBufferIdx != cluster)
    {
      disk_p->ReadCluster(clusterBuffer,cluster);
      clusterBufferIdx = cluster;
    }

    DirEntry_st* dirEntry = (DirEntry_st*) &clusterBuffer[32*entryIdx];

    if(dirEntry->name[0] == 0)
    {
      /* end of directory*/      
      break;      
    }
    else if(dirEntry->name[0] == 0xE5)
    {
      /* skip */
    }
    else
    { 
      lastUsedIdx = entryNr;      
    }
    entryNr++;    
  }

  uint32_t clusterIdx =  lastUsedIdx + 1 / NoOfEntriesPerCluster();
  uint32_t cluster = disk_p->fatUnit.GetClusterFromChain(currentDirStartCluster,clusterIdx);

  if(cluster == CLUSTER_NOT_VALID)
  {
    /* should never happen */ 
    return false;
  }

  uint32_t tailCluster;

  if(lastUsedIdx == NoOfEntriesPerCluster()-1 )
  {
    /* last used entry is last in cluster*/

    memset(clusterBuffer,0,disk_p->fatUnit.GetSectorsPerCluster()*512);
    disk_p->WriteCluster(clusterBuffer,cluster);
    clusterBufferIdx = cluster;
    tailCluster = disk_p->fatUnit.GetNextCluster(cluster);

  }
  else
  {
    for(int idx = lastUsedIdx+1; idx < NoOfEntriesPerCluster();idx++)
    {
      DirEntry_st* dirEntry = (DirEntry_st*) &clusterBuffer[32*idx];
      memset(dirEntry->raw,0,32);
    }
    disk_p->WriteCluster(clusterBuffer,cluster);
    clusterBufferIdx = cluster;
    tailCluster = disk_p->fatUnit.GetNextCluster(cluster);
  }

  if(tailCluster != CLUSTER_NOT_VALID)
  {
    disk_p->fatUnit.ReleaseClusters(tailCluster,CLUSTER_NOT_VALID);
  }

  /* remove entries */

  return true;
}

uint32_t DirUnit_c::NoOfEntriesPerCluster(void)
{
  return (disk_p->fatUnit.GetSectorsPerCluster()*16);
}

bool DirUnit_c::UpdateFileEntry(EntryData_st* entryData)
{

  uint16_t entryNr = entryData->entryLocation.entryStartIdx + entryData->entryLocation.entryLength -1;

  uint32_t clusterIdx =  entryNr/ NoOfEntriesPerCluster();
  uint32_t entryIdx = entryNr % NoOfEntriesPerCluster();

  uint32_t cluster = disk_p->fatUnit.GetClusterFromChain(entryData->entryLocation.dirStartCluster,clusterIdx);

  #if FAT_DEBUG >= 2
    printf("UpdateFileEntry name =%s, cluster=%d, idx=%d/%d\n",entryData->name,cluster,clusterIdx,entryIdx);
  #endif

  if(cluster == CLUSTER_NOT_VALID)
  {
  /* should never happen */ 
  return false;
  }
  else if (clusterBufferIdx != cluster)
  {
  disk_p->ReadCluster(clusterBuffer,cluster);
  clusterBufferIdx = cluster;
  }

  DirEntry_st* dirEntry = (DirEntry_st*) &clusterBuffer[32*entryIdx];

  dirEntry->clusterH = entryData->firstCluster >>16;
  dirEntry->clusterL = entryData->firstCluster & 0xFFFF;
  dirEntry->size = entryData ->fileSize;

  SystemTime_st actTime;
  TimeUnit_c::GetSystemTime(&actTime);

  FillEntryTime(&dirEntry->modificationTime,&dirEntry->modificationDate,&actTime);
  FillEntryTime(nullptr,&dirEntry->lastAccessDate,&actTime);

  disk_p->WriteCluster(clusterBuffer,cluster);

  return true;
}

uint32_t DirUnit_c::CreateDirectory(uint32_t workingDirCluster,  char* dirName )
{
 /* get new cluster for new directory */

  uint32_t newCluster;
  bool res =  disk_p->fatUnit.GetNewClusters(&newCluster,1,CLUSTER_NOT_VALID);

  if(res == false)
  {
    return CLUSTER_NOT_VALID;
  }

  /* get space for new entries chain in parent directory and write new entries */

  EntryData_st newEntryData;

  newEntryData.firstCluster = newCluster;
  newEntryData.attributes = FF_FAT_ATTR_DIR;
  newEntryData.fileSize = 0;

  res = CreateNewFileEntry(workingDirCluster,dirName,&newEntryData,true);

  if(res == false)
  {
    disk_p->fatUnit.ReleaseClusters(newCluster,CLUSTER_NOT_VALID);
    return CLUSTER_NOT_VALID;
  }

  /* prepare new directory cluster , dot, dotdot entries etc */

  memset(clusterBuffer,0,disk_p->fatUnit.GetSectorsPerCluster() * 512);

  SystemTime_st actTime;
  TimeUnit_c::GetSystemTime(&actTime);

  DirEntry_st* dotEntry = (DirEntry_st*)(clusterBuffer);
  DirEntry_st* dotdotEntry = (DirEntry_st*)(clusterBuffer+32);

  dotEntry->attributes = FF_FAT_ATTR_DIR;
  FillEntryTime(&dotEntry->creationTime,&dotEntry->creationDate,&actTime);
  FillEntryTime(&dotEntry->modificationTime,&dotEntry->modificationDate,&actTime);
  FillEntryTime(nullptr,&dotEntry->lastAccessDate,&actTime);
  dotEntry->size = 0;
  dotEntry->clusterH = newCluster>>16;
  dotEntry->clusterL = newCluster & 0xFFFF;
  memset(dotEntry->name,' ',11);
  dotEntry->name[0] = '.';
     

  dotdotEntry->attributes = FF_FAT_ATTR_DIR;
  FillEntryTime(&dotdotEntry->creationTime,&dotdotEntry->creationDate,&actTime);
  FillEntryTime(&dotdotEntry->modificationTime,&dotdotEntry->modificationDate,&actTime);
  FillEntryTime(nullptr,&dotdotEntry->lastAccessDate,&actTime);
  dotdotEntry->size = 0;
  dotdotEntry->clusterH = workingDirCluster>>16;
  dotdotEntry->clusterL = workingDirCluster & 0xFFFF;
  memset(dotdotEntry->name,' ',11);
  dotdotEntry->name[0] = '.';
  dotdotEntry->name[1] = '.';

  disk_p->WriteCluster(clusterBuffer,newCluster);
  clusterBufferIdx = newCluster;

  return newCluster;

}

bool DirUnit_c::MkDir( char *dirPath )
{
  #if FAT_DEBUG >= 1
    printf("MkDir %s\n",dirPath);
  #endif
  char* separator = strrchr(dirPath,'/');

  char* dirName = separator+1;
  *separator = 0;

  uint32_t workingDirCluster = GetDirStartCluster(dirPath,true);

  if(workingDirCluster == CLUSTER_NOT_VALID)
  {
    return false; /* parent directory not exists */
  }

  /* check if dir exists */


  EntryData_st* entryData =  FindEntry2(dirName, workingDirCluster);
  if(entryData!= nullptr)
  {
    delete entryData;
    return false;
  }

  uint32_t newDirCuster = CreateDirectory(workingDirCluster,dirName);

 
  return (CLUSTER_NOT_VALID != newDirCuster);

}
bool DirUnit_c::RmDir( char *dirPath )
{
  #if FAT_DEBUG >= 1
    printf("RmDir %s\n",dirPath);
  #endif
  char* separator = strrchr(dirPath,'/');

  char* dirName = separator+1;
  *separator = 0;

  uint32_t workingDirCluster = GetDirStartCluster(dirPath,false);

  if(workingDirCluster == CLUSTER_NOT_VALID)
  {
    return false; /* parent directory not exists */
  }

  /* check if directory exists */

  EntryData_st* entryData =  FindEntry2(dirName, workingDirCluster);
  if(entryData== nullptr)
  {    
    return false;
  }

  /* check if directory empty */
  EntryData_st* entryData2 = new EntryData_st;
  int actEntryIdx = 0;
  uint32_t currentDirCluster = entryData->firstCluster;
  bool dirEmpty = true;
  while(1)
  {
    actEntryIdx = GetNextEntry(entryData2,currentDirCluster,actEntryIdx);
    if(actEntryIdx >= 0)
    {
      if(strcmp(entryData2->name,".") == 0) {}
      else if(strcmp(entryData2->name,"..") == 0) {}
      else
      {
        dirEmpty = false;
        break;
      }
    }
    else
    {
      break;
    }

  }
  delete entryData2;

  if(dirEmpty == false)
  {
    delete entryData;
    return false;
  }

  /* release directory clusters */

  disk_p->fatUnit.ReleaseClusters(entryData->firstCluster,CLUSTER_NOT_VALID);

  /* release directory entry */

  RemoveFileEntry(workingDirCluster,entryData);

  delete entryData;
  
  return true;
}

bool DirUnit_c::RemoveFile( char *filePath )
{
  #if FAT_DEBUG >= 1
    printf("RemoveFile %s\n",filePath);
  #endif
  char* separator = strrchr(filePath,'/');

  char* fileName = separator+1;
  *separator = 0;

  uint32_t workingDirCluster = GetDirStartCluster(filePath,false);

  if(workingDirCluster == CLUSTER_NOT_VALID)
  {
    return false; /* parent directory not exists */
  }

  /* check if directory exists */

  EntryData_st* entryData =  FindEntry2(fileName, workingDirCluster);
  if(entryData== nullptr)
  {    
    return false;
  }

  /* release directory clusters */

  disk_p->fatUnit.ReleaseClusters(entryData->firstCluster,CLUSTER_NOT_VALID);

  /* release directory entry */

  RemoveFileEntry(workingDirCluster,entryData);

  delete entryData;


  return true;
}

bool DirUnit_c::Rename( char * oldName, char * newName)
{
  #if FAT_DEBUG >= 1
    printf("Rename %s->%s\n",oldName,newName);
  #endif

  /* check if within disk */

  Disk_c* diskNew_p = fileSystem.GetDisk(newName);

  if(disk_p != diskNew_p)
  {
    return false;
  }


  /* check if new name not exists */

  char* separatorNew = strrchr(newName,'/');

  char* fileNameNew = separatorNew+1;
  *separatorNew = 0;

  uint32_t workingDirClusterNew = GetDirStartCluster(newName,true);

  if(workingDirClusterNew == CLUSTER_NOT_VALID)
  {
    return false;
  }

  EntryData_st* entryData =  FindEntry2(fileNameNew, workingDirClusterNew);
  if(entryData != nullptr)
  {    
    delete entryData;
    return false;
  }

  /* check if old name exists */

  char* separatorOld = strrchr(oldName,'/');

  char* fileNameOld = separatorOld+1;
  *separatorOld = 0;

  uint32_t workingDirClusterOld = GetDirStartCluster(oldName,false);

  if(workingDirClusterOld == CLUSTER_NOT_VALID)
  {
    return false;
  }

  entryData =  FindEntry2(fileNameOld, workingDirClusterOld);
  if(entryData == nullptr)
  {    
    return false;
  }

  EntryLocation_st oldLocation = entryData->entryLocation;

  /* create new entry */

  bool res = CreateNewFileEntry(workingDirClusterNew,fileNameNew,entryData,false);

  /* delete old entry */

  if( res == true)
  {
    entryData->entryLocation = oldLocation;
    res = RemoveFileEntry(workingDirClusterOld,entryData);
  }

  if(entryData != nullptr)
  {    
    delete entryData;
  } 

  return res;
}