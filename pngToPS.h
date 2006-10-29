#ifndef __PNGTOPS_H__
#define __PNGTOPS_H__ "$Id: pngToPS.h,v 1.1 2006-10-29 21:59:10 ericn Exp $"

/*
 * pngToPS.h
 *
 * This header file declares the pngToPostscript() routine, which will 
 * convert a PNG file to Postscript using the Indexed colorspace and
 * FlateDecode filter (required Postscript Language version 3).
 *
 * Output is handled by a user-supplied output routine.
 * 
 * Change History : 
 *
 * $Log: pngToPS.h,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

//
// App-supplied output routine. Return true to keep
// going, false to stop.
//
typedef bool (*postscript_output_t)( char  outchar,
                                     void *opaque );

bool pngToPostscript
      ( void const         *pngData,
        unsigned            pngLength,
        postscript_output_t outHandler,
        void               *outParam );

#endif

