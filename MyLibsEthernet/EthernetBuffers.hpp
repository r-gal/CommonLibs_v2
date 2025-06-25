#ifndef ETHERNETBUF_H
#define ETHERNETBUF_H

//#include "stm32f4xx_hal_eth.h"
#include "GeneralConfig.h"




class EthernetBuffers_c
{

  public:

  static uint8_t* NewBuffer(void);
  static void DeleteBuffer(uint8_t* buffer);



};









#endif