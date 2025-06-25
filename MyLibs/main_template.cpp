/*********************************************************************
*               SEGGER MICROCONTROLLER GmbH & Co. KG                 *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (c) 2014 - 2017  SEGGER Microcontroller GmbH & Co. KG        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : main.c
Purpose : Generic application start
*/

#include <stdio.h>
#include <stdlib.h>

#pragma fullpath_file  off 

/* Scheduler includes. */
#include "FreeRTOS.h"

#include "task.h"

#include "main.hpp"

#include "CtrlProcess.hpp"


    void * operator new(size_t size) { 
      return (pvPortMalloc(size)); 
    }
    void operator delete(void* wsk) {
      vPortFree(wsk) ;
    }
     void* operator new[](size_t size) {
      return (pvPortMalloc(size));
    }
    void operator delete[](void* ptr) {
      vPortFree(ptr);
    }

static void prvSetupHardware( void );
static void prvSetupHeap( void );


/*********************************************************************
*
*       main()
*
*  Function description
*   Application entry point.
*/
int main(void)
{

  prvSetupHardware();
  prvSetupHeap();

  new CtrlProcess_c(256,tskIDLE_PRIORITY+2,64,signalLayer_c::HANDLE_CTRL);

  vTaskStartScheduler();

  return 0;
}


static void prvSetupHardware( void )
{
  SystemInit();
  NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );

}

static void prvSetupHeap( void )
{
  static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] __attribute__ ((aligned (4)));;

  static HeapRegion_t xHeapRegions[] =
  {
  { ucHeap ,			configTOTAL_HEAP_SIZE },
  { (uint8_t*) SDRAM_START ,	SDRAM_SIZE }, /* DRAM1 */
  { NULL, 0 }
  };

  vPortDefineHeapRegions( xHeapRegions );

}


/*************************** End of file ****************************/
