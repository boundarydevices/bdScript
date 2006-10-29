#ifndef __IMAGETOPS_H__
#define __IMAGETOPS_H__ "$Id: imageToPS.h,v 1.1 2006-10-29 21:59:10 ericn Exp $"

/*
 * imageToPS.h
 *
 * This header file declares the imageToPS() routine, which 
 * will create a compressed Postscript 'colorimage' sequence
 * of the following form:
 *
 *
         /pstr 960 string def                  % line length: 3*width
         /inputf                               % read input routine
           currentfile
           /ASCII85Decode   filter
           /FlateDecode     filter
         def
         0 240 translate
         320 240 scale
         320 240 8 [320 0 0 -240 0 0]
         { inputf pstr readstring pop }
         false
         3
         colorimage
         GhVPUGu.Mm*QPLo8Mj.R3jI-lSX?dm;ql$H9KJ.PC5beZJoq*\BM:6$e;-Rj</<%s
         6pXdKE<Sa8&4CI>6,AO3-is(M0rAZD0s)?O/iDm;mVb(+]_B,:hRi7@."4C1gc=gS
         ...
         >P&jT*SmDVLIR7ArSeX.GiXuW-sf7.,5hun5!15Akh0U@X]r9;o`JYiaITEirr\sO
         kpl
         ~>
         currentdict /inputf undef
         currentdict /pstr undef
 *         
 * 
 * The choice of FlateDecode or DCTDecode is made based on whether
 * the input image is PNG or JPEG. In the case of JPEG data, no
 * direct compression is performed by this routine.
 *
 * In order to pull this off, the input image width and height are 
 * determined before encoding (see imageInfo.h).
 *
 * The output is scaled to the specified on-page rectangle, specified
 * in Postscript units (normally 1/100th of an inch).
 *
 * This routine does not save and restore the graphics state.
 *
 * This routine calls the specified routine to perform output.
 *
 * Change History : 
 *
 * $Log: imageToPS.h,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#ifndef __FBDEV_H__
#include "fbDev.h"
#endif

typedef bool (*image_ps_output_t)( char const *outData,
                                   unsigned    outLength,
                                   void       *opaque );

bool imageToPS( void const        *imgData,      // input: image data
                unsigned           imgLen,       // input: bytes
                rectangle_t const &outRect,      // in printer units
                image_ps_output_t  outHandler,
                void              *outParam );

#endif

