#ifndef __SCREENIMAGE_H__
#define __SCREENIMAGE_H__ "$Id: screenImage.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * screenImage.h
 *
 * This header file declares the screenImageRect()
 * routine for grabbing an image off of the screen
 *
 *
 * Change History : 
 *
 * $Log: screenImage.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbDev.h"
#include "image.h"

void screenImageRect( fbDevice_t        &fb,      // input
                      rectangle_t const &r,       // input: which portion of the screen
                      image_t           &img );   // output

#endif

