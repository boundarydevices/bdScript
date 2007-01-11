#ifndef __IMGMONOTOPCL_H__
#define __IMGMONOTOPCL_H__ "$Id: imgMonoToPCL.h,v 1.1 2007-01-11 21:34:11 ericn Exp $"

/*
 * imgMonoToPCL.h
 *
 * This header file declares the imgMonoToPCL()
 * routine, that will generate an un-compressed
 * bitmap string in PCL form for the given input
 * image.
 * 
 * Note that this routine also returns the following
 * commands prior to the bitmap data itself:
 *
 *    <ESC>*r3F         - set rotation to page orientation
 *    <ESC>*t600R       - set DPI to 600
 *    <ESC>*r141T       - set number of raster lines
 *    <ESC>*r450S       - set number of pixels / line
 *    <ESC>*r1U         - simple, black & white palette
 *    <ESC>*r1A         - start raster graphics at current position
 *    <ESC>*b0Y         - move by 0 lines
 *    <ESC>*b0M         - compression mode zero
 *
 * The raster data is preceded by this escape sequence
 *
 *    <ESC>*b1350W      - raster data goes here
 *                        (one of these for each row)
 *
 * and finally, the end of the image is terminated like so:
 *
 *    <ESC>*r0C         - end raster graphics
 *
 * Change History : 
 *
 * $Log: imgMonoToPCL.h,v $
 * Revision 1.1  2007-01-11 21:34:11  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#ifndef __IMAGE_H__
#include "image.h"
#endif

void imgMonoToPCL( image_t const &img,       // input
                   char         *&outPCL,    // output: free() when done
                   unsigned      &outLen );  // output

#endif
