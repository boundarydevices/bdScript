#ifndef __BMPTOLCD_H__
#define __BMPTOLCD_H__ "$Id: bmpToLCD.h,v 1.1 2004-01-14 05:35:38 ericn Exp $"

/*
 * bmpToLCD.h
 *
 * This header file declares the bmpToLCD() routine, which 
 * translates a monochrome .BMP file into LCD order for 
 * display on a monochrome LCD.
 *
 * I'm not sure of the make and model of the LCD panel, but
 * many appear to have in common a row-wise order with the
 * low-order bit corresponding to the top-most row, and 
 * adjacent bytes descending the display.
 *
 *
 * Change History : 
 *
 * $Log: bmpToLCD.h,v $
 * Revision 1.1  2004-01-14 05:35:38  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

bool bmpToLCD( void const     *bmpData,      // input
               unsigned        bmpBytes,     // input
               unsigned       &numColumns,   // output
               unsigned       &numRows,      // output
               unsigned char *&lcdData );    // output: delete [] when done



#endif

