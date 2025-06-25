#ifndef ETHERNETMDNS_H
#define ETHERNETMDNS_H


#include "EthernetCommon.hpp"
#include "EthernetSocket.hpp"
//#include "EthernetIp.hpp"
#include "EthernetUdp.hpp"

#define MDNS_FLAG_QR 0x8000
#define MDNS_FLAG_AA 0x0400

struct MdnsFrame_st
{
  uint16_t identification;
  uint16_t flags;
  uint16_t NoOfQuestions;
  uint16_t noOfAnswers;
  uint16_t noOfAuthorityRRs;
  uint16_t noOfAdditionalRRs;
  uint8_t payload[1];


};

class MDNS_c: public SocketUdp_c
{
  static uint8_t mdnsMac[6];
  public:

  static uint8_t* GetMac(void);

  void HandlePacket(uint8_t* packet_p,uint16_t packetSize);

  MDNS_c(void);

  

};












#endif