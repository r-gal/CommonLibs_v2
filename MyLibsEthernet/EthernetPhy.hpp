#ifndef ETHERNETPHY_H
#define ETHERNETPHY_H

//#include "stm32f4xx_hal_eth.h"
#include "GeneralConfig.h"
//#define ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM 1
//#define ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM 1

#define PHY_ADR 0
#define PHY_MAX_RESET_TIME_MS    1000
#define PHY_SHORT_DELAY_MS    50
#define PHY_MAX_NEGOTIATE_TIME_MS    3000


/* Naming and numbering of PHY registers. */
#define PHY_REG_00_BMCR			0x00	/* Basic mode control register */
#define PHY_REG_01_BMSR			0x01	/* Basic mode status register */
#define PHY_REG_02_PHYSID1		0x02	/* PHYS ID 1 */
#define PHY_REG_03_PHYSID2		0x03	/* PHYS ID 2 */
#define PHY_REG_04_ADVERTISE	        0x04	/* Advertisement control reg */

/* Naming and numbering of extended PHY registers. */
#define PHY_REG_10_PHYSTS           0x10U    /* 16 PHY status register Offset */
#define PHY_REG_19_PHYCR            0x19U    /* 25 RW PHY Control Register */
#define PHY_REG_1F_PHYSPCS          0x1FU    /* 31 RW PHY Special Control Status */

#define PHY_MICR                        ((uint16_t)0x11)    /*!< MII Interrupt Control Register                  */
#define PHY_MISR                        ((uint16_t)0x12)    /*!< MII Interrupt Status and Misc. Control Register */

//#define PHY_AUTONEGO_COMPLETE           ((uint16_t)0x0020)  /*!< Auto-Negotiation process completed   */
//#define PHY_LINKED_STATUS               ((uint16_t)0x0004)  /*!< Valid link established               */
//#define PHY_JABBER_DETECTION            ((uint16_t)0x0002)  /*!< Jabber condition detected            */

#define PHY_LINK_STATUS                 ((uint16_t)0x0001)  /*!< PHY Link mask                                   */
//#define PHY_SPEED_STATUS                ((uint16_t)0x0002)  /*!< PHY Speed mask                                  */
//#define PHY_DUPLEX_STATUS               ((uint16_t)0x0004)  /*!< PHY Duplex mask                                 */

#define PHY_MICR_INT_EN                 ((uint16_t)0x0002)  /*!< PHY Enable interrupts                           */
#define PHY_MICR_INT_OE                 ((uint16_t)0x0001)  /*!< PHY Enable output interrupt events              */

#define PHY_MISR_LINK_INT_EN            ((uint16_t)0x0020)  /*!< Enable Interrupt on change of link status       */
#define PHY_LINK_INTERRUPT              ((uint16_t)0x2000)  /*!< PHY link status interrupt mask                  */

/*
 * Description of all capabilities that can be advertised to
 * the peer (usually a switch or router).
 */
#define ADVERTISE_CSMA			0x0001		// Only selector supported
#define ADVERTISE_10HALF		0x0020		// Try for 10mbps half-duplex
#define ADVERTISE_10FULL		0x0040		// Try for 10mbps full-duplex
#define ADVERTISE_100HALF		0x0080		// Try for 100mbps half-duplex
#define ADVERTISE_100FULL		0x0100		// Try for 100mbps full-duplex

#define ADVERTISE_ALL			( ADVERTISE_10HALF | ADVERTISE_10FULL | \
								  ADVERTISE_100HALF | ADVERTISE_100FULL)

/*
 * Value for the 'PHY_REG_00_BMCR', the PHY's Basic mode control register
 */
#define BMCR_FULLDPLX			0x0100		// Full duplex
#define BMCR_ANRESTART			0x0200		// Auto negotiation restart
#define BMCR_ANENABLE			0x1000		// Enable auto negotiation
#define BMCR_SPEED100			0x2000		// Select 100Mbps
#define BMCR_RESET				0x8000		// Reset the PHY
#define BMCR_ISOLATE                    0x0400

#define PHYCR_MDIX_EN			0x8000		// Enable Auto MDIX
#define PHYCR_MDIX_FORCE		0x4000		// Force MDIX crossed

#define BMSR_AN_COMPLETE                0x0020
#define BMSR_LINK_STATUS                0x0004U

#define PHYSTS_LINK_STATUS              0x0001U  /* PHY Link mask */
#define PHYSTS_SPEED_STATUS             0x0002U  /* PHY Speed mask */
#define PHYSTS_DUPLEX_STATUS            0x0004U  /* PHY Duplex mask */
/* Some defines used internally here to indicate preferences about speed, MDIX
(wired direct or crossed), and duplex (half or full). */
#define	PHY_SPEED_10       1
#define	PHY_SPEED_100      2
#define	PHY_SPEED_AUTO     (PHY_SPEED_10|PHY_SPEED_100)

#define	PHY_MDIX_DIRECT    1
#define	PHY_MDIX_CROSSED   2
#define	PHY_MDIX_AUTO      (PHY_MDIX_CROSSED|PHY_MDIX_DIRECT)

#define	PHY_DUPLEX_HALF    1
#define	PHY_DUPLEX_FULL    2
#define	PHY_DUPLEX_AUTO    (PHY_DUPLEX_FULL|PHY_DUPLEX_HALF)

enum LINK_EVENT_et
{
  LINK_OFF,
  LINK_ON,
  LINK_CHANGED_UP,
  LINK_CHANGED_DOWN
};

class EthernetPhy_c
{

  uint8_t actLinkStatus;
  uint32_t phySpeed;
  uint32_t phyDuplex;

  uint32_t ulBCRValue;
  uint32_t ulACRValue;

  

  ETH_HandleTypeDef* heth_p;

  bool PhyStartAutoNegotiation(void);

public:
  bool GetLinkStatus(void) { return actLinkStatus == 1; }

  static TaskHandle_t taskToNotifyOnTimeout;

  EthernetPhy_c(void);

  void ProvideData(ETH_HandleTypeDef* heth_, TaskHandle_t task) {heth_p = heth_;  taskToNotifyOnTimeout = task; }

  void Init(void);
  bool UpdateConfig( BaseType_t xForce );
  bool PhyReset( void);
  
  LINK_EVENT_et CheckLinkStatus(void);


};








#endif