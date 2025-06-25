#ifndef ETHERNETMAC_H
#define ETHERNETMAC_H

#include "TcpDataDef.hpp"
#include "EthernetCommon.hpp"




class MAC_c
{

  static uint8_t ownMac[6];

  public:

  static void InitMac(void);


  MAC_c(void);

  

  static uint8_t* GetOwnMac(void);
 
  static uint16_t GetMacHeaderSize(void) { return 14; }

  static void FillMacHeader(Packet_st* packet, uint8_t* mac, uint16_t ethType);
  uint8_t* GetSrcMac(Packet_st* packet);

  static bool SendPacket(uint8_t* buf1, uint16_t buf1len, uint8_t* buf2, uint16_t buf2len);


};

class MAC_Initializer_c
{
  public:
  MAC_Initializer_c(void)
  {
    MAC_c::InitMac();
  }

};

#ifdef __cplusplus
 extern "C" {
#endif
uint8_t* GetMacAddress(void);
#ifdef __cplusplus
}
#endif

#endif