#ifndef ETHERNETDHCP_H
#define ETHERNETDHCP_H


#include "EthernetCommon.hpp"
#include "EthernetSocket.hpp"
//#include "EthernetIp.hpp"
#include "EthernetUdp.hpp"


#define OPT_SPACE 0
#define OPT_SUBNET_MASK 1
#define OPT_GATEWAY 3
#define OPT_DNS_SERVER 6
#define OPT_REQ_IP 50
#define OPT_LEASETIME 51
#define OPT_DHCP_MSG_TYPE 53
#define OPT_DHCP_SERVER 54
#define OPT_REQ_PARAM 55
#define OPT_HOST_NAME 12
#define OPT_END 0xFF



#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_ACK 5
#define DHCP_NACK 6

#define DHCP_PORT 68
#define DHCP_TX_PORT 67

#define DEFAULT_LEASE_TIME 3600
#define DEFAULT_TIMEOUT 5

struct DhcpFrame_st
{
  uint8_t oper;
  uint8_t type;
  uint8_t adrLen;
  uint8_t jumpsNo;
  uint32_t xid;
  uint16_t time;
  uint16_t flags;
  uint32_t clientIp;
  uint32_t givenClientIp;
  uint32_t serverIp;
  uint32_t gateIp;
  uint8_t clientPhyAdr[16];
  uint8_t serverName[64];
  uint8_t startFile[128];
  uint32_t magicCookie;
};

struct dhcpOptions_st
{
  uint8_t code;
  uint8_t size;
  uint8_t value[12];
};

enum dchpState_et
{
  DHCP_STATE_IDLE,
  DHCP_STATE_DISCOVER,
  DHCP_STATE_REQUEST,
  DHCP_STATE_CONFIGURED,
  DHCP_STATE_RENEWING,
  DHCP_STATE_REBINDING
};


enum DHCP_TIMER_et
{
  DHCP_T1,
  DHCP_T2,
  DHCP_T3,
  DHCP_NOOFTIMERS
};

class DHCP_c : public SocketUdp_c
{
  dchpState_et state;

  uint32_t dhcpServerIp;
  uint32_t givenIp;

  uint32_t usedLeaseTime;

  TimerHandle_t timer[DHCP_NOOFTIMERS];

  public:

  void TimerHandler(uint8_t timerIndicator);

  void Init(void);

  uint16_t CreateMessage(DhcpFrame_st* msg_p,uint8_t oper);

  void HandleLinkStateChange(uint8_t newState);
  void InitStart(void);
  void DeinitStart(void);

  void SendFrame(uint8_t oper);

  void HandlePacket(uint8_t* packet_p,uint16_t packetSize);

  DHCP_c(void);


};


#endif