#ifndef __FBIMAGE_H__
#define __FBIMAGE_H__ "$Id: fbImage.h,v 1.1 2006-06-14 13:55:22 ericn Exp $"

/*
 * fbImage.h
 *
 * This header file declares a handful of routines
 * that get and put images from/to the frame-buffer device
 *
 *
 * Change History : 
 *
 * $Log: fbImage.h,v $
 * Revision 1.1  2006-06-14 13:55:22  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbDev.h"
#include "image.h"

void screenImage( fbDevice_t &fb,      // input
                  image_t    &img );   // output

void screenImageRect( fbDevice_t        &fb,      // input
                      rectangle_t const &r,       // input: which portion of the screen
                      image_t           &img );   // output

void showImage( fbDevice_t    &fb,
                unsigned       x,
                unsigned       y,
                image_t const &img );

#endif

