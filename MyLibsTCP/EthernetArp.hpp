#ifndef ETHERNETARP_H
#define ETHERNETARP_H

#include "TcpDataDef.hpp"
#include "EthernetCommon.hpp"
#include "EthernetMac.hpp"
#include "commandHandler.hpp"

#define ARP_MAX_AGE 200

struct ArpEntry_st
{
  uint32_t ip;
  uint16_t age;
  uint8_t mac[6];
};

class Arp_c :public MAC_c
{

  static ArpEntry_st arpArray[ARP_ARRAY_SIZE];

  static void FillArpHeader(Packet_st* packet, uint8_t oper, uint8_t*  destMac, uint32_t destId);
  static uint16_t GetArpHeaderSize(void) { return 28; }

  static void IpToByteArray(uint8_t* array, uint32_t ip);

  public:

  void Tick1s(void);

  void CleanArray(void);
  static uint8_t* FetchMac(uint32_t ip);
  void AddEntry(uint32_t ip, uint8_t* mac);

  static void SendRequest(uint32_t requestedIp);

  void HandlePacket(uint8_t* packet_p,uint16_t length);

  static ArpEntry_st* GetEntry(uint8_t idx) { return &arpArray[idx]; }


};

/*****************command section **************************/

#if CONF_USE_COMMANDS == 1
class Com_arp : public Command_c
{
  public:
  char* GetComString(void) { return (char*)"arp"; }
  void PrintHelp(CommandHandler_c* commandHandler ){}
  comResp_et Handle(CommandData_st* comData_);
};


class CommandArp_c :public CommandGroup_c
{

  Com_arp arp;

  public:

};

#endif

#endif