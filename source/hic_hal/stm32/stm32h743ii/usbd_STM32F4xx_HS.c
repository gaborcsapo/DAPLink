/*----------------------------------------------------------------------------
 *      RL-ARM - USB
 *----------------------------------------------------------------------------
 *      Name:    usbd_STM32F4xx_HS.c
 *      Purpose: Hardware Layer module for ST STM32F407
 *      Rev.:    V4.70
 *----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2013 KEIL - An ARM Company. All rights reserved.
 *---------------------------------------------------------------------------*/


//#define STM32F429X //elee: hack for now.
//todo(elee): ideally cleanup USBx_DEVICE vs OTG (they point to the same address).


// #include <RTL.h>
#include <rl_usb.h>
//#include <stm32f4xx.h>
#include "stm32h7xx.h" //elee: try to swap out directly.
#include "cmsis_gcc.h"
#include "usb_def.h"

#define __NO_USB_LIB_C
//#include "usb_config_HS.c"  //elee: didn't find a sample from a bit of searching, not sure of the contents.  Let's see if the "non _HS" will work...
#include "usb_config.c"

//#define OTG                 OTG_HS	//F4
#define OTG                 USB1_OTG_HS //H7

//Setup for H7.  Registers are no longer in one large structure, is split into smaller structures.
/*  From stm32h7xx_ll_usb.h
#define USBx_DEVICE     ((USB_OTG_DeviceTypeDef *)(USBx_BASE + USB_OTG_DEVICE_BASE))
#define USBx_INEP(i)    ((USB_OTG_INEndpointTypeDef *)(USBx_BASE + USB_OTG_IN_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_OUTEP(i)   ((USB_OTG_OUTEndpointTypeDef *)(USBx_BASE + USB_OTG_OUT_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_DFIFO(i)   *(__IO uint32_t *)(USBx_BASE + USB_OTG_FIFO_BASE + ((i) * USB_OTG_FIFO_SIZE))
*/
//Get a pointer to the HS USB registers.
#define USBx_BASE				USB1_OTG_HS_PERIPH_BASE
#define USBx_DEVICE     ((USB_OTG_DeviceTypeDef *)(USBx_BASE + USB_OTG_DEVICE_BASE))
#define USBx_INEP(i)    ((USB_OTG_INEndpointTypeDef *)(USBx_BASE + USB_OTG_IN_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_OUTEP(i)   ((USB_OTG_OUTEndpointTypeDef *)(USBx_BASE + USB_OTG_OUT_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))



#define RX_FIFO_SIZE        1024
#define TX0_FIFO_SIZE       64
#define TX1_FIFO_SIZE       512
#define TX2_FIFO_SIZE       512
#define TX3_FIFO_SIZE       512
#define TX4_FIFO_SIZE       512

#define TX_FIFO(n)          *((__packed volatile uint32_t*)(USB1_OTG_HS + 0x1000 + n*0x1000))
#define RX_FIFO             *((__packed volatile uint32_t*)(USB1_OTG_HS + 0x1000))
	//elee: modify to this format. h7xx_ll_usb.c
#define USBx_DFIFO(i)   *(__IO uint32_t *)(USBx_BASE + USB_OTG_FIFO_BASE + ((i) * USB_OTG_FIFO_SIZE))


//todo: elee: fix this, instead of replacing below...
#define DIEPTSIZ(EPNum)     *(&USBx_INEP(EPNum)->DIEPTSIZ)	//*(&OTG->DIEPTSIZ0 + EPNum * 8)
#define DIEPCTL(EPNum)      *(&USBx_INEP(EPNum)->DIEPCTL)	//*(&OTG->DIEPCTL0  + EPNum * 8)	//elee: fix this, instead of replacing below...
#define DTXFSTS(EPNum)      *(&USBx_INEP(EPNum)->DTXFSTS)	//*(&OTG->DTXFSTS0  + EPNum * 8)
#define DOEPTSIZ(EPNum)     *(&USBx_OUTEP(EPNum)->DOEPTSIZ)	//*(&OTG->DOEPTSIZ0 + EPNum * 8)
#define DOEPCTL(EPNum)      *(&USBx_OUTEP(EPNum)->DOEPCTL)	//*(&OTG->DOEPCTL0  + EPNum * 8)
#define DIEPINT(EPNum)      *(&USBx_INEP(EPNum)->DIEPINT)	//*(&OTG->DIEPINT0  + EPNum * 8)
#define DOEPINT(EPNum)      *(&USBx_OUTEP(EPNum)->DOEPINT)	//*(&OTG->DOEPINT0  + EPNum * 8)

#define EP_IN_TYPE(num)      ((DIEPCTL(num) >> 18) & 3)	//00: Control, 01: Isochronous, 10: Bulk, 11: Interrupt
#define EP_OUT_TYPE(num)     ((DOEPCTL(num) >> 18) & 3)

uint32_t OutMaxPacketSize[5] =   {USBD_MAX_PACKET0, 0, 0, 0, 0};
uint8_t  OutPacketCnt[5]     =   {1, 0, 0, 0, 0};
uint8_t  InPacketCnt[5]      =   {1, 0, 0, 0, 0};

#if (USBD_HID_ENABLE == 1)
uint32_t HID_IntInPacketData     [(USBD_HID_MAX_PACKET + 3) / 4];
#endif
#if (USBD_CDC_ACM_ENABLE == 1)
uint32_t CDC_ACM_IntInPacketData [(USBD_CDC_ACM_MAX_PACKET  + 3) / 4];
#endif

uint32_t *InPacketDataPtr[5] = { 0,
#if   ((USBD_HID_ENABLE     == 1) && (USBD_HID_EP_INTIN      == 1))
                       HID_IntInPacketData,
#elif ((USBD_CDC_ACM_ENABLE == 1) && (USBD_CDC_ACM_EP_INTIN  == 1))
                       CDC_ACM_IntInPacketData,
#else
                            0,
#endif

#if   ((USBD_HID_ENABLE     == 1) && (USBD_HID_EP_INTIN      == 2))
                       HID_IntInPacketData,
#elif ((USBD_CDC_ACM_ENABLE == 1) && (USBD_CDC_ACM_EP_INTIN  == 2))
                       CDC_ACM_IntInPacketData,
#else
                            0,
#endif

#if   ((USBD_HID_ENABLE     == 1) && (USBD_HID_EP_INTIN      == 3))
                       HID_IntInPacketData,
#elif ((USBD_CDC_ACM_ENABLE == 1) && (USBD_CDC_ACM_EP_INTIN  == 3))
                       CDC_ACM_IntInPacketData,
#else
                            0,
#endif

#if   ((USBD_HID_ENABLE     == 1) && (USBD_HID_EP_INTIN      == 4))
                       HID_IntInPacketData,
#elif ((USBD_CDC_ACM_ENABLE == 1) && (USBD_CDC_ACM_EP_INTIN  == 4))
                       CDC_ACM_IntInPacketData,
#else
                            0,
#endif
};


#if    (USBD_ADC_ENABLE == 1)

uint32_t ADC_IsoOutPacketData    [(USBD_ADC_WMAXPACKETSIZE + 3) / 4];

uint32_t *IsoOutPacketDataPtr[5] = { 0,
#if    (USBD_ADC_EP_ISOOUT == 1)
                                     ADC_IsoOutPacketData,
#else
                                     0,
#endif

#if    (USBD_ADC_EP_ISOOUT == 2)
                                     ADC_IsoOutPacketData,
#else
                                     0,
#endif

#if    (USBD_ADC_EP_ISOOUT == 3)
                                     ADC_IsoOutPacketData,
#else
                                     0,
#endif

#if    (USBD_ADC_EP_ISOOUT == 4)
                                     ADC_IsoOutPacketData,
#else
                                     0,
#endif
};
#else
uint32_t *IsoOutPacketDataPtr[5] = { 0 };
#endif


uint32_t InPacketDataCnt[5]      = { 0 };
uint32_t InPacketDataReady       =   0  ;
uint32_t SyncWriteEP             =   0  ;

uint32_t IsoOutPacketDataCnt[5]  = { 0 };
uint32_t IsoOutTokenRead         =   0  ;

/*
 *  usbd_stm32_delay
 *    Parameters:      delay:      Delay in 100 us ticks
 *    Return Value:    None
 */
void usbd_stm32_delay (uint32_t delay) {
  delay *= SystemCoreClock / 100000;
  while (delay--) {
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
  }
}


/*
 *  USB Device Interrupt enable
 *   Called by USBD_Init to enable the USB Interrupt
 *    Return Value:    None
 */

#ifdef __RTX
void __svc(1) USBD_IntrEna (void);
void __SVC_1               (void) {
#else
void          USBD_IntrEna (void) {
#endif
  NVIC_EnableIRQ   (OTG_HS_IRQn);       /* Enable OTG interrupt               */ /* OTG_HS global interrupt */
}


/*
 *  USB Device Initialize Function
 *   Called by the User to initialize USB
 *   Return Value:    None
 */

void USBD_Init (void) {
  int32_t tout;

#ifndef __OTG_HS_EMBEDDED_PHY
  RCC->AHB4ENR    |=  1 | (1 << 1) | (1 << 2) | (1 << 7) | (1 << 8);	/* GPIOAEN, GPIOBEN, GPIOCEN, GPIOHEN, GPIOIEN.  For H743 is in AHB4ENR, not AHB1ENR(F4xx) */

  /* ULPI data pins                                                           */
  /* PA3 (OTG_HS_ULPI alternate function, DATA0)                              */
  GPIOA->MODER     =  (GPIOA->MODER  & ~(3  <<  6)) | (2  <<  6);
  GPIOA->OTYPER   &= ~(1  <<  3);
  GPIOA->OSPEEDR  |=  (3  <<  6);
  GPIOA->PUPDR    &= ~(3  <<  6);
  GPIOA->AFR[0]    =  (GPIOA->AFR[0] & ~(15 << 12)) | (10 << 12);	/* Alt Func 10 for ULPI */

  /* PB0, PB1 (OTG_HS_ULPI alternate function, DATA1, DATA2)                  */
  GPIOB->MODER     =  (GPIOB->MODER  & ~(15 <<  0)) | (10 <<  0);
  GPIOB->OTYPER   &= ~(3  <<  0);
  GPIOB->OSPEEDR  |=  (15 <<  0);
  GPIOB->PUPDR    &= ~(15 <<  0);
  GPIOB->AFR[0]    =  (GPIOB->AFR[0] & ~(0xFF <<  0)) | (0xAA <<  0);

  /* PB10..13 (OTG_HS_ULPI alternate function, DATA3 to DATA6)                */
  GPIOB->MODER     =  (GPIOB->MODER  & ~(0xFF << 20)) | (0xAA << 20);
  GPIOB->OTYPER   &= ~(15 << 10);
  GPIOB->OSPEEDR  |=  (0xFF << 20);
  GPIOB->PUPDR    &= ~(0xFF << 20);
  GPIOB->AFR[1]    =  (GPIOB->AFR[1] & ~(0xFFFF << 8)) | (0xAAAA << 8);

  /* PB5 (OTG_HS_ULPI alternate function, DATA7)                              */
  GPIOB->MODER     =  (GPIOB->MODER  & ~(3  <<  10)) | (2  <<  10);
  GPIOB->OTYPER   &= ~(1  <<  5);
  GPIOB->OSPEEDR  |=  (3  << 10);
  GPIOB->PUPDR    &= ~(3  << 10);
  GPIOB->AFR[0]    =  (GPIOB->AFR[0] & ~(15 <<  20)) | (10 <<  20);

  /* ULPI control pins                                                        */
  /* PC0 (OTG_HS_ULPI alternate function, STP)                                */
  GPIOC->MODER     =  (GPIOC->MODER  & ~(3  <<   0)) | (2  <<  0);
  GPIOC->OSPEEDR  |=  (3  <<  0);
  GPIOC->AFR[0]    =  (GPIOC->AFR[0] & ~(15 <<   0)) | (10 <<  0);

  /*PI11 (OTG_HS_ULPI alternate functon, DIR)                                 */
  GPIOI->MODER     =  (GPIOI->MODER  & ~(3  <<  22)) | (2  << 22);
  GPIOI->OSPEEDR  |=  (3  << 22);
  GPIOI->AFR[1]    =  (GPIOI->AFR[1] & ~(15 <<  12)) | (10 << 12);

  /* PH4 (OTG_HS_ULPI alternate function, NXT)                                */
  GPIOH->MODER     =  (GPIOH->MODER  & ~(3  <<   8)) | (2  <<  8);
  GPIOH->OSPEEDR  |=  (3  <<  8);
  GPIOH->AFR[0]    =  (GPIOH->AFR[0] & ~(15 <<  16)) | (10 << 16);

  /* PA5 (OTG_HS_ULPI alternate function, CLOCK)                              */
  GPIOA->MODER     =  (GPIOA->MODER  & ~(3  <<  10)) | (2  << 10);
  GPIOA->OSPEEDR  |=  (3  << 10);
  GPIOA->AFR[0]    =  (GPIOA->AFR[0] & ~(15 <<  20)) | (10 << 20);
#else
  RCC->AHB1ENR    |=  (1 << 1);         /* Enable clock for Port B            */

  /* Configure PB14 and PB15 as alternate OTG_HS_DM and OTG_HS_DP pins        */
  GPIOB->MODER     =  (GPIOB->MODER  & ~(0x0FUL << 28)) | (0x0AUL << 28);
  GPIOB->OTYPER   &= ~(3 << 14);
  GPIOB->AFR[1]    =  (GPIOB->AFR[1] & ~(0xFFUL << 24)) | (0xCCUL << 24);	/* Alt Func 12 for PB14 and PB15 */
  GPIOB->OSPEEDR  |=  (15UL << 28);
  GPIOB->PUPDR    &= ~(15UL << 28);
#endif

  RCC->AHB1ENR    |=  (1 << 25);        /* Enable clock for OTG HS            */  /* H743, USB1OTGHSEN is bit 25 */
  usbd_stm32_delay    (100);            /* Wait ~10 ms                        */
  RCC->AHB1RSTR   |=  (1 << 25);        /* Reset OTG HS clock                 */  /* H743, USB1OTGRST */
  usbd_stm32_delay    (100);            /* Wait ~10 ms                        */
  RCC->AHB1RSTR   &= ~(1 << 25);
  usbd_stm32_delay    (400);            /* Wait ~40 ms                        */
#ifndef __OTG_HS_EMBEDDED_PHY
  RCC->AHB1ENR    |=  (1 << 26);        /* Enable clock for OTG HS ULPI       */	/* H743, USB1OTGHSULPIEN */
#endif

#ifdef __OTG_HS_EMBEDDED_PHY
  OTG->GUSBCFG    |=  (1 <<  6) ;       /* PHYSEL=1                           */
#endif
  usbd_stm32_delay    (200);            /* Wait ~20 ms                        */
  tout = 1000;                          /* Wait max 1 s for AHBIDL = 0        */
  while (!(OTG->GRSTCTL & (1UL << 31))) {
    if (!tout) break;
    tout --;
    usbd_stm32_delay  (10);             /* Wait 1 ms                          */
  }
  OTG->GRSTCTL    |=  1;                /* reset otg core                     */	/* H743, is renamed HSRST -> PSRST.  Looks ok to reset though */
  tout = 1000;                          /* Wait max 1 s for CRST = 0          */
  while (OTG->GRSTCTL & (1 << 0)) {
    if (!tout) break;
    tout --;
    usbd_stm32_delay  (10);             /* Wait 1 ms                          */
  }
  usbd_stm32_delay    (30);             /* Wait 3 ms                          */

  OTG->GAHBCFG    &=  ~1;               /* Disable interrupts                 */	/* GINTMSK */
  OTG->GCCFG      &=  ~(1 << 21);        /* No VBUS sensing                    */	/* H743, looks like name/polarity changed.  NOVBUSSENS -> VBDEN ToDo(elee): do we want this disabled?*/
  //OTG->DCTL       |=  (1 <<  1);        /* soft disconnect enabled            */
	USBx_DEVICE->DCTL       |=  (1 <<  1);        /* soft disconnect enabled            */

  OTG->GUSBCFG    |=  (1 << 30);        /* Force device mode                  */ /* FDMOD */
  usbd_stm32_delay    (1000);           /* Wait min 25 ms, we wait ~100 ms    */

#ifndef __OTG_HS_EMBEDDED_PHY
  //OTG->DCFG       &= ~3;                /* High speed phy                     */
	USBx_DEVICE->DCFG       &= ~3;                /* High speed phy                     */

	//TESTING, elee: set to "01: Full speed using HS" for testing.  The line above clears the 2 lower bits, this sets the lowest bit.
	USBx_DEVICE->DCFG				|= 1;


#else
  OTG->DCFG       |=  3;                /* Full speed phy                     */
#endif

  OTG->GINTMSK     = (1   << 11) |      /* suspend int unmask                 */
                     (1   << 12) |      /* reset int unmask                   */
                     (1   << 13) |      /* enumeration done int unmask        */
                     (1   << 4 ) |      /* receive fifo non-empty int  unmask */
                     (1   << 18) |      /* IN EP int unmask                   */
                     (1   << 19) |      /* OUT EP int unmask                  */
                     (1UL << 31) |      /* resume int unmask                  */
#ifdef __RTX				/* elee: isn't RTX used for DAPLink?  Curious why this isn't defined. */
  ((USBD_RTX_DevTask   != 0) ? (1 <<  3) : 0);   /* SOF int unmask            */
#else
  ((USBD_P_SOF_Event   != 0) ? (1 <<  3) : 0);   /* SOF int unmask            */
#endif

  USBD_IntrEna();                       /* Enable OTG interrupt               */
  OTG->GAHBCFG    |=  1 | (1 << 7);     /* Enable interrupts                  */ /* GINTMSK, TXFELVL */
}


/*
 *  USB Device Connect Function
 *   Called by the User to Connect/Disconnect USB Device
 *    Parameters:      con:   Connect/Disconnect
 *    Return Value:    None
 */

void USBD_Connect (BOOL con) {
  if (con) {
#ifdef __OTG_HS_EMBEDDED_PHY
    OTG->GCCFG  |=  (1 << 16);          /* power down deactivated             */
#endif
    USBx_DEVICE->DCTL   &= ~(1 <<  1);          /* soft disconnect disabled           */
  }
  else {
    USBx_DEVICE->DCTL   |=  (1 <<  1);          /* soft disconnect enabled            */
#ifdef __OTG_HS_EMBEDDED_PHY
    OTG->GCCFG  &= ~(1 << 16);          /* power down activated               */
#endif
  }
}


/*
 *  USB Device Reset Function
 *   Called automatically on USB Device Reset
 *    Return Value:    None
 */

void USBD_Reset (void) {
  uint32_t i;

  SyncWriteEP       = 0;
  InPacketDataReady = 0;
  USBx_DEVICE->DOEPMSK      = 0;
  USBx_DEVICE->DIEPMSK      = 0;

  for (i = 0; i < (USBD_EP_NUM + 1); i++) {
    if (DOEPCTL(i) & (1UL << 31)) {
      DOEPCTL(i)  = (1 << 30) | (1 << 27);   /* OUT EP disable, Set NAK       */
    }
    if (USBx_INEP(i)->DIEPCTL & (1UL << 31))
      USBx_INEP(i)->DIEPCTL  = (1 << 30) | (1 << 27);   /* IN EP disable, Set NAK        */

    DIEPINT(i)    = 0x1B;                    /* clear IN Ep interrupts        */   /* Todo(elee): confirm no other interrupts matter */
    DOEPINT(i)    = 0x1B;                    /* clear OUT Ep interrupts       */
  }

  USBD_SetAddress(0 , 1);

  USBx_DEVICE->DAINTMSK  = (1 << 16) |          /* unmask IN&OUT EP0 interruts        */
                    1;
  USBx_DEVICE->DOEPMSK   = (1 << 3) |           /* setup phase done                   */
                   (1 << 1) |           /* endpoint disabled                  */
                    1;                  /* transfer complete                  */
  USBx_DEVICE->DIEPMSK   = (1 << 1) |           /* endpoint disabled                  */
                    1;                  /* transfer completed                 */

  OTG->GRXFSIZ   =  RX_FIFO_SIZE/4;

  //OTG->TX0FSIZ   = (RX_FIFO_SIZE/4) | ((TX0_FIFO_SIZE/4) << 16);
	OTG->DIEPTXF0_HNPTXFSIZ   = (RX_FIFO_SIZE/4) | ((TX0_FIFO_SIZE/4) << 16);

	//elee: I believe DIEPTXF1 -> DIEPTXF[0] when going from stm32f4xx to stm32h743xx  (to __IO uint32_t DIEPTXF[0x0F];)
  OTG->DIEPTXF[0]  = ((RX_FIFO_SIZE + TX0_FIFO_SIZE)/4) |
                   ((TX1_FIFO_SIZE/4) << 16);

	//DIEPTXF2
  OTG->DIEPTXF[1]  = ((RX_FIFO_SIZE + TX0_FIFO_SIZE + TX1_FIFO_SIZE)/4) |
                   ((TX2_FIFO_SIZE/4) << 16);

	//DIEPTXF3
  OTG->DIEPTXF[2]  = ((RX_FIFO_SIZE + TX0_FIFO_SIZE+ TX1_FIFO_SIZE +TX2_FIFO_SIZE)/4) |
                   ((TX3_FIFO_SIZE/4) << 16);

	//DIEPTXF4
  OTG->DIEPTXF[3]  = ((RX_FIFO_SIZE + TX0_FIFO_SIZE + TX1_FIFO_SIZE + TX2_FIFO_SIZE + TX3_FIFO_SIZE)/4) |
                   ((TX4_FIFO_SIZE/4) << 16);

  DOEPTSIZ(0) =  (1 << 29) |         /* setup count = 1                    */
                    (1 << 19) |         /* packet count                       */
                     USBD_MAX_PACKET0;
}


/*
 *  USB Device Suspend Function
 *   Called automatically on USB Device Suspend
 *    Return Value:    None
 */

void USBD_Suspend (void) {
}


/*
 *  USB Device Resume Function
 *   Called automatically on USB Device Resume
 *    Return Value:    None
 */

void USBD_Resume (void) {
}


/*
 *  USB Device Remote Wakeup Function
 *   Called automatically on USB Device Remote Wakeup
 *    Return Value:    None
 */

void USBD_WakeUp (void) {
  USBx_DEVICE->DCTL |= 1;                       /* remote wakeup signaling            */
  usbd_stm32_delay (50);                /* Wait ~5 ms                         */
  USBx_DEVICE->DCTL &= ~1;
}


/*
 *  USB Device Remote Wakeup Configuration Function
 *    Parameters:      cfg:   Device Enable/Disable
 *    Return Value:    None
 */

void USBD_WakeUpCfg (BOOL cfg) {
  /* Not needed                                                               */
}


/*
 *  USB Device Set Address Function
 *    Parameters:      adr:   USB Device Address
 *    Return Value:    None
 */

void USBD_SetAddress (U32  adr, U32 setup) {
  if (setup) {
    USBx_DEVICE->DCFG = (USBx_DEVICE->DCFG & ~(0x7f << 4)) | (adr << 4);
  }
}

/*
 *  USB Device Flush IN Endpoint Transmit Fifo
 *    Parameters:      adr:   USB Device Address
 *    Return Value:    None
 */
static void USBD_FlushInEpFifo (uint32_t EPNum) {
  uint32_t wcnt;

  EPNum &= ~0x80;
  OTG->GRSTCTL = (OTG->GRSTCTL & ~(0x1F << 6)) | /* flush EP fifo             */
                 (EPNum << 6)| (1 << 5);
  wcnt = 10;
  while (OTG->GRSTCTL & (1 << 5))       /* wait until fifo is flushed         */
    if ((wcnt--) == 0) break;
}


/*
 *  USB Device Configure Function
 *    Parameters:      cfg:   Device Configure/Deconfigure
 *    Return Value:    None
 */

void USBD_Configure (BOOL cfg) {
  InPacketDataReady &= ~1UL;
}


/*
 *  Configure USB Device Endpoint according to Descriptor
 *    Parameters:      pEPD:  Pointer to Device Endpoint Descriptor
 *    Return Value:    None
 */

void USBD_ConfigEP (USB_ENDPOINT_DESCRIPTOR *pEPD) {
  uint32_t num, val, type;

  num  = pEPD->bEndpointAddress & ~(0x80);
  val  = pEPD->wMaxPacketSize;
  type = pEPD->bmAttributes & USB_ENDPOINT_TYPE_MASK;

  if (pEPD->bEndpointAddress & USB_ENDPOINT_DIRECTION_MASK) {
    if (type == USB_ENDPOINT_TYPE_ISOCHRONOUS)
      InPacketCnt[num] = (val >> 11) & 3;
    else InPacketCnt[num] = 1;

    USBx_DEVICE->DAINTMSK |= (1    << num);     /* unmask IN EP int                   */
    USBx_INEP(num)->DIEPCTL   = (num  <<  22) |    /* fifo number                        */
                     (type <<  18) |    /* ep type                            */
                      val & 0x7FF;      /* max packet size                    */
    if ((type & 3) > 1)                 /* if interrupt or bulk EP            */
      USBx_INEP(num)->DIEPCTL |= (1 << 28);        /* DATA0 PID                          */
  } else {
    OutMaxPacketSize[num] = val & 0x7FF;
    if (type == USB_ENDPOINT_TYPE_ISOCHRONOUS)
      OutPacketCnt[num]   = (val >> 11) & 3;
    else OutPacketCnt[num] = 1;

    USBx_DEVICE->DAINTMSK |= (1 << (num + 16)); /* unmask OUT EP int                  */

    DOEPCTL(num)   = (type <<  18)|     /* EP type                            */
                     (val & 0x7FF);     /* max packet size                    */

    DOEPTSIZ(num)  = (OutPacketCnt[num] << 19) |  /* packet count             */
                     (val  & 0x7FF);    /* transfer size                      */
    if ((type & 3) > 1)                 /* if int or bulk EP                  */
      DOEPCTL(num) |= (1 << 28);        /* DATA0 PID                          */
  }
}


/*
 *  Set Direction for USB Device Control Endpoint
 *    Parameters:      dir:   Out (dir == 0), In (dir <> 0)
 *    Return Value:    None
 */

void USBD_DirCtrlEP (U32 dir) {
  /* Not needed                                                               */
}


/*
 *  Enable USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_EnableEP (U32 EPNum) {
  if (EPNum & 0x80) {
    EPNum &= ~0x80;
    USBx_INEP(EPNum)->DIEPCTL    |= (1   << 15) |  /* EP active                          */
                         (1   << 27);   /* set EP NAK                         */
    if (USBx_INEP(EPNum)->DIEPCTL &  (1UL << 31))
      USBx_INEP(EPNum)->DIEPCTL  |= (1   << 30);   /* disable EP                         */

    InPacketDataReady &= ~(1 << EPNum);
    if (EP_IN_TYPE(EPNum) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
      OTG->GINTMSK |= (1 << 20);        /* enable IISOIXFR                    */
    }
  } else {
    if (EP_OUT_TYPE(EPNum) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
      OTG->GINTMSK   |= (1 << 15);      /* enable end of periodic frame       */
      DOEPCTL(EPNum) |= (1 << 15);      /* EP active                          */
    } else {
      DOEPCTL(EPNum)  |= (1   << 15) |  /* EP active                          */
                         (1UL << 31) |  /* enable EP                          */
                         (1   << 26);   /* clear EP NAK                       */
    }
  }
}


/*
 *  Disable USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_DisableEP (U32 EPNum) {
  uint32_t num, wcnt, IsoEpEnCnt;

  IsoEpEnCnt = 0;

  /* Disable IN Endpoint                                                      */
  if (EPNum & 0x80) {
    EPNum &= ~0x80;
    for (num = 1; num <= USBD_EP_NUM; num++) {
      if (DIEPCTL(num) & (1 <<15)) {
        if (EP_IN_TYPE(num) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
          IsoEpEnCnt++;
        }
      }
    }

    InPacketDataReady &= ~(1 << EPNum);
    if (IsoEpEnCnt == 1) {              /* if all iso endpoints disabled      */
      OTG->GINTMSK &= ~(1 << 20);       /* disable IISOIXFR                   */
    }
    if (USBx_INEP(EPNum)->DIEPCTL &   (1UL << 31))
      USBx_INEP(EPNum)->DIEPCTL   |=  (1   << 30); /* disable EP                         */
    USBx_INEP(EPNum)->DIEPCTL     |=  (1   << 27); /* set EP NAK                         */
    USBx_INEP(EPNum)->DIEPCTL     &= ~(1   << 15); /* deactivate EP                      */

  /* Disable OUT Endpoint                                                     */
  } else {
    for (num = 1; num <= USBD_EP_NUM; num++) {
      if (DOEPCTL(num) & (1 <<15)) {
        if (EP_OUT_TYPE(num) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
          IsoEpEnCnt++;
        }
      }
    }
    if (IsoEpEnCnt == 1)                /* if all iso endpoints disabled      */
      OTG->GINTMSK &= ~(1 << 15);       /* disable EOPF                       */

    USBx_DEVICE->DCTL |= (1 << 9);              /* set global out nak                 */

    wcnt = 1000;
    while (!(OTG->GINTSTS & (1 << 7)))  /* wait until global NAK              */
      if ((wcnt--) == 0) break;

    if (DOEPCTL(EPNum) &   (1UL << 31))   /* if ep enabled                    */
      DOEPCTL(EPNum)   |=  (1   << 30);   /* disable EP                       */
    DOEPCTL(EPNum)     |=  (1   << 27);   /* set EP NAK                       */
    DOEPCTL(EPNum)     &= ~(1   << 15);   /* deactivate EP                    */

    wcnt = 1000;
    while(!(DOEPINT(EPNum) & (1 << 1)))   /* wait until EP disabled           */
      if ((wcnt--) == 0) break;
    USBx_DEVICE->DCTL |= (1 << 10);               /* clear global nak                 */
  }
}


/*
 *  Reset USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_ResetEP (U32 EPNum) {

  /* Reset IN Endpoint                                                        */
  if (EPNum & 0x80) {
    EPNum &= ~0x80;
    InPacketDataReady &= ~(1 << EPNum);
    if (USBx_INEP(EPNum)->DIEPCTL &  (1UL << 31))  /* if Endpoint enabled                */
      USBx_INEP(EPNum)->DIEPCTL   |= (1   << 30);  /* disable EP                         */
    USBx_INEP(EPNum)->DIEPCTL     |= (1   << 27);  /* set EP NAK                         */

    USBD_FlushInEpFifo (EPNum | 0x80);  /* Flush endpoint fifo                */

    /* If endpoint is isochronous, set proper EVEN/ODD frame and enable Ep    */
    if (EP_IN_TYPE(EPNum) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
      if (USBD_GetFrame() & 1) USBx_INEP(EPNum)->DIEPCTL |= (1 << 28); /* set even frame */
      else USBx_INEP(EPNum)->DIEPCTL |= (1 << 29);                     /* set odd frame  */
      USBx_INEP(EPNum)->DIEPCTL  |= (1UL << 31) | (1 << 26);           /* enable EP      */
    }
  }
}


/*
 *  Set Stall for USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_SetStallEP (U32 EPNum) {
  uint32_t wcnt;

  /* Stall OUT Endpoint                                                       */
  if (!(EPNum & 0x80)) {
    USBx_DEVICE->DCTL |= (1 << 9);              /* set global out nak                 */
    wcnt = 1000;
    while (!(OTG->GINTSTS & (1 << 7)))    /* wait until global NAK            */
      if ((wcnt--) == 0) break;

    if (DOEPCTL(EPNum) &   (1UL << 31)) /* if endpoint enabled                */
      DOEPCTL(EPNum)   |=  (1   << 30); /* disable EP                         */
    DOEPCTL(EPNum) |= (1 << 21);        /* set stall                          */

    wcnt = 1000;
    while(!(DOEPINT(EPNum) & (1 << 1))) /* wait until EP disabled             */
      if ((wcnt--) == 0) break;

    USBx_DEVICE->DCTL |= (1 << 10);             /* clear global nak                   */

  /* Stall IN endpoint                                                        */
  } else {
    EPNum &= ~0x80;
    if (USBx_INEP(EPNum)->DIEPCTL &  (1UL << 31))  /* if endpoint enabled                */
      USBx_INEP(EPNum)->DIEPCTL   |= (1   << 30);  /* disable endpoint                   */
    USBx_INEP(EPNum)->DIEPCTL     |= (1   << 21);  /* set stall                          */

    USBD_FlushInEpFifo (EPNum | 0x80);  /* Flush endpoint fifo                */
  }
}


/*
 *  Clear Stall for USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_ClrStallEP (U32 EPNum) {
  /* Clear OUT endpoint Stall                                                 */
  if (!(EPNum & 0x80)) {

    if (((DOEPCTL(EPNum) >> 18) & 3) > 1) { /* if EP type Bulk or Interrupt   */
      DOEPCTL(EPNum) |= (1 << 28);      /* Set DATA0 PID                      */

    DOEPCTL(EPNum) &= ~(1 << 21);       /* Clear stall                        */
  }

  /* Clear IN Endpoint Stall                                                  */
  } else {
    EPNum &= ~0x80;

    if (USBx_INEP(EPNum)->DIEPCTL &  (1UL << 31))  /* if endpoint enabled                */
      USBx_INEP(EPNum)->DIEPCTL   |= (1 << 30);    /* disable endpoint                   */

    USBD_FlushInEpFifo (EPNum | 0x80);  /* Flush endpoint fifo                */

    if (((USBx_INEP(EPNum)->DIEPCTL >> 18) & 3) > 1)/* if Interrupt or bulk EP           */
      USBx_INEP(EPNum)->DIEPCTL |= (1 << 28);      /* Set DATA0 PID                      */

    USBx_INEP(EPNum)->DIEPCTL &= ~(1 << 21);       /* clear Stall                        */
  }
}


/*
 *  Clear USB Device Endpoint Buffer
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_ClearEPBuf (U32 EPNum) {
  if (EPNum & 0x80) {
    USBD_FlushInEpFifo (EPNum | 0x80);
  } else {
    OTG->GRSTCTL |= (1 << 4);
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
  }
}


/*
 *  Read USB Device Endpoint Data
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *    Return Value:    Number of bytes read
 */

uint32_t USBD_ReadEP (U32 EPNum, U8 *pData, uint32_t bufsz) {
  U32 val, sz, isoEpFlag;

  if ((DOEPCTL(EPNum) & (1 << 15)) == 0) return (0); /* if Ep not active      */
  isoEpFlag = (EP_OUT_TYPE(EPNum) == USB_ENDPOINT_TYPE_ISOCHRONOUS);

  /* Isochronous Ep: read data from intermediate buffer                       */
  if (isoEpFlag) {                      /* if Isochronouos endpoint           */
    if (IsoOutTokenRead == 0) {
      sz = IsoOutPacketDataCnt[EPNum];  /* get data size                      */
      for (val = 0; val < sz; val++) {  /* copy data from intermediate buffer */
        pData[val] = ((uint8_t *)(IsoOutPacketDataPtr[EPNum]))[val];
      }
      IsoOutPacketDataCnt[EPNum] = 0;   /* reset data count                   */
      return (sz);
    }
  }

  sz = ( OTG->GRXSTSP >> 4) & 0x7FF;    /* get available data size            */


	/* elee: copy from the f103 code, tbd, still required?
	  Commit 5ffac262f,  and 3d1a68b768c57cb403bab9d5598f3e9f1d3506a2 (for the core)
	  If smaller data is read here, is the rest of the data pulled in later?
	*/
	if (sz > bufsz) {
		sz = bufsz;
  }


  /* if isochronous endpoint                                                  */
  if (isoEpFlag) {
    val = OutPacketCnt[EPNum] - ((DOEPTSIZ(EPNum) >>  19) & 0x3FF);
    IsoOutPacketDataCnt[EPNum] = 0;     /* data unvalid                       */

    /* check is data is valid                                                 */
    switch ((DOEPTSIZ(EPNum) >> 29) & 3) {
      /* data0                                                                */
      case 0:
        if (val == 1) IsoOutPacketDataCnt[EPNum] = sz; break;
       /* data1                                                               */
      case 2:
        if (val == 2) IsoOutPacketDataCnt[EPNum] = sz; break;
      /* data2                                                                */
      case 1:
        if (val == 3) IsoOutPacketDataCnt[EPNum] = sz; break;
    }
  }

  /* copy data from fifo
     if Isochronous Ep: data is copied to intermediate buffer                 */
  for (val = 0; val < (uint32_t)((sz+3)/4); val++) {
    //*((__packed uint32_t *)pData) = *((__packed volatile uint32_t*)0x40041000UL); //RX_FIFO;  //elee, try hard-coding it here.
    __UNALIGNED_UINT32_WRITE(pData, USBx_DFIFO(0U));  //ToDo(elee): use stmewh743xx header? instead of USBx_DFIFI() from stm32h7xx_ll_usb.h?  Using a mixture now, no good reason.

		pData += 4;
  }
  /* wait RxFIFO non-empty (OUT transfer completed or Setup trans. completed) */
  while ((OTG->GINTSTS & (1 << 4)) == 0);
  OTG->GRXSTSP;                         /* pop register                       */
  OTG->GINTMSK |= (1 << 4);             /* unmask RxFIFO non-empty interrupt  */

  return (sz);
}


/*
 *  Write USB Device Endpoint Data
 *  If write was requested synchronously from IRQ then data is written to FIFO directly
 *  else data is written to the intermediate buffer and synchronously transferred to FIFO
 *  on next NAK event.
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *                     cnt:   Number of bytes to write
 *    Return Value:    Number of bytes written
 */

uint32_t USBD_WriteEP (U32 EPNum, U8 *pData, U32 cnt) {
  U32 *ptr, val, isoEpFlag;

  EPNum &= ~(0x80);

  if ((USBx_INEP(EPNum)->DIEPCTL & (1 << 15)) == 0) return (0); /* if Ep not active      */
  isoEpFlag = (EP_IN_TYPE(EPNum) == USB_ENDPOINT_TYPE_ISOCHRONOUS);

  /* Asynchronous write to intermediate buffer                                */
  if (!SyncWriteEP && InPacketDataPtr[EPNum]) {
    if ((!(InPacketDataReady & (1 << EPNum))) || isoEpFlag) {
      InPacketDataCnt[EPNum] = cnt;     /* save Data size                     */
      ptr = InPacketDataPtr[EPNum];
      val   = (cnt+3)/4;
      if (val) {
        while (val--) {                   /* save data to intermediate buffer   */
          *ptr++ = *((U32 *)pData); // *((__packed U32 *)pData); mjones -- is packed needed?
          pData +=4;
        }
      }
      if (!isoEpFlag) {
        InPacketDataReady |=  1 << EPNum;
        USBx_INEP(EPNum)->DIEPCTL    |= (1 << 27); /* Set NAK to enable interrupt on NAK */
        USBx_DEVICE->DIEPMSK   |= (1 <<  6);    /* INEPNEM = 1, IN EP NAK efective msk*/
      }
    } else {                            /* If packet already loaded to buffer */
      return 0;
    }
  } else {
    if (cnt) while ((DTXFSTS(EPNum) * 4) < cnt); /* get space in Ep TxFIFO    */

    /* set transfer size and packet count                                     */
    DIEPTSIZ(EPNum) = cnt | (InPacketCnt[EPNum] << 19) | (InPacketCnt[EPNum] << 29);

    if (isoEpFlag) {                    /* if Isochronous Ep: set packet frame*/
      if (USBD_GetFrame() & 1) USBx_INEP(EPNum)->DIEPCTL |= (1 << 28); /* even frame     */
      else USBx_INEP(EPNum)->DIEPCTL |= (1 << 29);                     /* odd frame      */
    }

    USBx_INEP(EPNum)->DIEPCTL |= (1UL << 31) | (1 << 26);  /* enable ep and clear NAK    */
    if (cnt) {
      ptr = (uint32_t *)pData;
      val   = (cnt+3)/4;
      while (val--) {                   /* copy data to endpoint TxFIFO       */
        //TX_FIFO(EPNum) = *(__packed uint32_t *)pData;
				USBx_DFIFO((uint32_t)EPNum) = __UNALIGNED_UINT32_READ(pData);  //ToDo(elee): use stmewh743xx header? instead of USBx_DFIFI() from stm32h7xx_ll_usb.h?  Using a mixture now, no good reason.
        pData +=4;
      }
    }
    if (isoEpFlag == 0) InPacketDataReady &= ~(1 << EPNum);
  }
  return (cnt);
}


/*
 *  Get USB Device Last Frame Number
 *    Parameters:      None
 *    Return Value:    Frame Number
 */

uint32_t USBD_GetFrame (void) {
  return ((USBx_DEVICE->DSTS >> 8) & 0x3FFF);
}






/* elee:
In 5ffac262fd481323edf0c68a7a906c09de76179e I can see the USB interrupt handler was modified,
so that it handles some things IN the irq handler, then sets a signal in the main loop,
which calls USBD_Handler() to do the rest.

I'm not sure what parts of the IRQ_handler are "non time sensitive", so just leave it as is
and make an empty USBD_Handler()

    USBD_SignalHandler();
}
void USBD_Handler(void)
*/

//void USBD_Handler() {
//}



/*
 *  USB Device Interrupt Service Routine
 */
void OTG_HS_IRQHandler(void) {

    NVIC_DisableIRQ(OTG_HS_IRQn);
    USBD_SignalHandler();
}

void USBD_Handler(void)
{
  uint32_t istr, val, num, i, msk;
  static uint32_t IsoInIncomplete = 0;

  istr = OTG->GINTSTS & OTG->GINTMSK;

/* reset interrupt                                                            */
  if (istr & (1 << 12)) {
    USBD_Reset();
    usbd_reset_core();
#ifdef __RTX
    if (USBD_RTX_DevTask) {
      isr_evt_set(USBD_EVT_RESET, USBD_RTX_DevTask);
    }
#else
    if (USBD_P_Reset_Event) {
      USBD_P_Reset_Event();
    }
#endif
    OTG->GINTSTS = (1 << 12);
  }

/* suspend interrupt                                                          */
  if (istr & (1 << 11)) {
    USBD_Suspend();
#ifdef __RTX
    if (USBD_RTX_DevTask) {
      isr_evt_set(USBD_EVT_SUSPEND, USBD_RTX_DevTask);
    }
#else
    if (USBD_P_Suspend_Event) {
      USBD_P_Suspend_Event();
    }
#endif
    OTG->GINTSTS = (1 << 11);
  }

/* resume interrupt                                                           */
  if (istr & (1UL << 31)) {
    USBD_Resume();
#ifdef __RTX
    if (USBD_RTX_DevTask) {
      isr_evt_set(USBD_EVT_RESUME, USBD_RTX_DevTask);
    }
#else
    if (USBD_P_Resume_Event) {
      USBD_P_Resume_Event();
    }
#endif
    OTG->GINTSTS = (1UL << 31);
  }

/* speed enumeration completed                                                */
  if (istr & (1 << 13)) {
    if (!((USBx_DEVICE->DSTS >> 1) & 3)) {
      USBD_HighSpeed = 1;
    }
    USBx_INEP(0)->DIEPCTL   &= ~0x3FF;
    USBx_INEP(0)->DIEPCTL   |= OutMaxPacketSize[0] ;    /* EP0 max packet             */
    USBx_DEVICE->DCTL       |= (1UL << 8);      /* clear global IN NAK                */
    USBx_DEVICE->DCTL       |= (1UL << 10);     /* clear global OUT NAK               */
    OTG->GINTSTS    |= (1UL << 13);
  }

/* Start Of Frame                                                             */
  if (istr & (1 << 3)) {
#ifdef __RTX
    if (USBD_RTX_DevTask) {
      isr_evt_set(USBD_EVT_SOF, USBD_RTX_DevTask);
    }
#else
    if (USBD_P_SOF_Event) {
      USBD_P_SOF_Event();
    }
#endif
     OTG->GINTSTS = (1 << 3);
  }

/* RxFIFO non-empty                                                           */
  if (istr & (1 << 4)) {
    val = OTG->GRXSTSR;
    num = val & 0x0F;

    switch ((val >> 17) & 0x0F) {
/* setup packet                                                               */
      case 6:
#ifdef __RTX
      OTG->GINTMSK &= ~(1 << 4);
      if (USBD_RTX_EPTask[num]) {
        isr_evt_set(USBD_EVT_SETUP, USBD_RTX_EPTask[num]);
      }
#else
      if (USBD_P_EP[num]) {
        USBD_P_EP[num](USBD_EVT_SETUP);
      }
#endif
      break;

/* OUT packet                                                                 */
      case 2:
      OTG->GINTMSK &= ~(1 << 4);
      if (EP_OUT_TYPE(num) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        IsoOutTokenRead = 1;
				//elee: add max length.   ;    /* EP0 max packet             */
        USBD_ReadEP (num, (uint8_t *)IsoOutPacketDataPtr[num], OutMaxPacketSize[0]);
        IsoOutTokenRead = 0;
      } else {
#ifdef __RTX
        if (USBD_RTX_EPTask[num]) {
          isr_evt_set(USBD_EVT_OUT, USBD_RTX_EPTask[num]);
        }
#else
        if (USBD_P_EP[num]) {
          USBD_P_EP[num](USBD_EVT_OUT);
        }
#endif
      }
      break;

      default:
        OTG->GRXSTSP;
    }
  }

/* OUT Packet                                                                 */
  if (istr & (1 << 19)) {
    msk = (((USBx_DEVICE->DAINT & USBx_DEVICE->DAINTMSK) >> 16) & 0xFFFF);
    i   = 0;
    while (msk) {
      num = 0;
      for (; i < (USBD_EP_NUM+1); i++) {
        if ((msk >> i) & 1) {
          num = i;
          msk &= ~(1 << i);
          break;
        }
      }

      /* Endpoint disabled                                                    */
      if (DOEPINT(num) & (1 << 1)) {
        if (EP_OUT_TYPE(num) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
          DOEPTSIZ(num) = (OutPacketCnt[num] << 19) |/* packet count          */
                          (OutMaxPacketSize[num]);   /* transfer size         */

          if ((USBD_GetFrame() & 1)) DOEPCTL(num) |= (1 << 28); /* even frame */
          else                       DOEPCTL(num) |= (1 << 29); /* odd frame  */

          DOEPCTL(num)    |= (1UL <<31) | (1 << 26);
        }
        DOEPINT(num) |= (1 << 1);
      }

      /* Transfer complete interrupt                                          */
      if ((DOEPINT(num) & 1) | (DOEPINT(num) & (1 << 3))) {
        if (EP_OUT_TYPE(num) != USB_ENDPOINT_TYPE_ISOCHRONOUS) {

          DOEPTSIZ(num)    = (OutPacketCnt[num] << 19) | /* packet count      */
                             (OutMaxPacketSize[num]);    /* transfer size     */
          if (num == 0) {
            DOEPTSIZ(0)   |= (1 << 29);
          }
          DOEPCTL(num)    |= (1UL <<31) | (1 << 26);     /* clr NAK, en EP    */
        }
        DOEPINT(num) |= 1;
      }
    }
  }

/* IN Packet                                                                  */
  if (istr & (1 << 18)) {
    msk = (USBx_DEVICE->DAINT & USBx_DEVICE->DAINTMSK & 0xFFFF);
    i   = 0;
    while (msk) {
      num = 0;
      for (; i < (USBD_EP_NUM+1); i++) {
        if ((msk >> i) & 1) {
          num = i;
          msk &= ~(1 << i);
          break;
        }
      }

      /* Endpoint disabled                                                    */
      if (DIEPINT(num) & (1 << 1)) {
        DIEPINT(num) = (1 << 1);

        if (EP_IN_TYPE(num) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
          if ((IsoInIncomplete & (1 << num)) != 0) {

            USBD_FlushInEpFifo(num | 0x80);
            SyncWriteEP = 1;
            USBD_WriteEP (num, (uint8_t *)InPacketDataPtr[num], InPacketDataCnt[num]);
            SyncWriteEP = 0;

            IsoInIncomplete &= ~(1 << num);
          }
        }
      }

      /* IN endpoint NAK effective                                            */
      if (DIEPINT(num) & (1 << 6)) {
        if (EP_IN_TYPE(num) != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
          if (InPacketDataPtr[num] && (InPacketDataReady & (1 << num))) {
            SyncWriteEP = 1;
            USBD_WriteEP (num, (uint8_t *)InPacketDataPtr[num], InPacketDataCnt[num]);
            SyncWriteEP = 0;
            if (!InPacketDataReady)     /* No more pending IN transfers       */
              USBx_DEVICE->DIEPMSK &= ~(1 << 6);/* Disable IN NAK interrupts          */
            continue;
          } else
              USBx_INEP(num)->DIEPCTL |= (1 << 26);
            DIEPINT(num)    = (1 <<  6);
        }
      }

      /* Transmit completed                                                   */
      if (DIEPINT(num) & 1) {
        DIEPINT(num) = 1;
        SyncWriteEP = 1;
        if (EP_IN_TYPE(num) == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
          USBD_WriteEP (num, (uint8_t *)InPacketDataPtr[num], InPacketDataCnt[num]);
        } else {
#ifdef __RTX
          if (USBD_RTX_EPTask[num]) {
            isr_evt_set(USBD_EVT_IN,  USBD_RTX_EPTask[num]);
          }
#else
          if (USBD_P_EP[num]) {
            USBD_P_EP[num](USBD_EVT_IN);
          }
#endif
        }
        SyncWriteEP = 0;
      }
    }
  }

/* End of periodic frame                                                      */
  if (istr & (1 << 15)) {
    for (num = 1; num <= USBD_EP_NUM; num++) {

      if (EP_OUT_TYPE(num) != USB_ENDPOINT_TYPE_ISOCHRONOUS) continue;
      if (((DOEPCTL(num) >> 15) & 1) == 0)                   continue;

      /* incomplete isochronous out transfer                                  */
      if (OTG->GINTSTS & (1 << 21)) {
        if ((USBD_GetFrame() & 1) == ((DOEPCTL(num) >> 16) & 1)) {
          if (DOEPCTL(num) & (1UL << 31)) {
            InPacketDataCnt[num] = 0;   /* discard data                       */
            DOEPCTL(num) |= (1UL << 30);/* disable endpoint                   */
          }
        }

      /* prepare for next isohronous transfer                                 */
      } else {
        DOEPTSIZ(num) = (OutPacketCnt[num] << 19) |/* packet count            */
                        (OutMaxPacketSize[num]);   /* transfer size           */

        if ((USBD_GetFrame() & 1)) DOEPCTL(num) |= (1 << 28); /* even frame   */
        else                       DOEPCTL(num) |= (1 << 29); /* odd frame    */

        DOEPCTL(num)    |= (1UL <<31) | (1 << 26);
      }
    }
    OTG->GINTSTS = (1 << 15) | (1 << 21);
  }

/* incomplete isochronous IN transfer                                         */
  if (istr & (1 << 20)) {
    OTG->GINTSTS = (1 << 20);
    for (num = 1; num < (USBD_EP_NUM + 1); num++) {

      if (EP_IN_TYPE(num) != USB_ENDPOINT_TYPE_ISOCHRONOUS) continue;
      if (((USBx_INEP(num)->DIEPCTL >> 15) & 1) == 0)                  continue;

      if (USBx_INEP(num)->DIEPCTL & (1UL << 31)) { /* if EP en & packet frame is incorect*/
        if ((USBD_GetFrame() & 1) == ((USBx_INEP(num)->DIEPCTL >> 16) & 1)) {

          IsoInIncomplete |= (1 << num);
          USBx_INEP(num)->DIEPCTL    |= (1UL << 30) | (1 << 27);
        }
      }
    }
  }

	NVIC_EnableIRQ(OTG_HS_IRQn);
}
