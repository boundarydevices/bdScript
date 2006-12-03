#ifndef __BMPTOPNG_H__
#define __BMPTOPNG_H__ "$Id: bmpToPNG.h,v 1.1 2006-12-03 00:33:02 ericn Exp $"

/*
 * bmpToPNG.h
 *
 * This header file declares the bitmapToPNG() routine
 * which is mostly used to save paper when 
 *
 *
 * Change History : 
 *
 * $Log: bmpToPNG.h,v $
 * Revision 1.1  2006-12-03 00:33:02  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#ifndef __IMAGE_H__
#include "bitmap.h"
#endif

/*
 * If the routine returns true, pngData and size
 * will contain the content and content length.
 *
 * free() when done.
 */
bool bitmapToPNG( bitmap_t const &bmp,
                  void const    *&pngData,
                  unsigned       &size );

#endif

