#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GeneralConfig.h"

#if CONF_USE_SDCARD == 1

#include "SignalList.hpp"
/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "sdCard_stm32h7xx.hpp"

extern SD_HandleTypeDef SD_CARD_HSD ;


extern FileSystem_c fileSystem;


Sig_c cardDetectEventSignal(SignalLayer_c::SIGNO_SDIO_CardDetectEvent,SignalLayer_c::HANDLE_CTRL);
SdCard_c* SdCard_c::ownPtr = nullptr;

SdCard_c::SdCard_c(void)
{
  #if USE_OLD_FILESYSTEM == 1
  pxDisk = nullptr;
  #endif
  status = SD_Init;
  ownPtr = this;
  multiUserSemaphore = nullptr;
  xSDCardSemaphore = nullptr;
  disk_p = nullptr;
}




void SdCard_c::Init(void)
{



 

  InitCardDetect();

  if(CardInserted())
  {
    if(MountCard() == true)
    {
      SetStatus(SD_OK);
    }
    else
    {
      SetStatus(SD_Failed);
    }
  }
  else
  {
    //DemountCard();
    SetStatus(SD_NoCard);
  }
}




bool SdCard_c::MountCard(void)
{
  #if DEBUG_SDCARD > 0
  printf("MountCard \n");
  #endif
/*


*/

  
  HAL_StatusTypeDef status = HAL_SD_Init(&SD_CARD_HSD);

  if(status != HAL_OK)
  {
    #if DEBUG_SDCARD > 0
    printf("HAL_SD_Init error (%d)\n",status);
    #endif
    return false;

  }

  status = HAL_SD_ConfigWideBusOperation(&SD_CARD_HSD,SDMMC_BUS_WIDE_4B);

  if(status != HAL_OK)
  {
    #if DEBUG_SDCARD > 0
    printf("HAL_SD_ConfigWideBusOperation error (%d)\n",status);
    #endif
    return false;
  }

  #if DEBUG_SDCARD > 0
  printf("Card info: BlockNbr=%d BlockSize=%d, CardType=%d, LogBlockNbr=%d, LogBlockSize=%d\n",
  SD_CARD_HSD.SdCard.BlockNbr,
  SD_CARD_HSD.SdCard.BlockSize,
  SD_CARD_HSD.SdCard.CardType,
  SD_CARD_HSD.SdCard.LogBlockNbr,
  SD_CARD_HSD.SdCard.LogBlockSize);
  #endif

  //xSDHandle.Init.BusWide = SDIO_BUS_WIDE_4B;
 /* HAL_StatusTypeDef rc;
  hsd.Init.BusWide = SDIO_BUS_WIDE_4B;
  rc = HAL_SD_ConfigWideBusOperation( &hsd, SDIO_BUS_WIDE_4B );

  if( rc != HAL_OK )
  {
      //printf( "HAL_SD_WideBus: %d: %s\n", rc, prvSDCodePrintable( ( uint32_t ) rc ) );
  }
*/
  if(SD_CARD_HSD.SdCard.BlockNbr > 0)
  {
    if( xSDCardSemaphore == nullptr )
    {
        xSDCardSemaphore = xSemaphoreCreateBinary();
    }

    if( multiUserSemaphore == nullptr )
    {
        multiUserSemaphore = xSemaphoreCreateBinary();
    }

    xSemaphoreGive(multiUserSemaphore);
    

    /* card identified */
    if(disk_p != nullptr)
    {
      /* fail */
      //configASSERT(0);
      return false;
    }

    disk_p = new Disk_c;
    disk_p->interface_p = this;
    disk_p->path = (char*)SD_CARD_PATH;
    disk_p->SetAlignMask(Disk_c::ALIGN_32);
    fileSystem.MountDisk(disk_p);
    return true;
  }
  else
  {
    #if DEBUG_SDCARD > 0
    printf("hsd.SdCard.BlockNbr error \n");
    #endif
    return false;
  }


  
}
void SdCard_c::DemountCard(void)
{
  DeleteDisk();

  HAL_StatusTypeDef status = HAL_SD_DeInit(&SD_CARD_HSD);

  printf("DemountCard \n");

}

void SdCard_c::DeleteDisk(void)
{
#if 0
  if(pxDisk != nullptr)
  {
    pxDisk->ulSignature = 0;
    pxDisk->xStatus.bIsInitialised = 0;

    if( pxDisk->pxIOManager != NULL )
    {
      FF_FS_Remove( SD_CARD_PATH);
      if( FF_Mounted( pxDisk->pxIOManager ) != pdFALSE )
      {
        FF_Unmount( pxDisk );
      }
      FF_DeleteIOManager( pxDisk->pxIOManager );
    }
    delete pxDisk;
    pxDisk = nullptr;
  }
# endif
  if(disk_p != nullptr)
  {
    fileSystem.UnmountDisk(disk_p);
    delete disk_p;
    disk_p = nullptr;

  }

}

/**************************** READ / WRITE DMA ***************************/

int32_t SdCard_c::Read( uint8_t *dataBuffer, uint32_t startSectorNumber, uint32_t sectorsCount)
{
  //printf("ReadSDIO_DMA\n");
  uint32_t startTime = TIM2->CNT;
    uint32_t ulISREvents = 0U;
  SdCard_c* p = GetOwnPtr();
  if( ( ( ( size_t ) dataBuffer ) & ( sizeof( size_t ) - 1 ) ) == 0 )
  {
    xSemaphoreTake(p->multiUserSemaphore,portMAX_DELAY );

    actRxTask = xTaskGetCurrentTaskHandle();

    HAL_StatusTypeDef  status = HAL_SD_ReadBlocks_DMA(&SD_CARD_HSD,dataBuffer,startSectorNumber,sectorsCount);

    if(status != HAL_OK)
    {
      #if DEBUG_SDCARD > 0
      printf("FILESYS: read fail %d\n",status);
      #endif

    }
    uint32_t beforeSemaphoreTime = TIM2->CNT;

    //GetOwnPtr()->TakeSemaphore();

    if(xTaskNotifyWait( 0U,                /* ulBitsToClearOnEntry */
                 SDMMC_RX_EVENT, /* ulBitsToClearOnExit */
                 &( ulISREvents ),  /* pulNotificationValue */
                 100000 ) == pdTRUE  )

    {

      uint32_t afterSemaphoreTime = TIM2->CNT;
      //GetOwnPtr()->WaitForProgComplete();
      GetOwnPtr()->SD_CheckStatusWithTimeout(100);
      uint32_t completeTime = TIM2->CNT;

      xSemaphoreGive(p->multiUserSemaphore);

      #if DEBUG_SDCARD > 0
      if(startTime < beforeSemaphoreTime) { beforeSemaphoreTime -= startTime; } else {beforeSemaphoreTime += (0xFFFFFFFF - startTime);}
      if(startTime < afterSemaphoreTime) { afterSemaphoreTime -= startTime; } else {afterSemaphoreTime += (0xFFFFFFFF - startTime);}
      if(startTime < completeTime) { completeTime -= startTime; } else {completeTime += (0xFFFFFFFF - startTime);}
    
      printf("Read %d sectors: Bt=%d, At=%d Et=%d\n",sectorsCount,beforeSemaphoreTime,afterSemaphoreTime,completeTime);
      #endif
      return 0;
    }
    else
    {
      #if DEBUG_SDCARD > 0
      printf("ReadSDIO_DMA, notificaton timeout \n");
      #endif
      return 1;
    }

    
  }
  else
  {
  #if DEBUG_SDCARD > 0
    printf("ReadSDIO_DMA, buffer not aligned \n");
    #endif
    return 1;
  }
  

  
}



int32_t SdCard_c::Write( uint8_t *pucSource,
                          uint32_t ulSectorNumber,
                          uint32_t ulSectorCount )
{
  uint32_t startTime = TIM2->CNT;
  uint32_t ulISREvents = 0U;

  //printf("WriteSDIO_DMA, SecIdx=%d, SecNo=%d\n",ulSectorNumber,ulSectorCount);
  SdCard_c* p = GetOwnPtr();

  if( ( ( ( size_t ) pucSource ) & ( sizeof( size_t ) - 1 ) ) == 0 )
  {
    //printf("SD_CARD state1 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));
    xSemaphoreTake(p->multiUserSemaphore,portMAX_DELAY );

    actTxTask = xTaskGetCurrentTaskHandle();

    if(0)//if(ulSectorCount > 1)
    {
      uint32_t status;   
      uint32_t rca = (uint32_t)((uint32_t)SD_CARD_HSD.SdCard.RelCardAdd);
      /* CMD55 */
      status = p->SendSDIOCommand(SDMMC_CMD_APP_CMD,rca);

      /* CMD23 */
      status = p->SendSDIOCommand(SDMMC_CMD_SET_BLOCK_COUNT,ulSectorCount);

       //p->WaitForProgComplete();
       //p->SD_CheckStatusWithTimeout(1000);
    }

    //printf("call HAL_SD_WriteBlocks_DMA\n");
    HAL_StatusTypeDef  status = HAL_SD_WriteBlocks_DMA(&SD_CARD_HSD,pucSource,ulSectorNumber,ulSectorCount);
    //printf("end HAL_SD_WriteBlocks_DMA\n");
    uint32_t beforeSemaphoreTime = TIM2->CNT;
    //printf("call TakeSemaphore\n");
    //p->TakeSemaphore();

        /* Wait for a new event or a time-out. */
    if(xTaskNotifyWait( 0U,                /* ulBitsToClearOnEntry */
                     SDMMC_TX_EVENT, /* ulBitsToClearOnExit */
                     &( ulISREvents ),  /* pulNotificationValue */
                     100000 ) == pdTRUE  )
    {
      //printf("end TakeSemaphore\n");
      uint32_t afterSemaphoreTime = TIM2->CNT;
      //p->WaitForProgComplete();
      //printf("call SD_CheckStatusWithTimeout\n");
      p->SD_CheckStatusWithTimeout(100);
      //printf("end SD_CheckStatusWithTimeout\n");
      uint32_t completeTime = TIM2->CNT;

      xSemaphoreGive(p->multiUserSemaphore);

      #if DEBUG_SDCARD > 0
      if(startTime < beforeSemaphoreTime) { beforeSemaphoreTime -= startTime; } else {beforeSemaphoreTime += (0xFFFFFFFF - startTime);}
      if(startTime < afterSemaphoreTime) { afterSemaphoreTime -= startTime; } else {afterSemaphoreTime += (0xFFFFFFFF - startTime);}
      if(startTime < completeTime) { completeTime -= startTime; } else {completeTime += (0xFFFFFFFF - startTime);}
   
      printf("Write %d sectors: Bt=%d, At=%d Et=%d\n",ulSectorCount,beforeSemaphoreTime,afterSemaphoreTime,completeTime);
      #endif
      return 0;
    }
    else
    {
      #if DEBUG_SDCARD > 0
      printf("WriteSDIO_DMA, notificaton timeout \n");
      #endif
      return 1;
    }
 

  }
  else
  {
  #if DEBUG_SDCARD > 0
    printf("WriteSDIO_DMA, buffer not aligned \n");
    #endif
    return 1;
  }
  return 0;
}
/* ************************************************/


void SdCard_c::GiveSemephoreISR(void)
{
  BaseType_t xHigherPriorityTaskWoken = 0;
  if( xSDCardSemaphore != NULL )
  {
      xSemaphoreGiveFromISR( xSDCardSemaphore, &xHigherPriorityTaskWoken );
  }
}

void SdCard_c::TakeSemaphore(void)
{
  xSemaphoreTake( xSDCardSemaphore, 1000 );
}

bool SdCard_c::WaitForProgComplete(void)
{
    HAL_SD_CardStateTypeDef cardState;

    do
    {
      cardState = HAL_SD_GetCardState(&SD_CARD_HSD);
      if(cardState == HAL_SD_CARD_READY)
      {
        vTaskDelay(1);

      }
    }
    while(cardState != HAL_SD_CARD_READY);
    return true;

}

uint8_t SdCard_c::BSP_SD_GetCardState(void)
{
  return ((HAL_SD_GetCardState(&SD_CARD_HSD) == HAL_SD_CARD_TRANSFER ) ? SD_TRANSFER_OK : SD_TRANSFER_BUSY);
}

int SdCard_c::SD_CheckStatusWithTimeout(uint32_t timeout)
{
  uint32_t timer = HAL_GetTick();
  /* block until SDIO IP is ready again or a timeout occur */
  while(HAL_GetTick() - timer < timeout)
  {
    if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
    {
      return 0;
    }
  }

  return -1;
}


/**************************** CARD DETECT ***************************/

void SdCard_c::InitCardDetect(void)
{
  
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = SD_DET_A_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SD_DET_A_GPIO_Port, &GPIO_InitStruct);

  EXTI_ConfigTypeDef pExtiConfig;
  pExtiConfig.GPIOSel = EXTI_GPIOC;
  pExtiConfig.Line = EXTI_LINE_6;
  pExtiConfig.Mode = EXTI_MODE_INTERRUPT;
  pExtiConfig.Trigger = EXTI_TRIGGER_RISING_FALLING;

  HAL_EXTI_SetConfigLine(&hexti, &pExtiConfig);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 15, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

bool SdCard_c::CardInserted(void)
{
    /*!< Check GPIO to detect SD */
    if( HAL_GPIO_ReadPin( SD_DET_A_GPIO_Port, SD_DET_A_Pin ) != 0 )
    {
        /* The internal pull-up makes the signal high. */
        return false;
    }
    else
    {
        /* The card will pull the GPIO signal down. */
        return true;
    }
}

void SdCard_c::CardDetectEventISR(void)
{
  if((GetStatus() != SD_Debouncing) && (GetStatus() != SD_DebouncingStart))
  { 
    SetStatus(SD_DebouncingStart);
    cardDetectEventSignal.SendISR();
  }
 
}

void vFunctionDebouncingTimerCallback( TimerHandle_t xTimer )
{
  cardDetectEventSignal.SendISR();
}

void SdCard_c::CardDetectEvent(void)
{
  switch(GetStatus())
  { 
    case SD_Debouncing:
      if(xTimerDelete(debouncingTimer,0) == pdPASS)
      {
        debouncingTimer = nullptr;
      } 
      
      if(CardInserted())
      {
        if(MountCard() == true)
        {
          SetStatus(SD_OK);
        }
        else
        {
          SetStatus(SD_Failed);
        }
      }
      else
      {
        DemountCard();
        SetStatus(SD_NoCard);
      }
      break;
    case SD_DebouncingStart:
      debouncingTimer = xTimerCreate("",pdMS_TO_TICKS(500),pdTRUE,( void * ) 0,vFunctionDebouncingTimerCallback);
      xTimerStart(debouncingTimer,0);
      SetStatus(SD_Debouncing);
      break;
    default:
    break;


  }
}


/**************************** IRQ CALLBACKS ***************************/

#ifdef __cplusplus
 extern "C" {
#endif

void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin )
{  
  if( GPIO_Pin == SD_DET_A_Pin )
  {
    SdCard_c* p = SdCard_c::GetOwnPtr();
    if(p != nullptr)
    {
      p->CardDetectEventISR();
    }
  }
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
  SdCard_c* p = SdCard_c::GetOwnPtr();
  if(p != nullptr)
  {
    //while( __HAL_SD_GET_FLAG(hsd, SDIO_FLAG_TXACT)) {}
    //printf("HAL_SD_TxCpltCallback \n ");
    //p->GiveSemephoreISR();

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR( p->GetTxTaskPid(), SDMMC_TX_EVENT, eSetBits, &( xHigherPriorityTaskWoken ) );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
  }
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
  SdCard_c* p = SdCard_c::GetOwnPtr();
  if(p != nullptr)
  {
    //while( __HAL_SD_GET_FLAG(hsd, SDIO_FLAG_TXACT)) {}
    //printf("HAL_SD_RxCpltCallback \n ");
   // p->GiveSemephoreISR();

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR( p->GetRxTaskPid(), SDMMC_RX_EVENT, eSetBits, &( xHigherPriorityTaskWoken ) );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
  }
}



void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
#if DEBUG_SDCARD > 0
  printf("HAL_SD_ErrorCallback \n ");
  #endif
}

void HAL_SD_AbortCallback(SD_HandleTypeDef *hsd)
{
#if DEBUG_SDCARD > 0
  printf("HAL_SD_AbortCallback \n");
  #endif
}




#ifdef __cplusplus
}
#endif

uint32_t SdCard_c::SendSDIOCommand(uint32_t command,uint32_t arg)
{
  SDMMC_CmdInitTypeDef  sdmmc_cmdinit;
  uint32_t errorstate;

  /* Send */
  sdmmc_cmdinit.Argument         = arg;
  sdmmc_cmdinit.CmdIndex         = command;
  sdmmc_cmdinit.Response         = SDMMC_RESPONSE_SHORT;
  sdmmc_cmdinit.WaitForInterrupt = SDMMC_WAIT_NO;
  sdmmc_cmdinit.CPSM             = SDMMC_CPSM_ENABLE;
  (void)SDMMC_SendCommand(SD_CARD_HSD.Instance, &sdmmc_cmdinit);

  /* Check for error conditions */
  errorstate = SDMMC_GetCmdResp1(SD_CARD_HSD.Instance, command, SDMMC_CMDTIMEOUT);

  return errorstate;

}

#endif

