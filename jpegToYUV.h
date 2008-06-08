#ifndef __JPEGTOYUV_H__
#define __JPEGTOYUV_H__ "$Id: jpegToYUV.h,v 1.1 2008-06-08 19:52:13 ericn Exp $"

/*
 * jpegToYUV.h
 *
 * This header file declares the jpegToYUV() routine,
 * which attempts to parse a JPEG file into a YUV
 * buffer, returning the buffer and length.
 *
 *
 * Change History : 
 *
 * $Log: jpegToYUV.h,v $
 * Revision 1.1  2008-06-08 19:52:13  ericn
 * add jpeg->YUV support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

bool jpegToYUV( void const    *inData,     // input
                unsigned long  inSize,     // input
                void const    *&pixData,   // output : delete [] when done 
                unsigned short &width,     // output
                unsigned short &height );  // output

#endif

