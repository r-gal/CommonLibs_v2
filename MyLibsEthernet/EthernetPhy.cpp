
#include <stdlib.h>
#include <cstring>

#define configSUPPORT_DYNAMIC_ALLOCATION 1

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "EthernetPhy.hpp"
#include "Ethernet.hpp"

#include "SignalList.hpp"

TaskHandle_t EthernetPhy_c::taskToNotifyOnTimeout = nullptr;



EthernetPhy_c::EthernetPhy_c(void)
{
  phySpeed = PHY_SPEED_100;
  phyDuplex = PHY_DUPLEX_FULL;
  actLinkStatus = 0;


}



void EthernetPhy_c::Init(void)
{

  

  uint32_t ulConfig, ulAdvertise;
    BaseType_t xPhyIndex;

    /* Set advertise register. */
    if( ( phySpeed == ( uint8_t ) PHY_SPEED_AUTO ) && ( phyDuplex == ( uint8_t ) PHY_DUPLEX_AUTO ) )
    {
        ulAdvertise = ADVERTISE_ALL;
        /* Reset auto-negotiation capability. */
    }
    else
    {
        /* Always select protocol 802.3u. */
        ulAdvertise = ADVERTISE_CSMA;

        if( phySpeed == ( uint8_t ) PHY_SPEED_AUTO )
        {
            if( phyDuplex == ( uint8_t ) PHY_DUPLEX_FULL )
            {
                ulAdvertise |= ADVERTISE_10FULL | ADVERTISE_100FULL;
            }
            else
            {
                ulAdvertise |= ADVERTISE_10HALF | ADVERTISE_100HALF;
            }
        }
        else if( phyDuplex == ( uint8_t ) PHY_DUPLEX_AUTO )
        {
            if( phySpeed == ( uint8_t ) PHY_SPEED_10 )
            {
                ulAdvertise |= ADVERTISE_10FULL | ADVERTISE_10HALF;
            }
            else
            {
                ulAdvertise |= ADVERTISE_100FULL | ADVERTISE_100HALF;
            }
        }
        else if( phySpeed == ( uint8_t ) PHY_SPEED_100 )
        {
            if( phyDuplex == ( uint8_t ) PHY_DUPLEX_FULL )
            {
                ulAdvertise |= ADVERTISE_100FULL;
            }
            else
            {
                ulAdvertise |= ADVERTISE_100HALF;
            }
        }
        else
        {
            if( phyDuplex == ( uint8_t ) PHY_DUPLEX_FULL )
            {
                ulAdvertise |= ADVERTISE_10FULL;
            }
            else
            {
                ulAdvertise |= ADVERTISE_10HALF;
            }
        }
    }

    /* Send a reset command  */
    PhyReset( );

    HAL_ETH_WritePHYRegister(heth_p,PHY_ADR, PHY_REG_04_ADVERTISE, ulAdvertise);
    
    /* Read Control register. */
    HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_00_BMCR, &ulConfig );

    ulConfig &= ~( BMCR_SPEED100 | BMCR_FULLDPLX );

    ulConfig |= BMCR_ANENABLE;

    if( ( phySpeed == ( uint8_t ) PHY_SPEED_100 ) || ( phySpeed == ( uint8_t ) PHY_SPEED_AUTO ) )
    {
        ulConfig |= BMCR_SPEED100;
    }
    else if( phySpeed == ( uint8_t ) PHY_SPEED_10 )
    {
        ulConfig &= ~BMCR_SPEED100;
    }

    if( ( phyDuplex == ( uint8_t ) PHY_DUPLEX_FULL ) || ( phyDuplex == ( uint8_t ) PHY_DUPLEX_AUTO ) )
    {
        ulConfig |= BMCR_FULLDPLX;
    }
    else if( phyDuplex == ( uint8_t ) PHY_DUPLEX_HALF )
    {
        ulConfig &= ~BMCR_FULLDPLX;
    }

    #if DEBUG_ETHERNET > 0
    printf(  "+TCP: advertise: %04lX config %04lX\n", ulAdvertise, ulConfig  );
    #endif

    /* Keep these values for later use. */
    ulBCRValue = ulConfig & ~BMCR_ISOLATE;
    ulACRValue = ulAdvertise;


}

bool EthernetPhy_c::UpdateConfig( BaseType_t xForce )
{
ETH_MACConfigTypeDef MACConf;
    uint32_t speed = 0, duplex = 0;
    #if DEBUG_ETHERNET > 0
    printf(  "prvEthernetUpdateConfig: LS mask %02lX Force %d\n",
                       actLinkStatus,
                       ( int ) xForce );
    #endif

    bool autoNegotiationResult = false;                     
    if( ( xForce != pdFALSE ) || ( actLinkStatus != 0 ) )
    {
        /* Restart the auto-negotiation. */
        autoNegotiationResult = PhyStartAutoNegotiation( );
    }
    if(autoNegotiationResult)
    {
        /* Configure the MAC with the Duplex Mode fixed by the
         * auto-negotiation process. */
        if( phyDuplex == PHY_DUPLEX_FULL )
        {
            duplex = ETH_FULLDUPLEX_MODE;
        }
        else
        {
            duplex = ETH_HALFDUPLEX_MODE;
        }

        /* Configure the MAC with the speed fixed by the
         * auto-negotiation process. */
        if( phySpeed == PHY_SPEED_10 )
        {
            speed = ETH_SPEED_10M;
        }
        else
        {
            speed = ETH_SPEED_100M;
        }

        /* Get MAC and configure it */
        HAL_ETH_GetMACConfig( heth_p, &( MACConf ) );
        MACConf.DuplexMode = duplex;
        MACConf.Speed = speed;
        HAL_ETH_SetMACConfig( heth_p, &( MACConf ) );
        #if ( ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM != 0 )
            {
                MACConf.ChecksumOffload = ENABLE;
            }
        #else
            {
                MACConf.ChecksumOffload = DISABLE;
            }
        #endif /* ( ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM != 0 ) */

        /* Restart MAC interface */
        HAL_ETH_Start_IT(heth_p );
        return true;
    }
    else
    {
        //FreeRTOS_NetworkDown();
        /* Stop MAC interface */
        HAL_ETH_Stop_IT( heth_p );

        actLinkStatus = 0; 

        return false;
    }

}


/* Send a reset command to a set of PHY-ports. */
bool EthernetPhy_c::PhyReset( void)
{
    uint32_t ulConfig;
    TickType_t xRemainingTime;
    TimeOut_t xTimer;


    /* Set the RESET bits high. */
    HAL_GPIO_WritePin(ETH_RESET_GPIO_Port,ETH_RESET_Pin,GPIO_PIN_RESET);
   vTaskDelay( pdMS_TO_TICKS( PHY_SHORT_DELAY_MS ) );

    HAL_GPIO_WritePin(ETH_RESET_GPIO_Port,ETH_RESET_Pin,GPIO_PIN_SET);

    /* Read Control register. */


    /* Read ID register. */
    uint32_t id1,id2;
    HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_02_PHYSID1, &id1 );
    HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_03_PHYSID2, &id2 );
    #if DEBUG_ETHERNET > 0
      printf("ETHPHY ID=%X,%x\n",id1,id2);

    #endif


    HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_00_BMCR, &ulConfig );
    HAL_ETH_WritePHYRegister(heth_p,PHY_ADR, PHY_REG_00_BMCR, ulConfig | BMCR_RESET );


    xRemainingTime = ( TickType_t ) pdMS_TO_TICKS( PHY_MAX_RESET_TIME_MS );
    vTaskSetTimeOutState( &xTimer );

    bool done = false;
    /* The reset should last less than a second. */
    for( ; ; )
    {

        HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_00_BMCR, &ulConfig );

        if( ( ulConfig & BMCR_RESET ) == 0 )
        {
            #if DEBUG_ETHERNET > 0
            printf(  "xPhyReset: BMCR_RESET ready\n"  );
            #endif
            done = true;
        }


        if( done )
        {
            break;
        }

        if( xTaskCheckForTimeOut( &xTimer, &xRemainingTime ) != pdFALSE )
        {
            #if DEBUG_ETHERNET > 0
            printf(  "xPhyReset: BMCR_RESET timed out \n"  );
            #endif
            break;
        }

        /* Block for a while */
        vTaskDelay( pdMS_TO_TICKS( PHY_SHORT_DELAY_MS ) );
    }

    /* Clear the reset bits. */

    if( done == false )
    {
        /* The reset operation timed out, clear the bit manually. */
        HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_00_BMCR, &ulConfig );
        HAL_ETH_WritePHYRegister(heth_p,PHY_ADR, PHY_REG_00_BMCR, ulConfig & ~BMCR_RESET );
    }


    vTaskDelay( pdMS_TO_TICKS( PHY_SHORT_DELAY_MS ) );

    return done;
}

bool EthernetPhy_c::PhyStartAutoNegotiation(void)
{
  uint32_t ulPHYLinkStatus, ulRegValue;
  TickType_t xRemainingTime;
  TimeOut_t xTimer;
  bool done;

  /* Enable Auto-Negotiation. */
  HAL_ETH_WritePHYRegister(heth_p,PHY_ADR, PHY_REG_04_ADVERTISE, ulACRValue );
  HAL_ETH_WritePHYRegister(heth_p,PHY_ADR, PHY_REG_00_BMCR, ulBCRValue | BMCR_ANRESTART );

  xRemainingTime = ( TickType_t ) pdMS_TO_TICKS( PHY_MAX_NEGOTIATE_TIME_MS );
  vTaskSetTimeOutState( &xTimer );


  /* Wait until the auto-negotiation will be completed */
  done = false;
  for( ; ; )
  {

      HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_01_BMSR, &ulRegValue );

      if( ( ulRegValue & BMSR_AN_COMPLETE ) != 0 )
      {
          done = true;
      }


      if( done )
      {
          break;
      }

      if( xTaskCheckForTimeOut( &xTimer, &xRemainingTime ) != pdFALSE )
      {
          #if DEBUG_ETHERNET > 0
          printf(  "xPhyStartAutoNegotiation: phyBMSR_AN_COMPLETE timed out \n");
          #endif
          break;
      }

      vTaskDelay( pdMS_TO_TICKS( PHY_SHORT_DELAY_MS ) );
  }

  if( done )
  {

    /* Clear the 'phyBMCR_AN_RESTART'  bit. */
    HAL_ETH_WritePHYRegister(heth_p,PHY_ADR, PHY_REG_00_BMCR, ulBCRValue );

    HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_01_BMSR, &ulRegValue );

    if( ( ulRegValue & BMSR_LINK_STATUS ) != 0 )
    {
        ulPHYLinkStatus |= BMSR_LINK_STATUS;


    }
    else
    {
        ulPHYLinkStatus &= ~( BMSR_LINK_STATUS );
        done = false;
    }

    HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_10_PHYSTS, &ulRegValue );
    #if DEBUG_ETHERNET > 0
    printf(  "Autonego ready: %08lx: %s duplex %u mbit %s status\n",
                       ulRegValue,
                       ( ulRegValue & PHYSTS_DUPLEX_STATUS ) ? "full" : "half",
                       ( ulRegValue & PHYSTS_SPEED_STATUS ) ? 10 : 100,
                       ( ( ulPHYLinkStatus |= BMSR_LINK_STATUS ) != 0 ) ? "high" : "low"  );
    #endif
    if( ( ulRegValue & PHYSTS_DUPLEX_STATUS ) != ( uint32_t ) 0U )
    {
        phyDuplex = PHY_DUPLEX_FULL;
    }
    else
    {
        phyDuplex = PHY_DUPLEX_HALF;
    }

    if( ( ulRegValue & PHYSTS_SPEED_STATUS ) != 0 )
    {
        phySpeed = PHY_SPEED_10;
    }
    else
    {
        phySpeed = PHY_SPEED_100;
    }

      
  } 
  return done;
}

LINK_EVENT_et EthernetPhy_c::CheckLinkStatus(void)
{
  #ifdef ETH_TEST
    return LINK_ON;
  #endif

  uint32_t ulRegValue, newLinkStatus;

  LINK_EVENT_et resp;

  HAL_ETH_ReadPHYRegister(heth_p,PHY_ADR, PHY_REG_01_BMSR, &ulRegValue );
  
  if( ( ulRegValue & BMSR_LINK_STATUS ) != 0 )
  {
      newLinkStatus = 1;
  }
  else
  {
      newLinkStatus = 0;
  }

  if(newLinkStatus != actLinkStatus)
  {
    #if DEBUG_ETHERNET > 0
    printf("Link status change, new = %x\n",newLinkStatus);
    #endif
    actLinkStatus = newLinkStatus;

    uint8_t r;
    if(actLinkStatus == 1)
    {
      resp = LINK_CHANGED_UP;
      //LinkStateChanged(1);
    }
    else
    {
      resp = LINK_CHANGED_DOWN;
      //LinkStateChanged(0);
    }


  }
  else if(actLinkStatus == 1)
  {
    resp = LINK_ON;
  }
  else
  {
    resp = LINK_OFF;
  }
  return resp;

}



