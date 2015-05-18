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

/* Standard includes. */
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+IO includes. */
#include "FreeRTOS_IO.h"

/* Library includes. */
#include "lpc17xx_gpio.h"

/* Example includes. */
#include "I2C-to-and-from-EEPROM.h"

/* EEPROM parameters. */
#define i2cTOTAL_EEPROM_SIZE 	( 1024UL )
#define i2cPAGE_SIZE			( 16UL )

/* I2C address of the EEPROM. */
#define i2cEEPROM_SLAVE_ADDRESS	( 0x50UL )

/* Maximum I2C frequency at which the EEPROM can operate at 5V. */
#define i2cEEPROM_CLOCK 		( ( void * ) 400000UL )

/* A delay is required to allow the EEPROM to program itself. */
#define i2cWRITE_CYCLE_DELAY	( 5UL )

/* The number of test loops to perform each tie vI2C_EEPROMTest() is called.
Each tests a different page in memory. */
#define i2cEEPROM_TESTS			( 6UL )

/* Place holder for calls to ioctl that don't use the value parameter. */
#define i2cPARAMETER_NOT_USED	( ( void * ) 0 )

/*
 * The least significant bits of the slave address form part of the EEPROM byte
 * address.  Set the slave address to be correct for the byte address being
 * read from or written to.
 */
static uint8_t prvSetSlaveAddress( Peripheral_Descriptor_t xI2CPort, uint32_t ulByteAddress );

/*
 * Write an entire page of data to the EEPROM, starting from the byte address
 * specified by the parameter.
 */
static void prvWritePageToEEPROM( Peripheral_Descriptor_t xI2CPort, uint32_t ulByteAddress );

/*-----------------------------------------------------------*/

/* A buffer large enough to hold a complete page of data plus a byte for the
address of the data. */
static uint8_t ucDataBuffer[ i2cPAGE_SIZE + 1 ];


/*-----------------------------------------------------------*/

void vI2C_EEPROMTest( Peripheral_Descriptor_t xI2CPort )
{
uint32_t ulStartAddress = 0UL, ulPage;
const uint32_t ulMaxDelay = 500UL / portTICK_RATE_MS;
static uint8_t ucByte, ucValue;
int32_t lReturned;

	/* The I2C port is opened and configured in the I2C-coordinator.c file.
	The opened handle is passed in to this file - which just uses the I2C
	FreeRTOS+IO driver with whatever configuration it happens to have at that
	time.  Sometimes it	will be operating in polled mode, and other in
	interrupt driven zero copy Tx with interrupt driven circular buffer Rx. */

	/* Set the clock frequency to be correct for the EEPROM. */
	FreeRTOS_ioctl( xI2CPort, ioctlSET_SPEED, i2cEEPROM_CLOCK );

	/* The write mutex is obtained in the code below, but it is possible that
	it is already held by this task - in which case the attempt to obtain it
	will fail.  Release the mutex first, just in case. */
	FreeRTOS_ioctl( xI2CPort, ioctlRELEASE_WRITE_MUTEX, i2cPARAMETER_NOT_USED );

	/* Wait until any writes already in progress have completed. */
	FreeRTOS_ioctl( xI2CPort, ioctlOBTAIN_WRITE_MUTEX, ( void * ) ulMaxDelay );

	/* Fill the EEPROM with 0x00, one page at a time. */
	memset( ucDataBuffer, 0x00, sizeof( ucDataBuffer ) );
	for( ulPage = 0UL; ulPage < ( i2cTOTAL_EEPROM_SIZE / i2cPAGE_SIZE ); ulPage++ )
	{
		prvWritePageToEEPROM( xI2CPort, ulStartAddress );

		/* Move to the next page. */
		ulStartAddress += i2cPAGE_SIZE;
	}

	/* Check all data read from the EEPROM reads as 0x00.  Start by setting the
	read address back to the start of the EEPROM. */
	ucDataBuffer[ 0 ] = 0x00;
	lReturned = FreeRTOS_write( xI2CPort, ucDataBuffer, sizeof( ucDataBuffer[ 0 ] ) );
	configASSERT( lReturned == sizeof( ucDataBuffer[ 0 ] ) );

	/* Wait until the write completes. */
	FreeRTOS_ioctl( xI2CPort, ioctlOBTAIN_WRITE_MUTEX, ( void * ) ulMaxDelay );

	for( ulPage = 0UL; ulPage < ( i2cTOTAL_EEPROM_SIZE / i2cPAGE_SIZE ); ulPage++ )
	{
		/* Ensure the data buffer does not contain 0x00 already. */
		memset( ucDataBuffer, 0xff, sizeof( ucDataBuffer ) );

		/* Read a page back from the EEPROM. */
		lReturned = FreeRTOS_read( xI2CPort, ucDataBuffer, i2cPAGE_SIZE );
		configASSERT( lReturned == i2cPAGE_SIZE );

		/* Check each byte in the page just read contains 0. */
		for( ucByte = 0U; ucByte < i2cPAGE_SIZE; ucByte++ )
		{
			configASSERT( ucDataBuffer[ ucByte ] == 0U );
		}
	}

	/* Do the same, but this time write a different value into each location
	(the value will overflow). */
	ucValue = 0x00U;
	ulStartAddress = 0UL;
	for( ulPage = 0UL; ulPage < ( i2cTOTAL_EEPROM_SIZE / i2cPAGE_SIZE ); ulPage++ )
	{
		for( ucByte = 0U; ucByte < i2cPAGE_SIZE; ucByte++ )
		{
			/* ucDataBuffer[ 0 ] holds the byte address so is skipped. */
			ucDataBuffer[ ucByte + 1 ] = ucValue;
			ucValue++;
		}

		prvWritePageToEEPROM( xI2CPort, ulStartAddress );

		/* Move to the next page. */
		ulStartAddress += i2cPAGE_SIZE;
	}

	/* Check all data read from the EEPROM reads as written.  Start by setting
	the	read address back to the start of the EEPROM. */
	ucDataBuffer[ 0 ] = 0x00;
	lReturned = FreeRTOS_write( xI2CPort, ucDataBuffer, sizeof( ucDataBuffer[ 0 ] ) );
	configASSERT( lReturned == sizeof( ucDataBuffer[ 0 ] ) );

	/* Wait until the write completes. */
	FreeRTOS_ioctl( xI2CPort, ioctlOBTAIN_WRITE_MUTEX, ( void * ) ulMaxDelay );

	ucValue = 0U;
	for( ulPage = 0UL; ulPage < ( i2cTOTAL_EEPROM_SIZE / i2cPAGE_SIZE ); ulPage++ )
	{
		/* Ensure the data buffer starts clear. */
		memset( ucDataBuffer, 0xff, sizeof( ucDataBuffer ) );

		/* Read a page back from the EEPROM. */
		lReturned = FreeRTOS_read( xI2CPort, ucDataBuffer, i2cPAGE_SIZE );
		configASSERT( lReturned == i2cPAGE_SIZE );

		/* Check each byte in the page contains the expected value. */
		for( ucByte = 0U; ucByte < i2cPAGE_SIZE; ucByte++ )
		{
			configASSERT( ucDataBuffer[ ucByte ] == ucValue );
			ucValue++;
		}
	}
}
/*-----------------------------------------------------------*/

static uint8_t prvSetSlaveAddress( Peripheral_Descriptor_t xI2CPort, uint32_t ulByteAddress )
{
uint32_t ulSlaveAddress;

	/* The bottom two bits of the slave address are used as the two most
	significant bits of the byte address within the EEPROM. */
	ulSlaveAddress = ulByteAddress;
	ulSlaveAddress >>= 8UL;
	ulSlaveAddress &= 0x03UL;
	ulSlaveAddress |= i2cEEPROM_SLAVE_ADDRESS;
	FreeRTOS_ioctl( xI2CPort, ioctlSET_I2C_SLAVE_ADDRESS, ( void * ) ulSlaveAddress );

	return ( uint8_t ) ulByteAddress;
}
/*-----------------------------------------------------------*/

static void prvWritePageToEEPROM( Peripheral_Descriptor_t xI2CPort, uint32_t ulByteAddress )
{
int32_t lReturned;
const uint32_t ulMaxDelay = ( 500UL / portTICK_RATE_MS );

	/* The first byte of the transmitted data contains the least significant
	byte of the EEPROM byte address.  The slave address holds the most
	significant bits of the EEPROM byte address. */
	ucDataBuffer[ 0 ] = prvSetSlaveAddress( xI2CPort, ulByteAddress );

	/* Write the buffer to the EEPROM. */
	lReturned = FreeRTOS_write( xI2CPort, ucDataBuffer, i2cPAGE_SIZE + 1 );
	configASSERT( lReturned == ( i2cPAGE_SIZE + 1 ) );

	/* Wait until the write in complete, then allow it to program.  This is a
	very crude way of doing it - the EEPROM could alternatively be polled. */
	FreeRTOS_ioctl( xI2CPort, ioctlOBTAIN_WRITE_MUTEX, ( void * ) ulMaxDelay );
	vTaskDelay( i2cWRITE_CYCLE_DELAY );
}

