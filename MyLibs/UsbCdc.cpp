 #include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"
#include "SignalList.hpp"
#include "commonDef.h"
#include "UsbCdc.hpp"

#ifdef __cplusplus
 extern "C" {
#endif

void Usb_Error_Handler(void)
{
  printf("USB error\n");

}
extern PCD_HandleTypeDef hpcd_USB_FS;
  


void USB_LP_IRQHandler(void)
{

  HAL_PCD_IRQHandler(&hpcd_USB_FS);
}

#ifdef __cplusplus
}
#endif

UsbCdc_c* UsbCdc_c::ownPtr = NULL;

usbRecDataSig_c RxSig;

static int8_t CDC_Init_Wrapper(void);
static int8_t CDC_DeInit_Wrapper(void);
static int8_t CDC_Control_Wrapper(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_Wrapper(uint8_t* pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_Wrapper(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_Wrappers =
{
  CDC_Init_Wrapper,
  CDC_DeInit_Wrapper,
  CDC_Control_Wrapper,
  CDC_Receive_Wrapper,
  CDC_TransmitCplt_Wrapper
};

static int8_t CDC_Init_Wrapper(void)
{
  return UsbCdc_c::GetOwnPtr()->CDC_Init();
}
static int8_t CDC_DeInit_Wrapper(void)
{
  return UsbCdc_c::GetOwnPtr()->CDC_DeInit();
}
static int8_t CDC_Control_Wrapper(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  return UsbCdc_c::GetOwnPtr()->CDC_Control(cmd,pbuf,length);
}
static int8_t CDC_Receive_Wrapper(uint8_t* pbuf, uint32_t *Len)
{
  return UsbCdc_c::GetOwnPtr()->CDC_Receive(pbuf,Len);
}
static int8_t CDC_TransmitCplt_Wrapper(uint8_t *pbuf, uint32_t *Len, uint8_t epnum)
{
  return UsbCdc_c::GetOwnPtr()->CDC_TransmitCplt(pbuf,Len,epnum);
}



int8_t UsbCdc_c::CDC_Init(void)
{
  //printf("CDC_Init_HS\n");
  /* USER CODE BEGIN 8 */
  /* Set Application Buffers */
  USBD_CDC_SetTxBuffer(&hUsbDevice, TxBuffer, 0);
  USBD_CDC_SetRxBuffer(&hUsbDevice, RxBuffer);
  return (USBD_OK);
  /* USER CODE END 8 */
}
int8_t UsbCdc_c::CDC_DeInit(void)
{
  //  printf("CDC_DeInit_HS\n");
  /* USER CODE BEGIN 9 */
  return (USBD_OK);
  /* USER CODE END 9 */
}

int8_t UsbCdc_c::CDC_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
/* USER CODE BEGIN 10 */

 /* printf("USB CMD=%d len=%d\n",cmd,length);

  for(int i=0;i<length;i++)
  {
    printf("0x%X,",pbuf[i]);

  }
  printf("\n"); */

  switch(cmd)
  {
  case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

  case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

  case CDC_SET_COMM_FEATURE:

    break;

  case CDC_GET_COMM_FEATURE:

    break;

  case CDC_CLEAR_COMM_FEATURE:

    break;

  /*******************************************************************************/
  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  /*******************************************************************************/
  case CDC_SET_LINE_CODING:

    break;

  case CDC_GET_LINE_CODING:

    break;

  case CDC_SET_CONTROL_LINE_STATE:

    break;

  case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
  /* USER CODE END 10 */
}
int8_t UsbCdc_c::CDC_Receive(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 11 */
  /*char buffer[65];
  printf("USB Rec data, len =%d\n",*Len);
  strncpy(buffer,(char*)Buf,*Len);
  buffer[*Len] = 0;
  printf("data = %s\n",buffer);
  printf("ptr  = %X\n",Buf);*/

  RxSig.dataLen = *Len;
  RxSig.data_p = Buf;
  Buf[*Len] = 0;
  RxSig.SendISR();

  lastRxLength = *Len;
  rxReadIdx = 0;

  return (USBD_OK);
  /* USER CODE END 11 */
}

int8_t UsbCdc_c::CDC_ReceiveCplt(void)
{
  USBD_CDC_ReceivePacket(&hUsbDevice);
  return (USBD_OK);
}

int8_t UsbCdc_c::CDC_TransmitCplt(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
 // printf("CDC_TransmitCplt\n");
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 14 */
  UNUSED(Buf);
  UNUSED(Len);
  UNUSED(epnum);

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );

  /* USER CODE END 14 */
  return result;
}

UsbCdc_c::UsbCdc_c(void)
{
  if(ownPtr == NULL)
  {
    ownPtr = this;
  }

}

void UsbCdc_c::Init(void )
{
  /* force reinit */
  lastRxLength = 0;
  rxReadIdx = 0;

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11|GPIO_PIN_12,GPIO_PIN_RESET);
  vTaskDelay(200);
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


  /* Init Device Library, add supported class and start the library. */
  if (USBD_Init(&hUsbDevice, &CDC_Desc, DEVICE_FS) != USBD_OK)
  {
    Usb_Error_Handler();
  }
  if (USBD_RegisterClass(&hUsbDevice, &USBD_CDC) != USBD_OK)
  {
    Usb_Error_Handler();
  }

  //printf("USB init\n");
  RxBuffer = new uint8_t[APP_RX_DATA_SIZE];
  TxBuffer = new uint8_t[APP_TX_DATA_SIZE];

  if (USBD_CDC_RegisterInterface(&hUsbDevice, &USBD_Interface_Wrappers) != USBD_OK)
  {
    //Error_Handler();
    printf("Init error \n");
  }

  if (USBD_Start(&hUsbDevice) != USBD_OK)
  {
    Usb_Error_Handler();
  }

  USBD_CDC_ReceivePacket(&hUsbDevice);

  /* USER CODE BEGIN USB_DEVICE_Init_PostTreatment */
  //HAL_PWREx_EnableUSBVoltageDetector();

  /* USER CODE END USB_DEVICE_Init_PostTreatment */
}

uint8_t UsbCdc_c::CDC_Transmit(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 12 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDevice.pClassData;
  if (hcdc->TxState != 0){
    return USBD_BUSY;
  }
  xTaskToNotify = xTaskGetCurrentTaskHandle();


  USBD_CDC_SetTxBuffer(&hUsbDevice, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDevice);

  ulTaskNotifyTake( pdFALSE, 1000 );
  /* USER CODE END 12 */
  return result;
}


uint8_t UsbCdc_c::GetByte(void)
{
  uint8_t ret = RxBuffer[rxReadIdx];
  rxReadIdx++;
  if(rxReadIdx >= lastRxLength)
  {
    lastRxLength = 0;
    rxReadIdx = 0;
    CDC_ReceiveCplt();
  }
  return ret;
}
int UsbCdc_c::GetNoOfWaitingBytes(void)
{
   return lastRxLength;
}
void UsbCdc_c::DataSend(char* buffer, uint16_t length)
{
  CDC_Transmit((uint8_t*) buffer,length);
}

uint8_t UsbCdc_c::GetDevState(void)
{
  return hUsbDevice.dev_state;
}