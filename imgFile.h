#ifndef __IMGFILE_H__
#define __IMGFILE_H__ "$Id: imgFile.h,v 1.1 2003-10-18 19:16:16 ericn Exp $"

/*
 * imgFile.h
 *
 * This header file declares the imageFromFile() 
 * routine, which tries to load an image from file.
 *
 *
 * Change History : 
 *
 * $Log: imgFile.h,v $
 * Revision 1.1  2003-10-18 19:16:16  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "image.h"

bool imageFromFile( char const *fileName,
                    image_t    &image );

#endif

