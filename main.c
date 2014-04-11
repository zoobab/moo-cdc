/*
 * main.c - Improved AVR USB driver for CDC interface on Low-Speed USB
 * Copyright (C) 2007 by Recursion Co., Ltd.
 * Copyright (C) 2008-2010 Stefan Szczygielski
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id: main.c 371 2010-01-30 13:12:03Z moomean $
 */

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include "usbdrv.h"
#include "oddebug.h"

#ifndef USBATTR_BUSPOWER
// from earlier AVR-USB version
#define USBATTR_BUSPOWER        0x80  // USB 1.1 does not define this value any more
#endif

#include "moo-cdc.h"

volatile uchar rx_buf[256] __attribute__ ((section (".rxbuf")));
register volatile uchar rx_buf_put_ptr asm("r2"); // = 0;
uchar rx_buf_get_ptr = 0;

enum {
    SEND_ENCAPSULATED_COMMAND = 0,
    GET_ENCAPSULATED_RESPONSE,
    SET_COMM_FEATURE,
    GET_COMM_FEATURE,
    CLEAR_COMM_FEATURE,
    SET_LINE_CODING = 0x20,
    GET_LINE_CODING,
    SET_CONTROL_LINE_STATE,
    SEND_BREAK
};


static PROGMEM char deviceDescrCDC[] = {    /* USB device descriptor */
    18,         /* sizeof(usbDescriptorDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,        /* descriptor type */
    0x01, 0x01,             /* USB version supported */
    0x02,                   /* device class: CDC */
    0,                      /* subclass */
    0,                      /* protocol */
    8,                      /* max packet size */
    USB_CFG_VENDOR_ID,      /* 2 bytes */
    0xe1, 0x05,             /* 2 bytes: shared PID for CDC-ACM devices */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
    1,                      /* manufacturer string index */
    2,                      /* product string index */
    0,                      /* serial number string index */
    1,                      /* number of configurations */
};

static PROGMEM char configDescrCDC[] = {   /* USB configuration descriptor */
    9,          /* sizeof(usbDescrConfig): length of descriptor in bytes */
    USBDESCR_CONFIG,    /* descriptor type */
    67,
    0,          /* total length of data returned (including inlined descriptors) */
    2,          /* number of interfaces in this configuration */
    1,          /* index of this configuration */
    0,          /* configuration name string index */
#if USB_CFG_IS_SELF_POWERED
    USBATTR_SELFPOWER,  /* attributes */
#else
    USBATTR_BUSPOWER,   /* attributes */
#endif
    USB_CFG_MAX_BUS_POWER/2,            /* max USB current in 2mA units */

    /* interface descriptor follows inline: */
    9,          /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE, /* descriptor type */
    0,          /* index of this interface */
    0,          /* alternate setting for this interface */
    USB_CFG_HAVE_INTRIN_ENDPOINT,   /* endpoints excl 0: number of endpoint descriptors to follow */
    USB_CFG_INTERFACE_CLASS,
    USB_CFG_INTERFACE_SUBCLASS,
    USB_CFG_INTERFACE_PROTOCOL,
    0,          /* string index for interface */

    /* CDC Class-Specific descriptor */
    5,           /* sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes */
    0x24,        /* descriptor type */
    0,           /* header functional descriptor */
    0x10, 0x01,

    4,           /* sizeof(usbDescrCDC_AcmFn): length of descriptor in bytes */
    0x24,        /* descriptor type */
    2,           /* abstract control management functional descriptor */
    0x02,        /* SET_LINE_CODING,    GET_LINE_CODING, SET_CONTROL_LINE_STATE    */

    5,           /* sizeof(usbDescrCDC_UnionFn): length of descriptor in bytes */
    0x24,        /* descriptor type */
    6,           /* union functional descriptor */
    0,           /* CDC_COMM_INTF_ID */
    1,           /* CDC_DATA_INTF_ID */

    5,           /* sizeof(usbDescrCDC_CallMgtFn): length of descriptor in bytes */
    0x24,        /* descriptor type */
    1,           /* call management functional descriptor */
    3,           /* allow management on data interface, handles call management by itself */
    1,           /* CDC_DATA_INTF_ID */

    /* Endpoint Descriptor */
    7,           /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x83,        /* IN endpoint number 3 */
    0x03,        /* attrib: Interrupt endpoint */
    1, 0,        /* maximum packet size */
    2,           /* in ms */

    /* Interface Descriptor  */
    9,           /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE,           /* descriptor type */
    1,           /* index of this interface */
    0,           /* alternate setting for this interface */
    2,           /* endpoints excl 0: number of endpoint descriptors to follow */
    0x0A,        /* Data Interface Class Codes */
    0,
    0,           /* Data Interface Class Protocol Codes */
    0,           /* string index for interface */

    /* Endpoint Descriptor */
    7,           /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x01,        /* OUT endpoint number 1 */
    0x02,        /* attrib: Bulk endpoint */
    8, 0,        /* maximum packet size	*/
    0,           /* in ms */

    /* Endpoint Descriptor */
    7,           /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x81,        /* IN endpoint number 1 */
    0x02,        /* attrib: Bulk endpoint */
    8, 0,        /* maximum packet size */
    0,           /* in ms */
};


uchar usbFunctionDescriptor(usbRequest_t *rq)
{

    if(rq->wValue.bytes[1] == USBDESCR_DEVICE){
        usbMsgPtr = (uchar *)deviceDescrCDC;
        return sizeof(deviceDescrCDC);
    }else{  /* must be config descriptor */
        usbMsgPtr = (uchar *)configDescrCDC;
        return sizeof(configDescrCDC);
    }
}

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

uchar usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */

//        if( rq->bRequest==GET_LINE_CODING || rq->bRequest==SET_LINE_CODING ){
        if( rq->bRequest==SET_LINE_CODING ){
            return 0xff;
            /*    GET_LINE_CODING -> usbFunctionRead()    */
            /*    SET_LINE_CODING -> usbFunctionWrite()    */
        }
    }

    return 0;
}

#ifdef DEBUG
uchar baud_test0 = 0x00;
uchar baud_test1 = 0x00;
uchar baud_test2 = 0x00;
uchar baud_test3 = 0x00;
#endif
/*---------------------------------------------------------------------------*/
/* usbFunctionRead                                                           */
/*---------------------------------------------------------------------------*/
/*
uchar usbFunctionRead( uchar *data, uchar len )
{

	data[0] = baud_l;
	data[1] = baud_h;
	data[2] = 0x00;
	data[3] = 0x00;
	data[4] = 0x00;
	data[5] = 0x00;
	data[6] = 0x08;
	return 7;
}
*/
/*---------------------------------------------------------------------------*/
/* usbFunctionWrite                                                          */
/*---------------------------------------------------------------------------*/

uchar usbFunctionWrite( uchar *data, uchar len )
{
	uchar stopbits = data[4];
	uchar parity = data[5];
	uchar databits = data[6];
#ifdef DEBUG
	baud_test0++;
#endif

/*	UCSRB &= ~(1 << RXCIE);
	rx_buf[rx_buf_cnt] = databits;
	if(rx_buf_cnt < 7)
		rx_buf_cnt++;
	UCSRB |= (1 << RXCIE);
*/
//	baud_test0 = len;

/*	baud_l = data[0];
	baud_h = data[1];
	baud_hl = data[2];
	baud_hh = data[3];*/
//	baud_times++;

	// some opcode-efficient method to be able to switch bauds :)
	if(data[1] == (115200 & 0xFF00) >> 8)
	{
		UBRRL = F_CPU/8/115200-1;
	}
	if(data[1] == (57600 & 0xFF00) >> 8)
	{
		UBRRL = F_CPU/8/57600-1;
	}
	if(data[1] == (38400 & 0xFF00) >> 8)
	{
		UBRRL = F_CPU/8/38400-1;
	}	
	if(data[1] == (19200 & 0xFF00) >> 8)
	{
		UBRRL = F_CPU/8/19200-1;
	}	
	if(data[1] == ((9600 & 0xFF00) >> 8))
	{
		UBRRL = F_CPU/8/9600-1;
	}

	// when using windows we received something else not concerning baudrate change
	// (this function is being called two times when connecting)
	// so we can't make a default entry - we will fall into it every time
	// no such problem when using Linux - here we get only one correct setup
	//
	// UPDATE: windows is sending data in two parts! first baudrate with
	// databits = 0x56 and then the other parameters with baudrate = 0!

//	UBRRL = F_CPU/8/9600-1; // anything else? make 9600

	// this calculation below is nice, but it costs too much precious flash
//	uint16_t *baud = data;
//	UBRRL = F_CPU/8L/(*baud) - 1;

	if(databits <= 8) // looks like sane parameters setup
	{
		if(parity > 2)
			parity = 0;
		if(stopbits == 1)
			stopbits = 0;
#ifdef URSEL
		UCSRC  = (1 << URSEL) | ((parity==1 ? 3 : parity) << UPM0) | (( stopbits >> 1) << USBS) | ( (databits-5) << UCSZ0);
#else
		UCSRC  = ((parity==1 ? 3 : parity) << UPM0) | (( stopbits >> 1) << USBS) | ( (databits-5) << UCSZ0);

#endif
	}
//	baud_test0 = len;
//	baud_test0 = stopbits;
//	baud_test1 = parity;
//	baud_test1 = databits;
//	baud_test3++;
	return 1;
}

volatile uchar tx_buf[8];
volatile uchar tx_buf_put_ptr;
volatile uchar tx_buf_get_ptr;

SIGNAL(USART_UDRE_vect)
{
	UCSRB &= ~(1 << UDRIE); // disable UDRE int, we want to re-enable ints
	sei(); // reenable interrupts as fast as possible

	UDR = tx_buf[tx_buf_get_ptr++];
	if(tx_buf_get_ptr == tx_buf_put_ptr) // all bytes sent
	{
		usbEnableAllRequests(); // can get new ones -> this actually does just usbRxLen = 0
	}
	else
	{
		UCSRB |= (1 << UDRIE); // enable UDRE int, we will be sending more data
	}
}
void usbFunctionWriteOut( uchar *data, uchar len )
{
	uchar i;
	for(i = 0; i<len; i++)
		tx_buf[i] = data[i];
	tx_buf_get_ptr = 0;
	tx_buf_put_ptr = len;
        usbDisableAllRequests();
	UCSRB |= (1 << UDRIE); // take it!
#ifdef DEBUG
	PORTB ^= (1 << PB1);
#endif
}

static void hardwareInit(void)
{
uchar    i, j;

    /* activate pull-ups except on USB lines */
    USB_CFG_IOPORT   = (uchar)~((1<<USB_CFG_DMINUS_BIT)|(1<<USB_CFG_DPLUS_BIT));
    /* all pins input except USB (-> USB reset) */
#ifdef USB_CFG_PULLUP_IOPORT    /* use usbDeviceConnect()/usbDeviceDisconnect() if available */
    USBDDR    = 0;    /* we do RESET by deactivating pullup */
    usbDeviceDisconnect();
#else
    USBDDR    = (1<<USB_CFG_DMINUS_BIT)|(1<<USB_CFG_DPLUS_BIT);
#endif

    j = 0;
    while(--j){          /* USB Reset by device only required on Watchdog Reset */
        i = 0;
        while(--i)
        ;
    }

#ifdef USB_CFG_PULLUP_IOPORT
    usbDeviceConnect();
#else
    USBDDR    = 0;      /*  remove USB reset condition */
#endif

    /*    USART configuration  -  38400bps 8N1 at 16Mhz */
//    UBRRL  = 25;
//    UBRRH  = 0x02;
//    UBRRL  = 0x70;
//    UCSRA  = (1<<U2X);

    UBRRL = F_CPU/8/9600;
//    UBRRL = F_CPU/8/57600;
    UCSRA  = (1<<U2X);

    UCSRB  = (1<<RXEN) | (1<<TXEN) | (1 << RXCIE);

#ifdef DEBUG
    DDRB |= (1 << PB1);
    PORTB |= (1 << PB1);
#endif

}

int main(void)
{

    rx_buf_put_ptr = 0; // set variable tied to register

    uint8_t send_empty_frame = 0; // send empty frame to indicate transfer end?

//    wdt_enable(WDTO_1S);
    odDebugInit();
    hardwareInit();
    usbInit();

#if HAVE_LED
    LED_DDR |= _BV(LED_P);
#endif

    sei();
    for(;;){    /* main event loop */
        wdt_reset();
        usbPoll();
        /*  device -> host  */
	uint8_t put_ptr = rx_buf_put_ptr; // at some point, can advance ,,in the meantime''
	// UNFORTUNETELLY when rx_buf_put_ptr is tied to a register gcc happily uses it
	// all the time without copying so put_ptr's value MAY CHANGE because of interrupt
	// arrival during if's processing... :/
	// FORTUNETELLY :) we don't care - it's safe for us to send more data if available
	// even if it arrived later... It's somewhat ugly, but it works as expected (tm) (see assembly)
	// Even if buffer overflows (i.e. put_ptr == rx_buf_get_ptr after initial comparision)
	// sending 0 bytes of data to the USB library won't hurt... (:
	if( usbInterruptIsReady() )
	{
		if(put_ptr != rx_buf_get_ptr) // something in current bank
		{
			uint8_t cnt;
			if(put_ptr > rx_buf_get_ptr) // put pointer ahead
			{
				cnt = put_ptr - rx_buf_get_ptr;
				if(cnt > 8)
					cnt = 8;
			}
			else // put pointer behind
			{
				if((uint8_t)(0 - rx_buf_get_ptr) > 8) // damm gcc compared 16-bit here!
					cnt = 8;
				else
					cnt = 0 - rx_buf_get_ptr; // less than 8 bytes
			}
			usbSetInterrupt(rx_buf + rx_buf_get_ptr, cnt);
			rx_buf_get_ptr += cnt;
			send_empty_frame = 1; // if no more data on next interrupt ready -> send empty frame
#if HAVE_LED
			LED_PORT |= _BV(LED_P); // LED on
#endif
		}
		else // no data to send
		{
			if(send_empty_frame)
			{
				usbSetInterrupt(NULL, 0); // send empty frame to indicate data end
				send_empty_frame = 0;
			}
#if HAVE_LED
			LED_PORT &= ~_BV(LED_P); // LED off
#endif
		}

        }
    }
    return 0;
}

