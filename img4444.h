#ifndef __IMG4444_H__
#define __IMG4444_H__ "$Id: img4444.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * img4444.h
 *
 * This header file declares a routine and class to
 * convert an image with alpha to RGBA 4444 format 
 * (usually in SM-501 frame buffer memory).
 *
 *
 * Change History : 
 *
 * $Log: img4444.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbMem.h"
#include "image.h"

void imgTo4444( unsigned short const *inPixels,
                unsigned              width,
                unsigned              height,
                unsigned char const  *inAlpha,
                unsigned short       *out4444,
                unsigned              outStride ); // in pixels
                
#endif

