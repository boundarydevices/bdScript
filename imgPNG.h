#ifndef __IMGPNG_H__
#define __IMGPNG_H__ "$Id: imgPNG.h,v 1.2 2002-11-20 00:38:28 ericn Exp $"

/*
 * imgPNG.h
 *
 * This header file declares the imagePNG() routine,
 * which tries to convert a hunk of memory into a pixmap
 * by translating PNG.
 *
 *
 * Change History : 
 *
 * $Log: imgPNG.h,v $
 * Revision 1.2  2002-11-20 00:38:28  ericn
 * -added comment
 *
 * Revision 1.1  2002/10/31 02:13:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

bool imagePNG( void const    *inData,     // input
               unsigned long  inSize,     // input
               void const    *&pixData,   // output : delete [] when done
               unsigned short &width,     // output
               unsigned short &height );  // output



#endif

