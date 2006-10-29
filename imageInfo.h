#ifndef __IMAGEINFO_H__
#define __IMAGEINFO_H__ "$Id: imageInfo.h,v 1.1 2006-10-29 21:59:10 ericn Exp $"

/*
 * imageInfo.h
 *
 * This header file declares the getImageInfo() routine, 
 * which parses the header of an image file and returns
 * the type, width, and height.
 *
 *
 * Change History : 
 *
 * $Log: imageInfo.h,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#ifndef __IMAGE_H__
#include "image.h"
#endif

struct imageInfo_t {
   image_t::type_e   type_ ;
   unsigned          width_ ;
   unsigned          height_ ;
};

bool getImageInfo( void const  *imgData,        // input
                   unsigned     imgLen,         // input
                   imageInfo_t &details );      // output

#endif

