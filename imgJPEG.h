#ifndef __IMGJPEG_H__
#define __IMGJPEG_H__ "$Id: imgJPEG.h,v 1.2 2002-11-20 00:38:33 ericn Exp $"

/*
 * imgJPEG.h
 *
 * This header file declares the imageJPEG routine,
 * which is used to translate a hunk o' RAM into a 
 * pixmap.
 *
 * Change History : 
 *
 * $Log: imgJPEG.h,v $
 * Revision 1.2  2002-11-20 00:38:33  ericn
 * -added comment
 *
 * Revision 1.1  2002/10/31 02:13:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

bool imageJPEG( void const    *inData,     // input
                unsigned long  inSize,     // input
                void const    *&pixData,   // output : delete [] when done
                unsigned short &width,     // output
                unsigned short &height );  // output

#endif

