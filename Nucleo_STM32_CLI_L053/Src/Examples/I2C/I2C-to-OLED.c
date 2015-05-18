/*
    FreeRTOS V7.3.0 - Copyright (C) 2012 Real Time Engineers Ltd.


    ***************************************************************************
     *                                                                       *
     *    FreeRTOS tutorial books are available in pdf and paperback.        *
     *    Complete, revised, and edited pdf reference manuals are also       *
     *    available.                                                         *
     *                                                                       *
     *    Purchasing FreeRTOS documentation will not only help you, by       *
     *    ensuring you get running as quickly as possible and with an        *
     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
     *    the FreeRTOS project to continue with its mission of providing     *
     *    professional grade, cross platform, de facto standard solutions    *
     *    for microcontrollers - completely free of charge!                  *
     *                                                                       *
     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
     *                                                                       *
     *    Thank you for using FreeRTOS, and thank you for your support!      *
     *                                                                       *
    ***************************************************************************


    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    >>>NOTE<<< The modification to the GPL is included to allow you to
    distribute a combined work that includes FreeRTOS without being obliged to
    provide the source code for proprietary components outside of the FreeRTOS
    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/

/* FreeRTOS includes. */
#include "FreeRTOS.h"

/* FreeRTOS+IO includes. */
#include "FreeRTOS_IO.h"

/* Library includes. */
#include "lpc17xx_gpio.h"

/* Example includes. */
#include "oled.h"
#include "I2C-to-OLED.h"

/* I2C address of the OLED display. */
#define i2cOLED_SLAVE_ADDRESS			( ( void * ) 0x3c )

/* Maximum I2C frequency at which the OLED can operate. */
#define i2cOLED_CLOCK 						( ( void * ) 800000UL )

/* Just used to write over text in the shadow buffer. */
#define i2cBLANK_LINE					( ( uint8_t * ) "                " )

/* Two lines are scrolled down the display.  Determine when the bottom is
reached. */
#define i2cMAX_DOUBLE_ROW				( ( uint8_t ) ( oledDISPLAY_HEIGHT - 1 ) )

/*
 * Set up the bus parameters as necessary for writing to the OLED.
 */
static void prvConfigureI2CBusForOLED( Peripheral_Descriptor_t xI2CPort );

/*-----------------------------------------------------------*/

void vI2C_OLEDInitialise( Peripheral_Descriptor_t xI2CPort )
{
	/* Set the I2C parameters to be correct for communicating with the OLED. */
	prvConfigureI2CBusForOLED( xI2CPort );

	/* Pass the handle to the I2C port into vOLEDInit() so the opened port can
	be used to write the init commands to the OLED.  The handle is then stored
	in oled.c for future use. */
	vOLEDInit( xI2CPort );
	vOLEDClearScreen( OLED_COLOR_BLACK );
}
/*-----------------------------------------------------------*/

void vI2C_OLEDTest( Peripheral_Descriptor_t xI2CPort, uint8_t *pcSecondRowText )
{
uint8_t ucRow;

	/* The I2C port is opened and configured in the I2C-coordinator.c file.
	The opened handle is passed in to this file - which just uses the I2C
	FreeRTOS+IO driver with whatever configuration it happens to have at that
	time.  Sometimes it	will be operating in polled mode, and other in
	interrupt driven zero copy Tx with interrupt driven circular buffer Rx.

	Set the I2C parameters to be correct for communicating with the OLED. */
	prvConfigureI2CBusForOLED( xI2CPort );

	/* Scroll a message down the OLED. */
	for( ucRow = 0U; ucRow <= i2cMAX_DOUBLE_ROW; ucRow++ )
	{
		/* Write two lines of text to the shadow frame buffer.  The text on the
		second line changes depending on the transmit method being used. */
		vOLEDPutString( ucRow, ( uint8_t * ) "   FreeRTOS+IO  ", OLED_COLOR_WHITE, OLED_COLOR_BLACK );
		vOLEDPutString( ucRow + oledCHARACTER_HEIGHT, pcSecondRowText, OLED_COLOR_WHITE, OLED_COLOR_BLACK );

		/* Write the shadow frame buffer to the OLED. */
		vOLEDRefreshDisplay();

		/* Clear down the shadow frame buffer. */
	    vOLEDPutString( ucRow, ( uint8_t * ) "                ", OLED_COLOR_WHITE, OLED_COLOR_BLACK );
	    vOLEDPutString( ucRow + oledCHARACTER_HEIGHT, ( uint8_t * ) "                ", OLED_COLOR_WHITE, OLED_COLOR_BLACK );
	}
}
/*-----------------------------------------------------------*/

static void prvConfigureI2CBusForOLED( Peripheral_Descriptor_t xI2CPort )
{
	/* Set the slave address to the address of the OLED. */
	FreeRTOS_ioctl( xI2CPort, ioctlSET_I2C_SLAVE_ADDRESS, i2cOLED_SLAVE_ADDRESS );

	/* Set the clock frequency for the OLED. */
	FreeRTOS_ioctl( xI2CPort, ioctlSET_SPEED, i2cOLED_CLOCK );
}
/*-----------------------------------------------------------*/

