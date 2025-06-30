#ifndef ETHERNETNTP_H
#define ETHERNETNTP_H


#include "EthernetCommon.hpp"
#include "EthernetSocket.hpp"
//#include "EthernetIp.hpp"
#include "EthernetUdp.hpp"

#include "commandHandler.hpp"

#define NTP_PORT 123

#define NTP_INTERVAL 200
#define NTP_SERVERS_MAX 4

struct NtpFrame_st
{
  uint8_t flagsMode;
  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;
  uint32_t rootDelay;
  uint32_t rootDispersion;
  uint32_t referenceId;
  uint32_t referenceTimestampSec;
  uint32_t referenceTimestampFrac;
  uint32_t originTimestampSec;
  uint32_t originTimestampFrac;
  uint32_t receiveTimestampSec;
  uint32_t receiveTimestampFrac;
  uint32_t transmitTimestampSec;
  uint32_t transmitTimestampFrac;

};

class NTP_c: public SocketUdp_c
{
  TimerHandle_t timer;

  static uint32_t* ntpServerIp;

  bool waitForResponse;
  uint8_t currentIdx;

  public:

  static uint32_t lastUpdate;
  static uint32_t lastHardUpdate;

  static void SetServerIp(uint8_t idx, uint32_t ip);
  static uint32_t GetServerIp(uint8_t idx);

  void HandlePacket(uint8_t* packet_p,uint16_t packetSize);

  NTP_c(void);  

  void HandleLinkStateChange(uint8_t newState);

  void SendRequest(uint8_t* gatewayMac, uint32_t ntpServerIp);
  void TimerHandler(void);


};

/*****************command section **************************/
#if CONF_USE_COMMANDS == 1
class Com_ntp : public Command_c
{
  public:
  char* GetComString(void) { return (char*)"ntp"; }
  void PrintHelp(CommandHandler_c* commandHandler ){}
  comResp_et Handle(CommandData_st* comData_);
};


class CommandNtp_c :public CommandGroup_c
{

  Com_ntp ntp;

  public:

};
#endif



#endif