#ifndef __IMGGIF_H__
#define __IMGGIF_H__ "$Id: imgGIF.h,v 1.1 2002-10-31 02:13:08 ericn Exp $"

/*
 * imgGIF.h
 *
 * This header file declares the imageGIF() routine,
 * which will try to parse a hunk of memory and return
 * a pixmap of the data.
 *
 *
 * Change History : 
 *
 * $Log: imgGIF.h,v $
 * Revision 1.1  2002-10-31 02:13:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

bool imageGIF( void const    *inData,     // input
               unsigned long  inSize,     // input
               void const    *&pixData,   // output
               unsigned short &width,     // output
               unsigned short &height );  // output

#endif

