#ifndef TCP_DATA_DEF_H
#define TCP_DATA_DEF_H

#include "EthernetConfig.hpp"


#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IPV4 0x0800


#define IP_PROTOCOL_ICMP 1
#define IP_PROTOCOL_IGM  2
#define IP_PROTOCOL_TCP  6
#define IP_PROTOCOL_UDP  17



struct MacHeader_st
{
  uint8_t MAC_Dest[6];
  uint8_t MAC_Src[6];
  uint16_t ethType;
};

struct IpHeader_st
{
  uint8_t version_ihl;
  uint8_t dscp_ecn;
  uint16_t length;
  uint16_t ident;
  uint16_t flags_foffset;
  uint8_t timeToLive;
  uint8_t protocol;
  uint16_t HeaderCS;
  uint32_t srcIP;
  uint32_t destIP;
};

struct UdpHeader_st
{
  uint16_t srcPort;
  uint16_t dstPort;
  uint16_t Length;
  uint16_t CS;
};

struct IcmpHeader_st
{
  uint8_t type;
  uint8_t code;
  uint16_t checkSum;
  uint32_t rest;
};

struct TcpHeader_st
{
  uint16_t srcPort;
  uint16_t dstPort;
  uint32_t seqNo;
  uint32_t ackNo;
  uint16_t offset_flags;
  uint16_t windowSize;
  uint16_t checkSum;
  uint16_t urgentPtr;
};

struct ArpHeader_st
{
    uint16_t hType;              
    uint16_t pType;              
    uint8_t hAdrLen;   
    uint8_t pAdrLen;     
    uint16_t oper;                
    uint8_t hAdrS[6];  
    uint8_t pAdrS[4]; 
    uint8_t hAdrR[6];  
    uint8_t pAdrR[4];    
};

struct Packet_st
{
  uint16_t packetLength;
  MacHeader_st macHeader;
  union
  {
    ArpHeader_st arpHeader;
    struct
    {
      IpHeader_st ipHeader;
      union
      {
        struct
        {
          TcpHeader_st tcpHeader;
          uint8_t tcpPayload[SMALL_PACKET_SIZE];
        };
        struct
        {
          UdpHeader_st udpHeader;
          uint8_t udpPayload[SMALL_PACKET_SIZE];
        };
        struct
        {
          IcmpHeader_st icmpHeader;
          uint8_t icmpPayload[SMALL_PACKET_SIZE];
        };
      };
    }; 
  };
};

class SocketTcp_c;

enum SocketRequest_et
{
  SOCKET_LISTEN,
  SOCKET_ACCEPT,
  SOCKET_SEND,
  SOCKET_SHUTDOWN,
  SOCKET_CLOSE,
  SOCKET_CLOSE_AFTER_SEND,
  SOCKET_DISCONNECT_ALL_CHILD,
  SOCKET_REUSE_LISTEN,
  SOCKET_GET_LOCAL_ADDR,
  SOCKET_SET_RXBUFFER_SIZE,
  SOCKET_SETTASK
};

struct SocketAdress_st
{
  uint32_t ip;
  uint16_t port;
};

















#endif
