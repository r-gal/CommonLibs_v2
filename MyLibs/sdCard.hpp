#ifndef SDCARD_H
#define SDCARD_H

#include "CommonDef.hpp"

#include "FileSystem.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#define SD_WP_Pin GPIO_PIN_6
#define SD_WP_GPIO_Port GPIOC
#define SD_CD_Pin GPIO_PIN_7
#define SD_CD_GPIO_Port GPIOC

#define sdSECTOR_SIZE                512UL

/* DMA constants. */
#define SD_DMAx_Tx_CHANNEL                   DMA_CHANNEL_4
#define SD_DMAx_Rx_CHANNEL                   DMA_CHANNEL_4
#define SD_DMAx_Tx_STREAM                    DMA2_Stream6
#define SD_DMAx_Rx_STREAM                    DMA2_Stream3
#define SD_DMAx_Tx_IRQn                      DMA2_Stream6_IRQn
#define SD_DMAx_Rx_IRQn                      DMA2_Stream3_IRQn
#define __DMAx_TxRx_CLK_ENABLE               __DMA2_CLK_ENABLE
#define configSDIO_DMA_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY )

#define SD_CARD_PATH "/"

#define USE_OLD_FILESYSTEM 0

union sdCardResp_u
{
  uint32_t regVal;
  struct 
  {
    unsigned int spare:9;
    unsigned int state:4;
    unsigned int rest:19;
  };
};

enum SDIO_COMMAND_RESP_et
{
  SDIO_COMMAND_RESP_NO_WAIT = 0,
  SDIO_COMMAND_RESP_SHORT = (0b01 << 6),
  SDIO_COMMAND_RESP_LONG = (0b11 << 6)
};



class SdCard_c : public FileSystemInterface_c
{
  enum cardStatus_et
  {
    SD_Init, 
    SD_OK,
    SD_Failed,
    SD_NoCard,
    SD_DebouncingStart,
    SD_Debouncing
  } status;

  SD_HandleTypeDef hsd;
  EXTI_HandleTypeDef hexti; 
  #if USE_OLD_FILESYSTEM == 1
  FF_Disk_t *pxDisk;
  #endif
  Disk_c* disk_p;

  TimerHandle_t debouncingTimer;
  SemaphoreHandle_t multiUserSemaphore;

  SemaphoreHandle_t xSDCardSemaphore;

  void InitCardDetect(void);
  static SdCard_c* ownPtr;

  bool CardInserted(void);

  bool MountCard(void);
  void DemountCard(void);

  void DeleteDisk(void);

  public:

  int32_t Write( uint8_t *dataBuffer, uint32_t startSectorNumber, uint32_t sectorsCount);
  int32_t Read( uint8_t *dataBuffer, uint32_t startSectorNumber, uint32_t sectorsCount) ;

  SdCard_c(void);
  static SdCard_c* GetOwnPtr(void) { return ownPtr; }
  SD_HandleTypeDef* GetHsdHandle(void) { return &hsd; }

  cardStatus_et GetStatus(void) { return status; }
  void SetStatus(cardStatus_et newStatus) { status = newStatus; }

  void CardDetectEvent(void);    /* called from TASK after signal reception */
  void CardDetectEventISR(void); /* called from EXTI interrupt */

  void GiveSemephoreISR(void);
  void TakeSemaphore(void);

  bool WaitForProgComplete(void);

#if USE_OLD_FILESYSTEM == 1
  static int32_t ReadSDIO( uint8_t *pucDestination,
                           uint32_t ulSectorNumber,
                           uint32_t ulSectorCount,
                           FF_Disk_t *pxDisk );

  static int32_t WriteSDIO( uint8_t *pucSource,
                            uint32_t ulSectorNumber,
                            uint32_t ulSectorCount,
                            FF_Disk_t *pxDisk );

  static int32_t ReadSDIO_DMA( uint8_t *pucDestination,
                           uint32_t ulSectorNumber,
                           uint32_t ulSectorCount,
                           FF_Disk_t *pxDisk );

  static int32_t WriteSDIO_DMA( uint8_t *pucSource,
                            uint32_t ulSectorNumber,
                            uint32_t ulSectorCount,
                            FF_Disk_t *pxDisk );
 
   static int32_t WriteSDIO_DMA_NO_HAL( uint8_t *pucSource,
                            uint32_t ulSectorNumber,
                            uint32_t ulSectorCount,
                            FF_Disk_t *pxDisk ); 
  #endif
   static uint32_t sendSDIOCommand(uint32_t command,uint32_t arg,SDIO_COMMAND_RESP_et resp);

  void Init(void);




};
/**************************** IRQ HANDLERS ***************************/
#ifdef __cplusplus
 extern "C" {
#endif

void EXTI9_5_IRQHandler( void );
void DMA2_Stream6_IRQHandler( void );
void DMA2_Stream3_IRQHandler( void );
void SDIO_IRQHandler( void );

#ifdef __cplusplus
}
#endif

#endif