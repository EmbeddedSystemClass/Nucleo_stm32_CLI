/*****************************************************************************
 *   oled.h:  Header file for OLED Display
 *
 *   Copyright(C) 2009, Embedded Artists AB
 *   All rights reserved.
 *
******************************************************************************/
#ifndef __OLED_H
#define __OLED_H

#define oledDISPLAY_WIDTH  	96
#define oledDISPLAY_HEIGHT 	64
#define oledCHARACTER_HEIGHT 8


typedef enum
{
    OLED_COLOR_BLACK,
    OLED_COLOR_WHITE
} oled_color_t;


void vOLEDInit( Peripheral_Descriptor_t xI2CPortIn );
void vOLEDPutString( uint8_t ucRow, uint8_t *pucString, oled_color_t xForegroundColour, oled_color_t xBackgroundColour );
void vOLEDClearScreen( oled_color_t xColour );
void vOLEDRefreshDisplay( void );

#endif /* end __OLED_H */
/****************************************************************************
**                            End Of File
*****************************************************************************/
