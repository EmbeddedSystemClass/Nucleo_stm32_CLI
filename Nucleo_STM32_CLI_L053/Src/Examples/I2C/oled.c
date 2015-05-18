/*****************************************************************************
 *
 *   Character bit-mapping and initialisation codes Copyright(C) 2009, Embedded Artists AB and used with permission
 *   Otherwise Copyright(C) 2011, Real Time Engineers ltd.
 *
 ******************************************************************************/

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+IO library includes. */
#include "FreeRTOS_IO.h"

/* Standard includes. */
#include <string.h>

/* Hardware/driver includes. */
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "oled.h"
#include "font5x7.h"

/* The display controller can handle a resolution of 132x64. The OLED
on the base board is 96x64.  Divide by 8 as the buffer is in bytes, not in
bits. */
#define oledSHADOW_BUFFER_SIZE ( oledDISPLAY_WIDTH * ( oledDISPLAY_HEIGHT >> 3 ) )
#define oledNUM_CHAR_ROWS ( oledSHADOW_BUFFER_SIZE / oledDISPLAY_WIDTH )

/* Hardware specifics, pixel colours and the offset to the first visible
pixel. */
#define oledWHITE 					( 0xff )
#define oledBLACK 					( 0x00 )
#define oledCOLUMN_OFFSET 			( 18 )
#define oledCHAR_WIDTH				( 6 )
/*-----------------------------------------------------------*/

/*
 * Scan the shadow buffer, writing any dirty lines to the oled.
 */
static void prvWriteShadowBufferToLED( uint32_t ulRow );

/*
 * Write a pixel of the given colour into the shadow frame buffer, at the
 * position specified by the ucRow and ucColumn parameters.
 */
static void prvWritePixelToFrameBuffer( uint8_t uxColumn, uint8_t ucRow, oled_color_t xColour );

/*
 * Write the character ucChar to the oled at the specified row and column
 * position, using the specified colours.
 */
static void prvWriteCharToFrameBuffer( uint8_t ucColumn, uint8_t ucRow, uint8_t ucChar, oled_color_t xForegroundColour, oled_color_t xBackgroundColour );

/*-----------------------------------------------------------*/

/* The shadow buffer holds an image of the oled's bit map in RAM.  It is
created one byte larger than it needs to be so the write command can be
written into the first byte before the entire character row is sent to
the oled in a single I2C transfer. */
static uint8_t ucFrameBuffer[ oledSHADOW_BUFFER_SIZE + 1 ] = { 0 };

/* The pointer to the frame buffer misses out the first byte, as the first byte
does not contain a bit map. */
static uint8_t * const pucFrameBuffer = &( ucFrameBuffer[ 1 ] );

/* Mask each bit in the byte in turn. */
static uint8_t const ucByteBitMasks[ 8 ] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

/* The handle to the opened I2C port - required by FreeRTOS+IO calls. */
static Peripheral_Descriptor_t xI2CPort = NULL;

/* A flag for each of the character rows.  A 0 in an index means the
corresponding character row has not changed since it was last written to the
oled. */
static uint8_t ucRowDirty[ oledNUM_CHAR_ROWS ] = { 0 };

const uint8_t ucInitData[] =
{
	/* First byte denotes a command with continuation, second byte is the
	command itself. */
	0x80, 0x02, /* set low column address */
	0x80, 0x12, /* set high column address */
	0x80, 0x40, /* display start set */
	0x80, 0x2e, /* stop horizontal scroll */
	0x80, 0x81, /* set contrast control register */
	0x80, 0x32,
	0x80, 0x82, /* brightness for color banks */
	0x80, 0x80, /* display on */
	0x80, 0xa1, /* set segment re-map */
	0x80, 0xa6, /* set normal/inverse display */
	0x80, 0xa8, /* set multiplex ratio */
	0x80, 0x3F,
	0x80, 0xd3,	/* set display offset */
	0x80, 0x40,
	0x80, 0xad, /* set dc-dc on/off */
	0x80, 0x8E,
	0x80, 0xc8, /* set com output scan direction */
	0x80, 0xd5, /* set display clock divide ratio/oscillator/frequency */
	0x80, 0xf0,
	0x80, 0xd8, /* set area color mode on/off & low power display mode  */
	0x80, 0x05,
	0x80, 0xd9, /* set pre-charge period */
	0x80, 0xf1,
	0x80, 0xda, /* set com pins hardware configuration */
	0x80, 0x12,
	0x80, 0xdb, /* set vcom deselect level */
	0x80, 0x34,
	0x80, 0x91, /* set look up table for area color */
	0x80, 0x3f,
	0x80, 0x3f,
	0x80, 0x3f,
	0x80, 0x3f,
	0x80, 0xaf, /* display on */
	0x80, 0xa4 	/* display on */
};
/*-----------------------------------------------------------*/

void vOLEDInit( Peripheral_Descriptor_t xI2CPortIn )
{
	configASSERT( xI2CPortIn );

	/* Store the handle to the I2C port that is going to be used for
	communicating with the OLED. */
	xI2CPort = xI2CPortIn;

	/* Configure the GPIO. */
	GPIO_SetDir( 2U, ( 1UL << 1UL ), 1U );
	GPIO_SetDir( 2U, ( 1UL << 7UL ), 1U );
	GPIO_SetDir( 0U, ( 1UL << 6UL ), 1U );

	/* Make sure power is off */
	GPIO_ClearValue( 2U, ( 1UL << 1UL ) );

	/* D/C# */
	GPIO_ClearValue( 2U, ( 1UL << 7UL ) );

	/* CS# */
	GPIO_ClearValue( 0U, ( 1UL << 6UL ) );

	/* Initialise the oled controller. */
	FreeRTOS_write( xI2CPort, ( uint8_t * ) ucInitData, sizeof( ucInitData ) );

	/* Clear down the frame buffer. */
	memset( ucFrameBuffer, 0x00, sizeof( ucFrameBuffer ) );

	/* small delay before turning on power */
	vTaskDelay( 100 / portTICK_RATE_MS );
	vOLEDClearScreen( OLED_COLOR_BLACK );
	GPIO_SetValue( 2, (1<<1) );
}
/*-----------------------------------------------------------*/

void vOLEDClearScreen( oled_color_t xColour )
{
	if( xColour == OLED_COLOR_WHITE )
	{
		memset( ucFrameBuffer, oledWHITE, sizeof( ucFrameBuffer ) );
	}
	else
	{
		memset( ucFrameBuffer, oledBLACK, sizeof( ucFrameBuffer ) );
	}

	/* Mark all rows as dirty. */
	memset( ucRowDirty, pdTRUE, sizeof( ucRowDirty ) );

	vOLEDRefreshDisplay();
}
/*-----------------------------------------------------------*/

void vOLEDRefreshDisplay( void )
{
uint32_t ulRow;

	for( ulRow = 0; ulRow < oledNUM_CHAR_ROWS; ulRow++ )
	{
		if( ucRowDirty[ ulRow ] != pdFALSE )
		{
			ucRowDirty[ ulRow ] = pdFALSE;
			prvWriteShadowBufferToLED( ulRow * oledCHARACTER_HEIGHT );
		}
	}
}
/*-----------------------------------------------------------*/

void vOLEDPutString( uint8_t ucRow, uint8_t *pucString, oled_color_t xForegroundColour, oled_color_t xBackgroundColour )
{
uint8_t ucColumn = 1U;

	while( *pucString != '\0' )
	{
		prvWriteCharToFrameBuffer( ucColumn, ucRow, *pucString, xForegroundColour, xBackgroundColour );
		ucColumn += oledCHAR_WIDTH;
		pucString++;
	}

	/* Mark the row written to as being dirty. */
	ucRow /= oledCHARACTER_HEIGHT;
	if( ucRow < oledNUM_CHAR_ROWS )
	{
		ucRowDirty[ ucRow ] = pdTRUE;
	}

	/* Also mark the row below as being dirty, in case the characters being
	written do not line up exactly with a row multiple. */
	if( ( ucRow + 1U ) < oledNUM_CHAR_ROWS )
	{
		ucRowDirty[ ucRow + 1U ] = pdTRUE;
	}

  return;
}

/*-----------------------------------------------------------*/

static void prvWritePixelToFrameBuffer( uint8_t ucColumn, uint8_t ucRow, oled_color_t xColour )
{
uint8_t ucPage, ucMask;
uint16_t usAdd;
uint32_t ulBytePosition = 0UL;

	/* Modified from Embedded Artists source file. */

	if( ( ucColumn <= oledDISPLAY_WIDTH ) && ( ucRow <= oledDISPLAY_HEIGHT ) )
	{
		/* Set the page address. */
		if( ucRow < 8U )
		{
			ucPage = 0x0U;
		}
		else if( ucRow < 16U )
		{
			ucPage = 0x1U;
		}
		else if( ucRow < 24U )
		{
			ucPage = 0x2U;
		}
		else if( ucRow < 32U )
		{
			ucPage = 0x3U;
		}
		else if( ucRow < 40U )
		{
			ucPage = 0x4U;
		}
		else if( ucRow < 48U )
		{
			ucPage = 0x5U;
		}
		else if( ucRow < 56U )
		{
			ucPage = 0x6U;
		}
		else
		{
			ucPage = 0x7U;
		}

		usAdd = ucColumn + oledCOLUMN_OFFSET;

		/* Calculate a mask from rows, basically do a ( row % 8 ), and
		the remainder is the bit position.

		Junk first three bits by dividing by 8 then multiplying by 8. */
		usAdd = ucRow >> 3U;
		usAdd <<= 3U;

		/* The remainder is the the difference. */
		usAdd = ucRow - usAdd;

		/* Left shift to bit position. */
		ucMask = 1U << usAdd;

		ulBytePosition = ( ucPage * oledDISPLAY_WIDTH ) + ucColumn;

		if( xColour > 0 )
		{
			pucFrameBuffer[ ulBytePosition ] |= ucMask;
		}
		else
		{
			pucFrameBuffer[ ulBytePosition ] &= ~ucMask;
		}
	}
}
/*-----------------------------------------------------------*/

static void prvWriteCharToFrameBuffer( uint8_t ucColumn, uint8_t ucRow, uint8_t ucChar, oled_color_t xForegroundColour, oled_color_t xBackgroundColour )
{
uint8_t ucData = 0;
uint8_t i = 0, j = 0;
oled_color_t xColour = OLED_COLOR_BLACK;

	/* Modified from Embedded Artists source file. */

	if( ( ucColumn < ( oledDISPLAY_WIDTH - 8 ) ) && ( ucRow < ( oledDISPLAY_HEIGHT - 8 ) ) )
	{
		/* Ensure a valid character is being asked for.  If not, just display
		a space. */
		if( ( ucChar < ' ' ) || ( ucChar > '~' ) )
		{
			ucChar = ' ';
		}

		/* Convert ascii to integer. */
		ucChar -= ' ';

		for( i = 0U; i < 8U; i++ )
		{
			ucData = font5x7[ ucChar ][ i ];

			for( j = 0U; j < 6U; j++ )
			{
				if( ( ucData&ucByteBitMasks[ j ] ) == 0U )
				{
					xColour = xBackgroundColour;
				}
				else
				{
					xColour = xForegroundColour;
				}

				prvWritePixelToFrameBuffer( ucColumn, ucRow, xColour );
				ucColumn++;
			}

			ucRow++;
			ucColumn -= oledCHAR_WIDTH;
		}
	}
}
/*-----------------------------------------------------------*/

static void prvWriteShadowBufferToLED( uint32_t ulRow )
{
uint8_t ucPage, ucTemp, *pucFirstFrameBufferByte;
uint8_t ucBuffer[ 6 ];
uint32_t ulMaxDelay = 500UL / portTICK_RATE_MS;

	if( ulRow < 8UL )
	{
		ucPage = 0xB0UL;
	}
	else if( ulRow < 16UL )
	{
		ucPage = 0xB1UL;
	}
	else if( ulRow < 24UL )
	{
		ucPage = 0xB2UL;
	}
	else if( ulRow < 32UL )
	{
		ucPage = 0xB3UL;
	}
	else if( ulRow < 40UL )
	{
		ucPage = 0xB4UL;
	}
	else if( ulRow < 48UL )
	{
		ucPage = 0xB5UL;
	}
	else if( ulRow < 56UL )
	{
		ucPage = 0xB6UL;
	}
	else
	{
		ucPage = 0xB7UL;
	}

	/* Set the address in the OLED controller. */
	ucBuffer[ 0 ] = 0x80; 							/* Write Co & D/C bits */
	ucBuffer[ 1 ] = ucPage; 						/* Data */
	ucBuffer[ 2 ] = 0x80;
	ucBuffer[ 3 ] = ( 0x0F & oledCOLUMN_OFFSET ); 	/* Lower address. */
	ucBuffer[ 4 ] = 0x80;
	ucBuffer[ 5 ] = ( 0x10 | ( oledCOLUMN_OFFSET >> 4 ) );	/* Higher address. */
	FreeRTOS_write( xI2CPort, ucBuffer, sizeof( ucBuffer ) );

	/* Find first byte to send. */
	pucFirstFrameBufferByte = pucFrameBuffer + ( ( ulRow / oledCHARACTER_HEIGHT ) * ( oledDISPLAY_WIDTH ) );

	/* Back up by one. */
	pucFirstFrameBufferByte--;

	/* Remember the byte at the new position, as it is going to be temporarily
	replaced. */
	ucTemp = *pucFirstFrameBufferByte;

	/* Replace the byte with the write data command. */
	*pucFirstFrameBufferByte = 0x40;

	/* Ensure the previous data is written before writing more. */
	if( FreeRTOS_ioctl( xI2CPort, ioctlOBTAIN_WRITE_MUTEX, ( void * ) ulMaxDelay ) == pdPASS )
	{
		/* Write the data, using a length + 1 as the command is also being sent. */
		FreeRTOS_write( xI2CPort, pucFirstFrameBufferByte, oledDISPLAY_WIDTH + 1 );

		/* Ensure the previous data is written before restoring the clobbered byte. */
		FreeRTOS_ioctl( xI2CPort, ioctlOBTAIN_WRITE_MUTEX, ( void * ) ulMaxDelay );
	}

	/* Restore the overwritten byte. */
	*pucFirstFrameBufferByte = ucTemp;
}


