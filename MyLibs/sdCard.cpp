#include <stdio.h>
#include <stdlib.h>

#include "SignalList.hpp"
#include "CommonDef.hpp"
/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "stm32f4xx_hal_conf.h"
//#include "stm32f4xx_hal_dma.h"
//#include "stm32f4xx_hal_gpio.h"
//#include "stm32f4xx_hal_rcc.h"
//#include "stm32f4xx_hal_sd.h"

#include "sdCard.hpp"

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

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_ENABLE;
  hsd.Init.ClockDiv = 2;

  //HAL_StatusTypeDef status = HAL_SD_Init(&hsd);
  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */

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

void prvSDIO_DMA_Init( SD_HandleTypeDef* hsd )
{
  static DMA_HandleTypeDef xRxDMAHandle;
  static DMA_HandleTypeDef xTxDMAHandle;

  /* Enable DMA2 clocks */
  __DMA2_CLK_ENABLE();

  /* NVIC configuration for SDIO interrupts */
  HAL_NVIC_SetPriority( SDIO_IRQn, configSDIO_DMA_INTERRUPT_PRIORITY, 0 );
  HAL_NVIC_EnableIRQ( SDIO_IRQn );

  /* Configure DMA Rx parameters */
  xRxDMAHandle.Init.Channel = SD_DMAx_Rx_CHANNEL;
  xRxDMAHandle.Init.Direction = DMA_PERIPH_TO_MEMORY;
  /* Peripheral address is fixed (FIFO). */
  xRxDMAHandle.Init.PeriphInc = DMA_PINC_DISABLE;
  /* Memory address increases. */
  xRxDMAHandle.Init.MemInc = DMA_MINC_ENABLE;
  xRxDMAHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  xRxDMAHandle.Init.MemDataAlignment = DMA_PDATAALIGN_WORD;
  /* The peripheral has flow-control. */
  xRxDMAHandle.Init.Mode = DMA_PFCTRL;
  xRxDMAHandle.Init.Priority = DMA_PRIORITY_VERY_HIGH;
  xRxDMAHandle.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  xRxDMAHandle.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  xRxDMAHandle.Init.MemBurst = DMA_MBURST_INC4;
  xRxDMAHandle.Init.PeriphBurst = DMA_PBURST_INC4;

  /* DMA2_Stream3. */
  xRxDMAHandle.Instance = SD_DMAx_Rx_STREAM;

  /* Associate the DMA handle */
  __HAL_LINKDMA( hsd, hdmarx, xRxDMAHandle );

  /* Deinitialize the stream for new transfer */
  HAL_DMA_DeInit( &xRxDMAHandle );

  /* Configure the DMA stream */
  HAL_DMA_Init( &xRxDMAHandle );

  /* Configure DMA Tx parameters */
  xTxDMAHandle.Init.Channel = SD_DMAx_Tx_CHANNEL;
  xTxDMAHandle.Init.Direction = DMA_MEMORY_TO_PERIPH;
  xTxDMAHandle.Init.PeriphInc = DMA_PINC_DISABLE;
  xTxDMAHandle.Init.MemInc = DMA_MINC_ENABLE;
  xTxDMAHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  xTxDMAHandle.Init.MemDataAlignment = DMA_PDATAALIGN_WORD;
  xTxDMAHandle.Init.Mode = DMA_PFCTRL;
  xTxDMAHandle.Init.Priority = DMA_PRIORITY_VERY_HIGH;
  xTxDMAHandle.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  xTxDMAHandle.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  xTxDMAHandle.Init.MemBurst = DMA_MBURST_SINGLE;
  xTxDMAHandle.Init.PeriphBurst = DMA_PBURST_INC4;

  /* DMA2_Stream6. */
  xTxDMAHandle.Instance = SD_DMAx_Tx_STREAM;

  /* Associate the DMA handle */
  __HAL_LINKDMA( hsd, hdmatx, xTxDMAHandle );

  /* Deinitialize the stream for new transfer */
  HAL_DMA_DeInit( &xTxDMAHandle );

  /* Configure the DMA stream */
  HAL_DMA_Init( &xTxDMAHandle );

  /* NVIC configuration for DMA transfer complete interrupt */
  HAL_NVIC_SetPriority( SD_DMAx_Rx_IRQn, configSDIO_DMA_INTERRUPT_PRIORITY + 2, 0 );
  HAL_NVIC_EnableIRQ( SD_DMAx_Rx_IRQn );

  /* NVIC configuration for DMA transfer complete interrupt */
  HAL_NVIC_SetPriority( SD_DMAx_Tx_IRQn, configSDIO_DMA_INTERRUPT_PRIORITY + 2, 0 );
  HAL_NVIC_EnableIRQ( SD_DMAx_Tx_IRQn );
}

void prvSDIO_DMA_DeInit( SD_HandleTypeDef* hsd )
{
  HAL_DMA_DeInit( hsd->hdmarx );
  HAL_DMA_DeInit( hsd->hdmatx );
}

void HAL_SD_MspInit(SD_HandleTypeDef* hsd)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hsd->Instance==SDIO)
  {
  /* USER CODE BEGIN SDIO_MspInit 0 */

  /* USER CODE END SDIO_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_SDIO_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**SDIO GPIO Configuration
    PC8     ------> SDIO_D0
    PC9     ------> SDIO_D1
    PC10     ------> SDIO_D2
    PC11     ------> SDIO_D3
    PC12     ------> SDIO_CK
    PD2     ------> SDIO_CMD
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    prvSDIO_DMA_Init(hsd);

  /* USER CODE BEGIN SDIO_MspInit 1 */

  /* USER CODE END SDIO_MspInit 1 */
  }

}

/**
* @brief SD MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param hsd: SD handle pointer
* @retval None
*/
void HAL_SD_MspDeInit(SD_HandleTypeDef* hsd)
{
  if(hsd->Instance==SDIO)
  {
  /* USER CODE BEGIN SDIO_MspDeInit 0 */

  /* USER CODE END SDIO_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_SDIO_CLK_DISABLE();

    /**SDIO GPIO Configuration
    PC8     ------> SDIO_D0
    PC9     ------> SDIO_D1
    PC10     ------> SDIO_D2
    PC11     ------> SDIO_D3
    PC12     ------> SDIO_CK
    PD2     ------> SDIO_CMD
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12);

    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);

  /* USER CODE BEGIN SDIO_MspDeInit 1 */
    prvSDIO_DMA_DeInit(hsd);
  /* USER CODE END SDIO_MspDeInit 1 */
  }

}


bool SdCard_c::MountCard(void)
{
  #if DEBUG_SDCARD > 0
  printf("MountCard \n");
  #endif
/*


*/
  memset(&hsd.SdCard,0,sizeof(HAL_SD_CardInfoTypeDef));
  
  HAL_StatusTypeDef status = HAL_SD_Init(&hsd);

  if(status != HAL_OK)
  {
    #if DEBUG_SDCARD > 0
    printf("HAL_SD_Init error (%d)\n",status);
    #endif
    return false;

  }

  status = HAL_SD_ConfigWideBusOperation(&hsd,SDIO_BUS_WIDE_4B);

  if(status != HAL_OK)
  {
    #if DEBUG_SDCARD > 0
    printf("HAL_SD_ConfigWideBusOperation error (%d)\n",status);
    #endif
    return false;
  }

  #if DEBUG_SDCARD > 0
  printf("Card info: BlockNbr=%d BlockSize=%d, CardType=%d, LogBlockNbr=%d, LogBlockSize=%d\n",
  hsd.SdCard.BlockNbr,
  hsd.SdCard.BlockSize,
  hsd.SdCard.CardType,
  hsd.SdCard.LogBlockNbr,
  hsd.SdCard.LogBlockSize);
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
  if(hsd.SdCard.BlockNbr > 0)
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

  HAL_StatusTypeDef status = HAL_SD_DeInit(&hsd);

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
/**************************** READ / WRITE ***************************/

int32_t SdCard_c::ReadSDIO( uint8_t *pucDestination,
                         uint32_t ulSectorNumber,
                         uint32_t ulSectorCount,
                         FF_Disk_t *pxDisk )
{
  //printf("ReadSDIO\n");
  GetOwnPtr()->WaitForProgComplete();

  HAL_StatusTypeDef status = HAL_SD_ReadBlocks(&(GetOwnPtr()->hsd),pucDestination,ulSectorNumber,ulSectorCount,20000);
  return FF_ERR_NONE;
}

/*
static union
{
  uint8_t testArray[512];
  uint32_t a;
};
*/

int32_t SdCard_c::WriteSDIO( uint8_t *pucSource,
                          uint32_t ulSectorNumber,
                          uint32_t ulSectorCount,
                          FF_Disk_t *pxDisk )
{
  //printf("WriteSDIO\n");
  GetOwnPtr()->WaitForProgComplete();
  HAL_StatusTypeDef status =  HAL_SD_WriteBlocks(&(GetOwnPtr()->hsd),pucSource,ulSectorNumber,ulSectorCount,20000);
/*
  ReadSDIO(testArray,ulSectorNumber,1,pxDisk);

  for(int i=0;i<512;i++)
  {
    if(pucSource[i] != testArray[i])
    {
      printf("Write check error, src[%d] = %x des[%d] = %X\n",i,pucSource[i],i,testArray[i]);
    }

  }
*/
  return FF_ERR_NONE;
}

/**************************** READ / WRITE DMA ***************************/

int32_t SdCard_c::Read( uint8_t *dataBuffer, uint32_t startSectorNumber, uint32_t sectorsCount)
{
  //printf("ReadSDIO_DMA\n");
  //GetOwnPtr()->WaitForProgComplete();
  SdCard_c* p = GetOwnPtr();
  if( ( ( ( size_t ) dataBuffer ) & ( sizeof( size_t ) - 1 ) ) == 0 )
  {
    xSemaphoreTake(p->multiUserSemaphore,portMAX_DELAY );
    HAL_StatusTypeDef  status = HAL_SD_ReadBlocks_DMA(&(GetOwnPtr()->hsd),dataBuffer,startSectorNumber,sectorsCount);

    if(status != HAL_OK)
    {
      printf("FILESYS: read fail %d\n",status);

    }

    GetOwnPtr()->TakeSemaphore();
    GetOwnPtr()->WaitForProgComplete();

    xSemaphoreGive(p->multiUserSemaphore);
    return FF_ERR_NONE;
  }
  else
  {
  #if DEBUG_SDCARD > 0
    printf("ReadSDIO_DMA, buffer not aligned \n");
    #endif
    return FF_ERR_DEVICE_DRIVER_FAILED;
  }
  

  
}



int32_t SdCard_c::Write( uint8_t *pucSource,
                          uint32_t ulSectorNumber,
                          uint32_t ulSectorCount )
{
  uint32_t startTime = TIM2->CNT;
  //printf("WriteSDIO_DMA, SecIdx=%d, SecNo=%d\n",ulSectorNumber,ulSectorCount);
  SdCard_c* p = GetOwnPtr();
  //p->WaitForProgComplete();
  if( ( ( ( size_t ) pucSource ) & ( sizeof( size_t ) - 1 ) ) == 0 )
  {
    //printf("SD_CARD state1 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));
    xSemaphoreTake(p->multiUserSemaphore,portMAX_DELAY );

    if(ulSectorCount > 1)
    {
      uint32_t status;   
      sdCardResp_u sdCardResp;   
      uint32_t rca = (uint32_t)((uint32_t)p->hsd.SdCard.RelCardAdd);
      /* CMD55 */
      status = sendSDIOCommand(SDMMC_CMD_APP_CMD,rca,SDIO_COMMAND_RESP_SHORT);
       sdCardResp.regVal = SDIO->RESP1;
      /* CMD23 */
      status = sendSDIOCommand(SDMMC_CMD_SET_BLOCK_COUNT,ulSectorCount,SDIO_COMMAND_RESP_SHORT);
       sdCardResp.regVal = SDIO->RESP1;
       p->WaitForProgComplete();
    }

    HAL_StatusTypeDef  status = HAL_SD_WriteBlocks_DMA(&(p->hsd),pucSource,ulSectorNumber,ulSectorCount);

    uint32_t beforeSemaphoreTime = TIM2->CNT;
    p->TakeSemaphore();
    uint32_t afterSemaphoreTime = TIM2->CNT;
    p->WaitForProgComplete();
    uint32_t completeTime = TIM2->CNT;

    xSemaphoreGive(p->multiUserSemaphore);

    if(startTime < beforeSemaphoreTime) { beforeSemaphoreTime -= startTime; } else {beforeSemaphoreTime += (0xFFFFFFFF - startTime);}
    if(startTime < afterSemaphoreTime) { afterSemaphoreTime -= startTime; } else {afterSemaphoreTime += (0xFFFFFFFF - startTime);}
    if(startTime < completeTime) { completeTime -= startTime; } else {completeTime += (0xFFFFFFFF - startTime);}
    #if DEBUG_SDCARD > 0
    printf("Write %d sectors: Bt=%d, At=%d Et=%d\n",ulSectorCount,beforeSemaphoreTime,afterSemaphoreTime,completeTime);
    #endif

    //printf("SD_CARD state2 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));

    //p->WaitForProgComplete();

    //printf("SD_CARD state3 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));
/*
    ReadSDIO_DMA(testArray,ulSectorNumber,1,pxDisk);

    for(int i=0;i<512;i++)
    {
      if(pucSource[i] != testArray[i])
      {
        printf("Write check error, src[%d] = %x des[%d] = %X\n",i,pucSource[i],i,testArray[i]);
      }

    }*/
    //printf("SD_CARD state4 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));

    return FF_ERR_NONE;
  }
  else
  {
  #if DEBUG_SDCARD > 0
    printf("WriteSDIO_DMA, buffer not aligned \n");
    #endif
    return FF_ERR_DEVICE_DRIVER_FAILED;
  }
  return FF_ERR_NONE;
}
/* ************************************************/
int32_t SdCard_c::ReadSDIO_DMA( uint8_t *pucDestination,
                         uint32_t ulSectorNumber,
                         uint32_t ulSectorCount,
                         FF_Disk_t *pxDisk )
{
  //printf("ReadSDIO_DMA\n");
  //GetOwnPtr()->WaitForProgComplete();
  if( ( ( ( size_t ) pucDestination ) & ( sizeof( size_t ) - 1 ) ) == 0 )
  {

    HAL_StatusTypeDef  status = HAL_SD_ReadBlocks_DMA(&(GetOwnPtr()->hsd),pucDestination,ulSectorNumber,ulSectorCount);

    GetOwnPtr()->TakeSemaphore();
    GetOwnPtr()->WaitForProgComplete();
    return FF_ERR_NONE;
  }
  else
  {
  #if DEBUG_SDCARD > 0
    printf("ReadSDIO_DMA, buffer not aligned \n");
    #endif
    return FF_ERR_DEVICE_DRIVER_FAILED;
  }
  

  
}



int32_t SdCard_c::WriteSDIO_DMA( uint8_t *pucSource,
                          uint32_t ulSectorNumber,
                          uint32_t ulSectorCount,
                          FF_Disk_t *pxDisk )
{
  uint32_t startTime = TIM2->CNT;
  //printf("WriteSDIO_DMA, SecIdx=%d, SecNo=%d\n",ulSectorNumber,ulSectorCount);
  SdCard_c* p = GetOwnPtr();
  //p->WaitForProgComplete();
  if( ( ( ( size_t ) pucSource ) & ( sizeof( size_t ) - 1 ) ) == 0 )
  {
    //printf("SD_CARD state1 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));
    if(ulSectorCount > 1)
    {
      uint32_t status;   
      sdCardResp_u sdCardResp;   
      uint32_t rca = (uint32_t)((uint32_t)p->hsd.SdCard.RelCardAdd);
      /* CMD55 */
      status = sendSDIOCommand(SDMMC_CMD_APP_CMD,rca,SDIO_COMMAND_RESP_SHORT);
       sdCardResp.regVal = SDIO->RESP1;
      /* CMD23 */
      status = sendSDIOCommand(SDMMC_CMD_SET_BLOCK_COUNT,ulSectorCount,SDIO_COMMAND_RESP_SHORT);
       sdCardResp.regVal = SDIO->RESP1;
       p->WaitForProgComplete();
    }

    HAL_StatusTypeDef  status = HAL_SD_WriteBlocks_DMA(&(p->hsd),pucSource,ulSectorNumber,ulSectorCount);

    uint32_t beforeSemaphoreTime = TIM2->CNT;
    p->TakeSemaphore();
    uint32_t afterSemaphoreTime = TIM2->CNT;
    p->WaitForProgComplete();
    uint32_t completeTime = TIM2->CNT;


    if(startTime < beforeSemaphoreTime) { beforeSemaphoreTime -= startTime; } else {beforeSemaphoreTime += (0xFFFFFFFF - startTime);}
    if(startTime < afterSemaphoreTime) { afterSemaphoreTime -= startTime; } else {afterSemaphoreTime += (0xFFFFFFFF - startTime);}
    if(startTime < completeTime) { completeTime -= startTime; } else {completeTime += (0xFFFFFFFF - startTime);}
#if DEBUG_SDCARD > 0
    printf("Write %d sectors: Bt=%d, At=%d Et=%d\n",ulSectorCount,beforeSemaphoreTime,afterSemaphoreTime,completeTime);
#endif
    //printf("SD_CARD state2 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));

    //p->WaitForProgComplete();

    //printf("SD_CARD state3 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));
/*
    ReadSDIO_DMA(testArray,ulSectorNumber,1,pxDisk);

    for(int i=0;i<512;i++)
    {
      if(pucSource[i] != testArray[i])
      {
        printf("Write check error, src[%d] = %x des[%d] = %X\n",i,pucSource[i],i,testArray[i]);
      }

    }*/
    //printf("SD_CARD state4 = %d \n", HAL_SD_GetCardState(&(GetOwnPtr()->hsd)));

    return FF_ERR_NONE;
  }
  else
  {
  #if DEBUG_SDCARD > 0
    printf("WriteSDIO_DMA, buffer not aligned \n");
    #endif
    return FF_ERR_DEVICE_DRIVER_FAILED;
  }
  return FF_ERR_NONE;
}

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
      cardState = HAL_SD_GetCardState(&(GetOwnPtr()->hsd));
      if(cardState == HAL_SD_CARD_PROGRAMMING)
      {
       //vTaskDelay(1);

      }
    }
    while(cardState == HAL_SD_CARD_PROGRAMMING);
    return true;

}


/**************************** CARD DETECT ***************************/

void SdCard_c::InitCardDetect(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /*Configure GPIO pins : SD_WP_Pin SD_CD_Pin */
  GPIO_InitStruct.Pin = SD_WP_Pin|SD_CD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  EXTI_ConfigTypeDef pExtiConfig;
  pExtiConfig.GPIOSel = EXTI_GPIOC;
  pExtiConfig.Line = EXTI_LINE_7; /*SD_CD_Pin = 7 */
  pExtiConfig.Mode = EXTI_MODE_INTERRUPT;
  pExtiConfig.Trigger = EXTI_TRIGGER_RISING_FALLING;

  HAL_EXTI_SetConfigLine(&hexti, &pExtiConfig);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 15, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

bool SdCard_c::CardInserted(void)
{
    /*!< Check GPIO to detect SD */
    if( HAL_GPIO_ReadPin( configSD_DETECT_GPIO_PORT, configSD_DETECT_PIN ) != 0 )
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
void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin )
{  
  if( GPIO_Pin == configSD_DETECT_PIN )
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
    p->GiveSemephoreISR();
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
/**************************** IRQ HANDLERS ***************************/

void EXTI9_5_IRQHandler( void )
{
    HAL_GPIO_EXTI_IRQHandler( configSD_DETECT_PIN ); /* GPIO PIN H.13 */
}

void DMA2_Stream6_IRQHandler( void )
{
  BaseType_t xHigherPriorityTaskWoken = 0;

  SdCard_c* p = SdCard_c::GetOwnPtr();
  if(p != nullptr)
  {
    /* DMA SDIO-TX interrupt handler. */
    HAL_DMA_IRQHandler( p->GetHsdHandle()->hdmatx );
    //p->GiveSemephoreISR();
  }

  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void DMA2_Stream3_IRQHandler( void )
{
  BaseType_t xHigherPriorityTaskWoken = 0;

  SdCard_c* p = SdCard_c::GetOwnPtr();
  if(p != nullptr)
  {
    /* DMA SDIO-RX interrupt handler. */
    HAL_DMA_IRQHandler( p->GetHsdHandle()->hdmarx );
    p->GiveSemephoreISR();
  }

  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void SDIO_IRQHandler( void )
{
  BaseType_t xHigherPriorityTaskWoken = 0;

    SdCard_c* p = SdCard_c::GetOwnPtr();
  if(p != nullptr)
  {
    //printf("STA = 0x%08X\n",SDIO->STA);
    //while( __HAL_SD_GET_FLAG(p->GetHsdHandle(), SDIO_FLAG_TXACT)) {}
    //printf("STA2 = 0x%08X\n",SDIO->STA);

   HAL_SD_IRQHandler( p->GetHsdHandle() );
   // p->GiveSemephoreISR();
  }

  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}


/********************************************************* NO HAL solution ******************************************/
uint32_t SdCard_c::sendSDIOCommand(uint32_t command,uint32_t arg,SDIO_COMMAND_RESP_et resp)
{
  uint32_t status;
  SDIO->ARG = arg;
  SDIO->CMD =  SDIO_CMD_CPSMEN | command | resp;
  do
  {
    status = SDIO->STA;
  }
  while((status & SDIO_STA_CMDACT) != 0);
  SDIO->ICR = 0xFFFFFFFF;
  return status;

}

int32_t SdCard_c::WriteSDIO_DMA_NO_HAL( uint8_t *pucSource,
                          uint32_t ulSectorNumber,
                          uint32_t ulSectorCount,
                          FF_Disk_t *pxDisk )
{
  uint32_t arg,cmd;
  uint32_t status;
  uint32_t errorStatus;
  uint8_t cardState;
  uint8_t cardStateEnd;
  sdCardResp_u sdCardResp;
  uint8_t errorSubCode;
  uint32_t notifyState;
  FF_Error_t  xError= FF_ERR_NONE;
  SDIO_DataInitTypeDef config;
  //currentSDIOuser = xTaskGetCurrentTaskHandle();

  SdCard_c* p = GetOwnPtr();
  uint32_t rca = (uint32_t)((uint32_t)p->hsd.SdCard.RelCardAdd);

  /* CMD7 */
  status = sendSDIOCommand(SDMMC_CMD_SEL_DESEL_CARD, rca ,SDIO_COMMAND_RESP_SHORT);
  sdCardResp.regVal = SDIO->RESP1;
  cardState = sdCardResp.state;

  SDIO->DCTRL = 0;
  /*set DMA */    


  /*
  DMA_HISR_tmp = 0;   
  DMA2->HIFCR = DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTCIF6 | DMA_HIFCR_CTEIF6;

  DMA_InitStruct.DMA_BufferSize         = sdSECTOR_SIZE*ulSectorCount/4;//0;
  DMA_InitStruct.DMA_DIR                = DMA_DIR_MemoryToPeripheral;
  DMA_InitStruct.DMA_Memory0BaseAddr    = (uint32_t)pucSource;

  DMA_Cmd(DMA2_Stream6,DISABLE);
  DMA_Init(DMA2_Stream6,&DMA_InitStruct);

  SDIO_DMACmd(ENABLE);      
  DMA_Cmd(DMA2_Stream6,ENABLE);
  SDIO_SetSDIOOperation(ENABLE); 
  */
      /* Enable SDIO DMA transfer */
    __HAL_SD_DMA_ENABLE(hsd);

    /* Force DMA Direction */
    p->hsd.hdmatx->Init.Direction = DMA_MEMORY_TO_PERIPH;
    MODIFY_REG(p->hsd.hdmatx->Instance->CR, DMA_SxCR_DIR, p->hsd.hdmatx->Init.Direction);
 
 HAL_DMA_Start_IT(p->hsd.hdmatx, (uint32_t)pucSource, (uint32_t)&p->hsd.Instance->FIFO, (uint32_t)(BLOCKSIZE * ulSectorCount)/4U);




 
  /* CMD16 */
  status = sendSDIOCommand(SDMMC_CMD_SET_BLOCKLEN,sdSECTOR_SIZE,SDIO_COMMAND_RESP_SHORT);
   sdCardResp.regVal = SDIO->RESP1;

  //arg = (pxDisk->pxIOManager-> SDCARD_HCmode == true) ? ulSectorNumber : (ulSectorNumber*sdSECTOR_SIZE);
  arg = (p->hsd.SdCard.CardType == CARD_SDHC_SDXC) ?  ulSectorNumber : (ulSectorNumber*sdSECTOR_SIZE);


  if(ulSectorCount > 1)
  {
    /* CMD55 */
    status = sendSDIOCommand(SDMMC_CMD_APP_CMD,rca,SDIO_COMMAND_RESP_SHORT);
     sdCardResp.regVal = SDIO->RESP1;
    /* CMD23 */
    status = sendSDIOCommand(SDMMC_CMD_SET_BLOCK_COUNT,ulSectorCount,SDIO_COMMAND_RESP_SHORT);
     sdCardResp.regVal = SDIO->RESP1;
     /* CMD25 */
    cmd = SDMMC_CMD_WRITE_MULT_BLOCK;

  }
  else
  {
    /* CMD24 */
    cmd = SDMMC_CMD_WRITE_SINGLE_BLOCK;
  }

  status = sendSDIOCommand(cmd,arg,SDIO_COMMAND_RESP_SHORT);
   sdCardResp.regVal = SDIO->RESP1;
   
  config.DataTimeOut   = SDMMC_DATATIMEOUT;
  config.DataLength    = ulSectorCount * BLOCKSIZE;
  config.DataBlockSize = SDIO_DATABLOCK_SIZE_512B;
  config.TransferDir   = SDIO_TRANSFER_DIR_TO_SDIO;
  config.TransferMode  = SDIO_TRANSFER_MODE_BLOCK;
  config.DPSM          = SDIO_DPSM_ENABLE;

   (void)SDIO_ConfigData(p->hsd.Instance, &config); // SDIO_DataConfig(&SDIO_DataInitStruct);
  if(status ==0x04)
  {
    xError = FF_ERR_IOMAN_DRIVER_FATAL_ERROR; 
    errorStatus = status;
    errorSubCode = 2;      
  }

  uint32_t cnt2 = xTaskGetTickCount();

  GetOwnPtr()->TakeSemaphore(); //notifyState = ulTaskNotifyTake( pdTRUE, 1000 );

  cnt2 = xTaskGetTickCount() - cnt2;

   if(notifyState == 0)
  {
    xError = FF_ERR_IOMAN_DRIVER_FATAL_ERROR;
    errorStatus = status;
    errorSubCode = 3;
  }
  /*else if(DMA_HISR_tmp & DMA_HISR_FEIF6)
  {
    xError = FF_ERR_IOMAN_DRIVER_FATAL_ERROR;
    errorStatus = status;
    errorSubCode = 4;

  }*/
  /*else if(DMA_HISR_tmp & DMA_HISR_TEIF6)
  {
    xError = FF_ERR_IOMAN_DRIVER_FATAL_ERROR;
    errorStatus = status;
    errorSubCode = 5;
  }*/

  if(ulSectorCount > 1)
  {
    /* CMD12 */
    status = sendSDIOCommand( SDMMC_CMD_STOP_TRANSMISSION,0,SDIO_COMMAND_RESP_SHORT);
     sdCardResp.regVal = SDIO->RESP1;
    if(status ==0x04)
    {
      xError = FF_ERR_IOMAN_DRIVER_FATAL_ERROR; 
      errorStatus = status;
      errorSubCode = 6;      
    } 
  }
  uint32_t cnt = xTaskGetTickCount();
  while(SDIO->STA & SDIO_STA_TXACT);
  cnt = xTaskGetTickCount() - cnt;

  //sysPrintf("Wtick=%u,%u\r\n",cnt,cnt2);

  do
  {
    sdCardResp.regVal = 0;
    /* CMD13 */
    status = sendSDIOCommand(SDMMC_CMD_SEND_STATUS,rca,SDIO_COMMAND_RESP_SHORT);
    if(status != 0x04) 
    {
      sdCardResp.regVal = SDIO->RESP1;
    }
  } while(sdCardResp.state == 7);  /* wait until prog end */
   
  if(status ==0x04) 
  {
    xError = FF_ERR_IOMAN_DRIVER_FATAL_ERROR; 
    errorStatus = status;
    errorSubCode = 7;      
  }
  cardStateEnd = (SDIO->RESP1>>9) &0x07;

  if(cardStateEnd != 0x04)
  {
    xError = FF_ERR_IOMAN_DRIVER_FATAL_ERROR; 
    errorStatus = status;
    errorSubCode = 8;
  }
  

  SDIO->ICR = 0xFFFFFFFF;


  SDIO->DCTRL = 0;

  /************** werification ************************/

/*
    HAL_SD_ReadBlocks(&(GetOwnPtr()->hsd),testArray,ulSectorNumber,1,20000);

    for(int i=0;i<512;i++)
    {
      if(pucSource[i] != testArray[i])
      {
        printf("Write check error, src[%d] = %x des[%d] = %X\n",i,pucSource[i],i,testArray[i]);
      }

    }
*/







  return xError;

}

