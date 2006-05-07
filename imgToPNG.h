#ifndef __IMGTOPNG_H__
#define __IMGTOPNG_H__ "$Id: imgToPNG.h,v 1.1 2006-05-07 15:41:15 ericn Exp $"

/*
 * imgToPNG.h
 *
 * This header file declares the imageToPNG() routine,
 * which converts an image into the PNG file format
 * (in RAM).
 *
 *
 * Change History : 
 *
 * $Log: imgToPNG.h,v $
 * Revision 1.1  2006-05-07 15:41:15  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#ifndef __IMAGE_H__
#include "image.h"
#endif

/*
 * If the routine returns true, pngData and size
 * will contain the content and content length.
 *
 * free() when done.
 */
bool imageToPNG( image_t const &img,
		 void const   *&pngData,
		 unsigned      &size );

#endif

