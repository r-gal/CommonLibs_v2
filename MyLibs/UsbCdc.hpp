#ifndef USBCDC_H
#define USBCDC_H

//#include "common.hpp"
#include "usbd_cdc.h"

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"

#include "Serial.hpp"

#define APP_RX_DATA_SIZE  1024
#define APP_TX_DATA_SIZE  1024


class UsbCdc_c : public Serial_c
{
  USBD_HandleTypeDef hUsbDevice;

  uint8_t* RxBuffer; 

  /** Data to send over USB CDC are stored in this buffer   */
  uint8_t* TxBuffer;

  uint16_t lastRxLength, rxReadIdx;

  TaskHandle_t xTaskToNotify;

  public:


  static UsbCdc_c* ownPtr;
  static UsbCdc_c* GetOwnPtr(void) { return ownPtr; }

  UsbCdc_c(void);

  int8_t CDC_Init(void);
  int8_t CDC_DeInit(void);
  int8_t CDC_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length);
  int8_t CDC_Receive(uint8_t* pbuf, uint32_t *Len);
  int8_t CDC_ReceiveCplt(void);
  int8_t CDC_TransmitCplt(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);
  uint8_t CDC_Transmit(uint8_t* Buf, uint16_t Len);

  void Init(void);

  uint8_t GetByte(void);
  int GetNoOfWaitingBytes(void);
  void DataSend(char* buffer, uint16_t length);

  uint8_t GetDevState(void);

};










#endif