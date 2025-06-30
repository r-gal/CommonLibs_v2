#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

//#include "common.hpp"

#include "EthernetIpConfig.hpp"
#include "EthernetIp.hpp"
#include "GeneralConfig.h"

#include "EthernetArp.hpp"


IpConfig_c ipConfig;

IpConfig_c* IP_c::ipConfig_p = &ipConfig;



IpConfig_c::IpConfig_c(void)
{
  
  currentConfig.ownIp = 0;
  currentConfig.dhcpServerIp = 0;
  currentConfig.dnsServerIp = 0;
  currentConfig.subnetMask = 0;
  currentConfig.gateway = 0;
  currentConfig.dhcpEna = true;
  currentConfig.configValid = 0xDEADBEEF;

  admConfig_p = (ipConf_st*)(&(RTC->BKP1R));
}



void IpConfig_c::InitialConfig(void)
{


}

void IpConfig_c::UpdateConfig(  uint32_t ownIp_,
                    uint32_t dhcpServerIp_,
                    uint32_t dnsServerIp_,
                    uint32_t subnetMask_,
                    uint32_t gateway_)
{

  currentConfig.ownIp = ownIp_;
  currentConfig.dhcpServerIp = dhcpServerIp_;
  currentConfig.dnsServerIp = dnsServerIp_;
  currentConfig.subnetMask = subnetMask_;
  currentConfig.gateway = gateway_;

  if(gateway_ != 0)
  {
    Arp_c::FetchMac(gateway_);
  }
  #if DEBUG_IPCONF > 0
  printf("new config:\n");
  char* strBuf = new char[512];

  PrintIpConfig(strBuf,&currentConfig);
  printf(strBuf);

  delete[] strBuf;
  #endif

}

#if USE_CONFIGURABLE_IP == 1

void IpConfig_c::UseAdministeredConfiguration(void)
{
  if(admConfig_p->configValid == 0xDEADBEEF)
  {
    UpdateConfig(admConfig_p->ownIp,
                admConfig_p->dhcpServerIp,
                admConfig_p->dnsServerIp,
                admConfig_p->subnetMask,
                admConfig_p->gateway);
  }
  else
  {
    UpdateConfig(DEFAULT_IP,
                 DEFAULT_DHCP_SERVER,
                 DEFAULT_DNS_SERVER,
                 DEFAULT_SUBNET_MASK,
                 DEFAULT_GATEWAY);
  }



  currentConfig.dhcpEna = false;
}

void IpConfig_c::WriteAdmConf(ipConf_st* newConfig)
{
  //HAL_PWR_EnableBkUpAccess();
  memcpy(admConfig_p,newConfig,sizeof(ipConf_st));
  //HAL_PWR_DisableBkUpAccess();
}

bool IpConfig_c::GetDhcpEna(void) 
{ 
  if(admConfig_p->configValid == 0xDEADBEEF)
  {
    return admConfig_p->dhcpEna; 
  }
  else
  {
    return true;
  }
}

void IpConfig_c::PrintIpConfig(char* buffer,ipConf_st* conf_p)
{


  if(conf_p->dhcpEna) 
  {
    sprintf(buffer,"DHCP: ENABLED\n");
  }
  else
  {
    sprintf(buffer,"DHCP: DISABLED\n");
  }
  buffer += strlen(buffer);

  PrintIp(buffer,conf_p->ownIp,       "ownIp        = "); strcat(buffer,"\n"); buffer += strlen(buffer);
  PrintIp(buffer,conf_p->dhcpServerIp,"dhcpServerIp = "); strcat(buffer,"\n"); buffer += strlen(buffer);
  PrintIp(buffer,conf_p->dnsServerIp, "dnsServerIp  = "); strcat(buffer,"\n"); buffer += strlen(buffer);
  PrintIp(buffer,conf_p->subnetMask,  "subnetMask   = "); strcat(buffer,"\n"); buffer += strlen(buffer);
  PrintIp(buffer,conf_p->gateway,     "gateway      = "); strcat(buffer,"\n"); buffer += strlen(buffer);

  sprintf(buffer,"END.\n");

}




/*****************command section **************************/

#if CONF_USE_COMMANDS == 1

CommandIp_c commandIp;

comResp_et Com_ipconfig::Handle(CommandData_st* comData)
{

  char* strBuf = new char[512];

  Print(comData->commandHandler,"ADMINISTRED IP CONFIG:\n");

  ipConfig.PrintIpConfig(strBuf,ipConfig.GetAdmConfig());

  Print(comData->commandHandler,strBuf);

  Print(comData->commandHandler,"\nUSED IP CONFIG:\n");

  ipConfig.PrintIpConfig(strBuf,ipConfig.GetCurrentConfig());

  Print(comData->commandHandler,strBuf);

  delete[] strBuf;

  return COMRESP_OK;
}

comResp_et Com_ipset::Handle(CommandData_st* comData)
{

  uint32_t newIp;
  bool ipValid = FetchParameterIp(comData,"IP",&newIp);
  uint32_t newDnsIp;
  bool dnsIpValid = FetchParameterIp(comData,"DNS",&newDnsIp);
    uint32_t newDhcpIp;
  bool dhcpIpValid = FetchParameterIp(comData,"DHCP",&newDhcpIp);
    uint32_t newGateway;
  bool gatewayValid = FetchParameterIp(comData,"GATEWAY",&newGateway);
    uint32_t newSubmask;
  bool submaskValid = FetchParameterIp(comData,"SUBNET",&newSubmask);
   
    uint32_t dhcpEna;
  bool dhcpEnaValid = FetchParameterValue(comData,"DHCPENA",&dhcpEna,0,1);

  ipConf_st newConfig;
  memcpy(&newConfig,ipConfig.GetAdmConfig(),sizeof(ipConf_st));

  if(newConfig.configValid != 0xDEADBEEF)
  {
    newConfig.configValid = 0xDEADBEEF;
    newConfig.dhcpEna = true;
    newConfig.dhcpServerIp = 0;
    newConfig.dnsServerIp = 0;
    newConfig.gateway = 0;
    newConfig.subnetMask = 0;
    newConfig.ownIp = 0;
  }

  if(ipValid) { newConfig.ownIp = newIp; }
  if(dnsIpValid)  { newConfig.dnsServerIp = newDnsIp; }
  if(dhcpIpValid)  { newConfig.dhcpServerIp = newDhcpIp; }
  if(gatewayValid)  { newConfig.gateway = newGateway; }
  if(submaskValid)  { newConfig.subnetMask = newSubmask; }
  if(dhcpEnaValid) { newConfig.dhcpEna = (dhcpEna == 1); }

  ipConfig.WriteAdmConf(&newConfig);
  
  return COMRESP_OK;
}
#endif

#else

/* constant addresses */


void IpConfig_c::UseAdministeredConfiguration(void)
{
  UpdateConfig(DEFAULT_IP,
             DEFAULT_DHCP_SERVER,
             DEFAULT_DNS_SERVER,
             DEFAULT_SUBNET_MASK,
             DEFAULT_GATEWAY);
}

#endif

#if CONF_USE_COMMANDS == 1

void IpConfig_c::PrintIp(char* strBuffer,uint32_t ip, char* name)
{
  sprintf(strBuffer,"%s%d.%d.%d.%d",name,(ip>>24)&0xFF,(ip>>16)&0xFF,(ip>>8)&0xFF,(ip)&0xFF);


}

#endif