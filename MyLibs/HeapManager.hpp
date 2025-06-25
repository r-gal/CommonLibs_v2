

#ifndef HEAP_MANAGER_H
#define HEAP_MANAGER_H


#define NO_OF_LISTS 8

#include "FreeRTOS.h"
#include "task.h"


class HeapManager_c
{
   #if STORE_EXTRA_MEMORY_STATS == 1
  struct ExtraMemStats_st
  {
    uint16_t size;
    uint16_t requests;
    uint16_t releases;
    uint16_t allocated;
  } extraMemStats[EXTRA_MEMORY_SIZE_DEPTH];
  #endif
  struct elementInfo_st; 

  struct list_st
  {
   elementInfo_st* first;
   #if STORE_EXTRA_MEMORY_STATS == 1
   elementInfo_st* firstAlloc;
   #endif
   uint16_t noOfFree;
   uint16_t noOfAllocated;
  };

  struct elementInfo_st
  {
    elementInfo_st* next;
    #if STORE_EXTRA_MEMORY_STATS == 1
    elementInfo_st* nextAlloc;    
    UBaseType_t taskNumber;

    #endif

    #if STORE_MEMORY_ID == 1
    int32_t id;
    #endif

    uint16_t size;
    
    uint8_t bitMap;
     /*
     bit 0 - used
     bit 1 - from ISR
     */
    uint8_t blockNo; /* bits 7-4: manager id, bits 3-0: listIdx */
  
  };


  HeapRegion_t * heapRegions;
  uint32_t freeSpace ;
  uint32_t minFreeSpace ;
  uint32_t unallocatedSpace ;
  uint32_t primaryUnallocatedSpace ;
  list_st memoryList[NO_OF_LISTS];
  uint8_t errorCode;

  bool memoryInitialized;
  uint16_t* allowedSizes;
  uint8_t   noOfLists;
  uint8_t   managerId;


  public:

  bool allowNullResult;

  HeapManager_c(void);
  HeapManager_c(uint8_t noOfLists,uint16_t* listSizes,uint8_t managerId);

  void* Malloc( size_t xWantedSize  , int id_);

  void Free( void *pv );

  void DefineHeapRegions( HeapRegion_t * const pxHeapRegions  );


  void GetHeapListStats(uint16_t* noOfAllocated_p, uint16_t* noOfFree_p,uint16_t* size,uint8_t listIndex);
  size_t GetFreeHeapSize( void );
  size_t GetMinimumEverFreeHeapSize( void );
  size_t GetUnallocatedHeapSize( void );
  size_t GetPrimaryUnallocatedHeapSize( void );

  #if STORE_MEMORY_ID == 1
  static int GetId(void* pv);
  static void SetId(void* pv, int id);
  #endif
  static uint8_t GetManagerId(void* pv);



  #if STORE_EXTRA_MEMORY_STATS == 1
  void GetHeapAllocationDetails(void** helpPtr, uint16_t* size, UBaseType_t* allocatingTaskNumber, uint8_t* state, int* id, uint16_t idx,  uint16_t minSize, uint16_t maxSize);
  void GetHeapLargestRequests(uint16_t* noOfRequests, uint16_t* noOfReleases,uint16_t* noOfAllocated, uint16_t* size, uint8_t idx);
  #endif
};


#endif