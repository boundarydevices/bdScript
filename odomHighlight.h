#ifndef __ODOMHIGHLIGHT_H__
#define __ODOMHIGHLIGHT_H__ "$Id: odomHighlight.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomHighlight.h
 *
 * This header file declares the createHighlight()
 * routine that generates a highlight pattern in 
 * video RAM in 8-bit (4:4) alpha-layer format from
 * two gradients.
 *
 *
 * Change History : 
 *
 * $Log: odomHighlight.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

void createHighlight( unsigned char const *darkGrad,
                      unsigned char const *lightGrad,
                      unsigned             height,
                      unsigned             width,
                      unsigned char       *outData ); // generally fb ram

#endif

