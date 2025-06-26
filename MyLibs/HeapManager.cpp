#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "HeapManager.hpp"


#define HEAP_OVERWRITE_DETECTION 1
#define BYTES_START 8
#define BYTES_STOP 8
#define BYTE_START 0x55
#define BYTE_STOP 0xCC




 uint16_t defaultAllowedSizes[] = {16,32,64,128,256,1024,2048,4096};
#define DEF_NO_OF_LISTS 8

HeapManager_c::HeapManager_c(void)
{
  memoryInitialized = false;
  allowedSizes = defaultAllowedSizes;
  noOfLists = DEF_NO_OF_LISTS;

}

HeapManager_c::HeapManager_c(uint8_t noOfLists_,uint16_t* listSizes, uint8_t managerId_)
{
  memoryInitialized = false;
  allowedSizes = listSizes;
  noOfLists = noOfLists_;
  allowNullResult = false;
  managerId = managerId_;
}

void* HeapManager_c::Malloc( size_t xWantedSize , int id_)
{

  uint8_t *pvReturn = nullptr;
  uint8_t listIterator=0;
  uint8_t regionIterator=0;
  elementInfo_st* elementInfo;
  UBaseType_t uxSavedInterruptStatus;
  uint8_t bitISRmasc = 0;
  uint8_t fromISR;
  size_t wantedSize = xWantedSize;
  size_t elementPayloadSize;
  size_t elementTotalSize;


  #if STORE_EXTRA_MEMORY_STATS == 1
  uint8_t newAllocation = 0;  
  #endif
/*
  if(xWantedSize > 1000)
  {
    printf("alloc %d bytes, idx=%d, free = %d\n",xWantedSize, managerId,freeSpace);
  }*/


  /* The heap must be initialised before the first call to
  prvPortMalloc(). */
  configASSERT( memoryInitialized );

  if(xWantedSize <= allowedSizes[noOfLists-1])
  {
    if((portNVIC_INT_CTRL_REG & 0x1F)==0)
    { 
      vPortEnterCritical();
      fromISR = 0;
    }
    else
    {
      uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
      bitISRmasc = 0x02;
      fromISR = 1;
      errorCode = 3;
      while(1);
    }
    {

      /*round up to allowed size*/
      while(allowedSizes[listIterator]<xWantedSize)
      {
        listIterator++;
      }

      elementPayloadSize = allowedSizes[listIterator];
      elementTotalSize = elementPayloadSize + sizeof(struct elementInfo_st);

      #if HEAP_OVERWRITE_DETECTION == 1
      elementTotalSize += BYTES_START + BYTES_STOP; 
      #endif

   
      if(memoryList[listIterator].noOfFree>0)
      {
        /*get from this list */
        pvReturn = (uint8_t*)memoryList[listIterator].first;
        pvReturn += sizeof(struct elementInfo_st); /*header shift*/
        #if HEAP_OVERWRITE_DETECTION == 1
        pvReturn += BYTES_START;
        #endif
        elementInfo = memoryList[listIterator].first;
        memoryList[listIterator].first = elementInfo ->next;
        memoryList[listIterator].noOfFree--;
        elementInfo->bitMap |= (0x01|bitISRmasc);
        elementInfo->next = nullptr;
        elementInfo->size = wantedSize;
        #if LOST_ALLOC_TRACE == 1
        elementInfo->key = 0xDEADBEEF;
        #endif
        freeSpace -= elementInfo->size;
        if(freeSpace<minFreeSpace)
        {
          minFreeSpace = freeSpace;
        }

      }
      else
      {
        /*get from free heap */

        /*add header*/

        while((heapRegions[regionIterator].xSizeInBytes < elementTotalSize) && (heapRegions[regionIterator].pucStartAddress != nullptr))
        {
          regionIterator++;
        }

        if(heapRegions[regionIterator].pucStartAddress != nullptr)
        {
          pvReturn = heapRegions[regionIterator].pucStartAddress;
          pvReturn +=  sizeof(struct elementInfo_st);
          elementInfo = (struct elementInfo_st*) (heapRegions[regionIterator].pucStartAddress);
          heapRegions[regionIterator].pucStartAddress += elementTotalSize;
          heapRegions[regionIterator].xSizeInBytes -= elementTotalSize;
          elementInfo->bitMap |= (0x01|bitISRmasc);
          elementInfo->size = wantedSize;
          elementInfo->next = nullptr;
          elementInfo->blockNo = listIterator | managerId<<4;
          memoryList[listIterator].noOfAllocated++;
          freeSpace -= elementTotalSize;
          unallocatedSpace -= elementTotalSize;
          if(freeSpace<minFreeSpace)
          {
            minFreeSpace= freeSpace;
          }
          if(regionIterator == 0)
          {            
            primaryUnallocatedSpace -= elementTotalSize;
          }
          #if STORE_EXTRA_MEMORY_STATS == 1
          elementInfo->nextAlloc = memoryList[listIterator].firstAlloc;
          memoryList[listIterator].firstAlloc = elementInfo;
          #endif

          #if HEAP_OVERWRITE_DETECTION == 1


          for(int i=0; i< BYTES_START ; i++)
          {
            *pvReturn = BYTE_START;
            pvReturn++;
          }

          uint8_t* stopPtr = pvReturn + elementPayloadSize;

          for(int i=0; i< BYTES_STOP ; i++)
          {             
            *stopPtr = BYTE_STOP;
            stopPtr++;
          }
          #endif
        }
        else
        {
          pvReturn = nullptr;
          
        }

        if((pvReturn != nullptr) &&
          (elementInfo->size<sizeof(struct elementInfo_st)))
        {
          elementInfo->size = 8;
        }



        #if STORE_EXTRA_MEMORY_STATS == 1
        newAllocation = 1;
        #endif
      }



      traceMALLOC( pvReturn, xWantedSize );
   //   printf("alloc %X s%d\r\n",elementInfo,xWantedSize);
    }
    if(fromISR == 0)
    {
      vPortExitCritical();
    }
    else
    {
      portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
    }

  }
 /* else
  {
    printf("Alloc oversize, requested %d\r\n",xWantedSize);
  }*/

  if(pvReturn != nullptr)
  {
    #if STORE_MEMORY_ID == 1
    elementInfo-> id = id_;
    #endif

    #if STORE_EXTRA_MEMORY_STATS == 1

    if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
      TaskStatus_t  pxTaskStatus;
      vTaskGetInfo(xTaskGetCurrentTaskHandle(),&pxTaskStatus,pdFALSE,eInvalid);
      elementInfo->taskNumber = pxTaskStatus.xTaskNumber;
    }
    else
    {
      elementInfo->taskNumber = 0xFFFFFFFF;
    }


    int statPos = 0;
    uint8_t foundSize = 0;
    for(int i = EXTRA_MEMORY_SIZE_DEPTH-1; i>=0; i--)
    {
      if(extraMemStats[i].size == wantedSize)
      {
        statPos = i;
        foundSize = 1;
        break;
      }
      if(extraMemStats[i].size > wantedSize)
      {
        statPos = i+1;
        break;
      }
    }

    if(statPos < EXTRA_MEMORY_SIZE_DEPTH)
    {
      if(foundSize == 0)
      {
        for(int i = EXTRA_MEMORY_SIZE_DEPTH-2; i>=statPos; i--)
        {
          extraMemStats[i+1] = extraMemStats[i];
        }
        extraMemStats[statPos].allocated = 0;
        extraMemStats[statPos].requests = 0;
        extraMemStats[statPos].size = wantedSize;
      }
      extraMemStats[statPos].allocated += newAllocation;
      extraMemStats[statPos].requests++;
    }

    #endif
  }
  else
  {

    #if( configUSE_MALLOC_FAILED_HOOK == 1 )
    extern void vApplicationMallocFailedHook( void );
    vApplicationMallocFailedHook();

    #endif

    if(allowNullResult == false)
    {

      while(1);
    }
  }
  /*printf("alloc %X %d\n",pvReturn,xWantedSize);*/

  return (void*)pvReturn;
}

void HeapManager_c::Free( void *pv )
{
 /*printf("free %X \n",pv);*/
  uint8_t *puc = ( uint8_t * ) pv;
  uint8_t listIterator=0;
  struct elementInfo_st* elementInfo;
  UBaseType_t uxSavedInterruptStatus;
  uint8_t formISR;



  if( puc != nullptr )
  {
    #if HEAP_OVERWRITE_DETECTION == 1

    /* check stop bytes */

    uint8_t* stopPtr = puc;
    puc -= BYTES_START;

    /* check start bytes */

    for(int i=0;i<BYTES_START;i++)
    {
      if(puc[i] != BYTE_START)
      {
        printf("start bytes mismath\n");
      }
    }

    #endif

    puc -= sizeof(struct elementInfo_st); /*restore header*/
    elementInfo = (struct elementInfo_st*) (puc);


/*
    while(allowedSizes[listIterator]<(elementInfo->size-sizeof(struct elementInfo_st)))
    {
      listIterator++;
      if(listIterator>= noOfLists)
      {
        break;
      }
    }*/
    listIterator = elementInfo->blockNo & 0x0F;

    if(listIterator >= noOfLists)
    {
      while(1);
    }

    size_t elementPayloadSize = allowedSizes[listIterator];
    size_t elementTotalSize = elementPayloadSize + sizeof(struct elementInfo_st);

    #if HEAP_OVERWRITE_DETECTION == 1
    stopPtr += elementPayloadSize;
    for(int i=0;i<BYTES_STOP;i++)
    {
      if(stopPtr[i] != BYTE_STOP)
      {
        printf("stop bytes mismath\n");
      }
    }
    #endif
    
    if((portNVIC_INT_CTRL_REG & 0x1F)==0)
    {
      vPortEnterCritical();
      formISR = 0;
    }
    else
    {
      uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
      formISR = 1;
      errorCode = 3;
      while(1);
    }

    if((elementInfo->bitMap &0x01) == 0)
    {
      if( elementInfo->bitMap &0x02)
      {
        errorCode = 1;
      }
      else
      {
        errorCode = 2;
      }
      while(1);
    }

    elementInfo->next = memoryList[listIterator].first;
    elementInfo->bitMap =0;
    memoryList[listIterator].first = elementInfo;      
    memoryList[listIterator].noOfFree++;

    freeSpace += elementTotalSize;

    #if STORE_MEMORY_ID == 1
    elementInfo-> id = -1;
    #endif


  #if STORE_EXTRA_MEMORY_STATS == 1
  for(int i = EXTRA_MEMORY_SIZE_DEPTH-1; i>=0; i--)
  {
    
    if(extraMemStats[i].size ==  elementInfo->size)
    {
      extraMemStats[i].releases++;
      break;
    }
  }

  //elementInfo->allocatingTask = NULL;

  #endif

    if(formISR == 0)
    {
      vPortExitCritical();
    }
    else
    {
      portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
    }
//printf("free %X, s %d\r\n",elementInfo,elementInfo->size);
  
  }
}

uint8_t HeapManager_c::GetManagerId(void* pv)
{
  struct elementInfo_st* elementInfo;
  if( pv != nullptr )
  {
    uint8_t *puc = ( uint8_t * ) pv;
    puc -= sizeof(struct elementInfo_st); /*restore header*/
    #if HEAP_OVERWRITE_DETECTION == 1
    puc -= BYTES_START;
    #endif
    elementInfo = (struct elementInfo_st*) (puc);
    return  elementInfo->blockNo>>4;
  }
  return 0xFF;
}

#if STORE_MEMORY_ID == 1
int HeapManager_c::GetId(void* pv)
{
  struct elementInfo_st* elementInfo;
  if( pv != nullptr )
  {
    uint8_t *puc = ( uint8_t * ) pv;
    puc -= sizeof(struct elementInfo_st); /*restore header*/
    elementInfo = (struct elementInfo_st*) (puc);
    return  elementInfo->id;
  }
  return 0;
}
void HeapManager_c::SetId(void* pv, int id)
{
  struct elementInfo_st* elementInfo;
  if( pv != nullptr )
  {
    uint8_t *puc = ( uint8_t * ) pv;
    puc -= sizeof(struct elementInfo_st); /*restore header*/
    elementInfo = (struct elementInfo_st*) (puc);
    elementInfo->id = id;
  }
}
#endif

void HeapManager_c::DefineHeapRegions( HeapRegion_t * const pxHeapRegions  )
{
  HeapRegion_t *pxHeapRegion;
  uint8_t i;

  //printf("size= %u\r\n",sizeof(struct memData_st));
  /* Can only call once! */
  configASSERT( memoryInitialized == false );

  freeSpace = 0;
  minFreeSpace = 0;
  unallocatedSpace = 0;
  primaryUnallocatedSpace = 0;
  errorCode = 0;

  heapRegions = pxHeapRegions;
  for(i=0;i<noOfLists;i++)
  {
    memoryList[i].first = nullptr;
    memoryList[i].noOfFree = 0;
    memoryList[i].noOfAllocated = 0;
    #if STORE_EXTRA_MEMORY_STATS == 1
    memoryList[i].firstAlloc = nullptr;
    #endif
  }

  i=0;

  primaryUnallocatedSpace = heapRegions[0].xSizeInBytes;;

  while((heapRegions[i].pucStartAddress != nullptr))
  {
    freeSpace += heapRegions[i].xSizeInBytes;
    i++;
  }
  minFreeSpace = freeSpace;
  unallocatedSpace = freeSpace;

  #if STORE_EXTRA_MEMORY_STATS == 1
  memset(extraMemStats,0,sizeof(struct ExtraMemStats_st) * EXTRA_MEMORY_SIZE_DEPTH);
  #endif
  memoryInitialized = true;
}

/*-----------------------------------------------------------*/

size_t HeapManager_c::GetFreeHeapSize( void )
{
	return freeSpace;
}
/*-----------------------------------------------------------*/

size_t HeapManager_c::GetMinimumEverFreeHeapSize( void )
{
	return minFreeSpace;
}

size_t HeapManager_c::GetUnallocatedHeapSize( void )
{
	return unallocatedSpace;
}
size_t HeapManager_c::GetPrimaryUnallocatedHeapSize( void )
{
	return primaryUnallocatedSpace;
}
/*-----------------------------------------------------------*/
void HeapManager_c::GetHeapListStats(uint16_t* noOfAllocated_p, uint16_t* noOfFree_p,uint16_t* size,uint8_t listIndex)
{
  if(listIndex<noOfLists)
  {
    *noOfAllocated_p = memoryList[listIndex].noOfAllocated;
    *noOfFree_p = memoryList[listIndex].noOfFree;
    *size = allowedSizes[listIndex];
  }
  else
  {
    *noOfAllocated_p = 0;
    *noOfFree_p = 0;
    *size = 0;
  }
}

#if STORE_EXTRA_MEMORY_STATS == 1

void HeapManager_c::GetHeapLargestRequests(uint16_t* noOfRequests, uint16_t* noOfReleases, uint16_t* noOfAllocated, uint16_t* size, uint8_t idx)
{ 
  if(idx < EXTRA_MEMORY_SIZE_DEPTH)
  {
    *noOfRequests = extraMemStats[idx].requests;
    *noOfReleases = extraMemStats[idx].releases;
    *noOfAllocated = extraMemStats[idx].allocated;
    *size = extraMemStats[idx].size;
  }
  else
  {
    *noOfRequests = 0;
    *noOfReleases = 0;
    *noOfAllocated = 0;
    *size = 0;
  }
}

void HeapManager_c::GetHeapAllocationDetails(void** helpPtr, uint16_t* size, UBaseType_t* allocatingTaskNumber, uint8_t* state, int* id, uint16_t idx,  uint16_t minSize, uint16_t maxSize)
{
  elementInfo_st* act = (elementInfo_st*) *helpPtr;

  if(act == nullptr)
  {
    act = memoryList[idx].firstAlloc;
  }

  while(act != nullptr)
  {
    if((act->size >= minSize) && (act->size <= maxSize))
    {
      *state = act->bitMap;
      *size = act->size;
      *allocatingTaskNumber = act->taskNumber;
      *helpPtr = act->nextAlloc;
      #if STORE_MEMORY_ID == 1
      *id = act->id;
      #endif
      return;
    }
    act = act->nextAlloc;    
  }
  *helpPtr = act;

}


#endif