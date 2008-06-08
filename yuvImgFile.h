#ifndef __YUVIMGFILE_H__
#define __YUVIMGFILE_H__ "$Id: yuvImgFile.h,v 1.1 2008-06-08 19:52:13 ericn Exp $"

/*
 * yuvImgFile.h
 *
 * This header file declares the yuvImageFromFile()
 * routine, which is a direct replacement for imgFromFile()
 * that returns a YUV-encoded image (if successful)
 *
 *
 * Change History : 
 *
 * $Log: yuvImgFile.h,v $
 * Revision 1.1  2008-06-08 19:52:13  ericn
 * add jpeg->YUV support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "image.h"

bool yuvImageFromFile( char const *fileName,
                       image_t    &image );

#endif

