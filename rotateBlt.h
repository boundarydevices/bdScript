#ifndef __ROTATEBLT_H__
#define __ROTATEBLT_H__ "$Id: rotateBlt.h,v 1.1 2008-11-03 16:40:43 ericn Exp $"

/*
 * Module rotateBlt.h
 *
 * This header file defines a set of interfaces for blitting with
 * rotation of 0, 90, 180 or 270 degrees.
 *
 *
 * Change History : 
 *
 * $Log: rotateBlt.h,v $
 * Revision 1.1  2008-11-03 16:40:43  ericn
 * Added rotateBlt module
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

enum rotation_e {    // counter clock-wise (algebraic?) rotations
   ROTATENONE  = 0,
   ROTATE90    = 1,
   ROTATE180   = 2,
   ROTATE270   = 3,
   NUMROTATIONS
};

/*
 * This routine does no checking or translation
 *    src and dest should point at the upper-left corner of the source 
 *    and destination rectangles respectively
 */
void bltNormalized( rotation_e, 
                    void const *src, unsigned srcStride,
                    void       *dest, int perPixelAdder, int perLineAdder,
                    unsigned    bytesPerLine,        // copy this many of each line
                    unsigned    numLines );          // and this many lines

/*
 * Blts the rectangle from src [srcX:srcY,widthXheight] into dst at [dstX:dstY]
 *
 *    Note that the source position may be negative in x or y, but the width and height 
 *    will be adjusted smaller to compensate.
 *
 *    Same/same with destination position
 *
 * dstX, dstY, dstW, and dstH are all specified in virtual coordinates
 *
 * dst is the physical memory start, or pixel offset
 *
 *    [0,0]                for 0 degree rotation
 *    [width-1,0]          for 90 degree rotation
 *    [width-1,height-1]   for 180 degree rotation
 *    [0,height-1]         for 270 degree rotation
 */
void rotateBlt( rotation_e  rotation,
                unsigned    bytesPerPixel,
                unsigned    width,
                unsigned    height,
                void const *src, int srcX, int srcY, unsigned srcW, unsigned srcH,
                void       *dst, int dstX, int dstY, unsigned dstW, unsigned dstH );

#endif
