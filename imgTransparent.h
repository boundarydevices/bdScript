#ifndef __IMGTRANSPARENT_H__
#define __IMGTRANSPARENT_H__ "$Id: imgTransparent.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * imgTransparent.h
 *
 * This header file declares the imgTransparent()
 * routine, used to reduce the opacity of an image
 * to a specified level. Partially transparent data
 * in the input will be reduced accordingly.
 *
 *
 * Change History : 
 *
 * $Log: imgTransparent.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "image.h"

void imgTransparent( image_t       &img,
                     unsigned char  maxOpacity );

#endif

