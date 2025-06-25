#ifndef ETHERNETIPCONFIG_H
#define ETHERNETIPCONFIG_H

#include "commandHandler.hpp"

struct ipConf_st
{
  uint32_t configValid;
  uint32_t ownIp;
  uint32_t dhcpServerIp;
  uint32_t dnsServerIp;
  uint32_t subnetMask;
  uint32_t gateway;
  bool dhcpEna;
  
};

class IpConfig_c
{

  
  ipConf_st currentConfig;
  ipConf_st* admConfig_p;

  public:

  IpConfig_c(void);

  uint32_t GetIp(void) { return currentConfig.ownIp; }
  uint32_t GetDhcpIp(void) { return currentConfig.dhcpServerIp; }
  uint32_t GetDnsIp(void) { return currentConfig.dnsServerIp; }
  uint32_t GetSubnetMask(void) { return currentConfig.subnetMask; }
  uint32_t GetGateway(void) { return currentConfig.gateway; }
  bool GetDhcpEna(void);

  ipConf_st* GetCurrentConfig(void) { return &currentConfig; }
  ipConf_st* GetAdmConfig(void) { return admConfig_p; }
  void WriteAdmConf(ipConf_st* newConfig);

  void UpdateConfig(  uint32_t ownIp_,
                      uint32_t dhcpServerIp_,
                      uint32_t dnsServerIp_,
                      uint32_t subnetMask_,
                      uint32_t gateway_);

  void InitialConfig(void);

  void UseAdministeredConfiguration(void);

  static void PrintIp(char* strBuffer,uint32_t ip, char* name);

  void PrintIpConfig(char* buffer,ipConf_st* conf_p);
};

/*****************command section **************************/
#if USE_COMMANDS == 1

class Com_ipconfig : public Command_c
{
  public:
  char* GetComString(void) { return (char*)"ipconfig"; }
  void PrintHelp(CommandHandler_c* commandHandler ){}
  comResp_et Handle(CommandData_st* comData_);
};

class Com_ipset : public Command_c
{
  public:
  char* GetComString(void) { return (char*)"ipset"; }
  void PrintHelp(CommandHandler_c* commandHandler ){}
  comResp_et Handle(CommandData_st* comData_);
};


class CommandIp_c :public CommandGroup_c
{

  Com_ipconfig ipconfig;
  Com_ipset ipset;

  public:

  CommandIp_c(void){}


};

#endif



#endif