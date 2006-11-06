#ifndef __CYLINDERSHADOW_H__
#define __CYLINDERSHADOW_H__ "$Id: cylinderShadow.h,v 1.1 2006-11-06 10:36:12 ericn Exp $"

/*
 * cylinderShadow.h
 *
 * This header file declares the cylinderShadow()
 * routine, which creates a frame-buffer shadow image
 * in rgb4444 form in SM-501 memory.
 *
 * Only a vertical gradient is currently supported.
 *
 * The gradient is produced based on the cosine of
 * a virtual circle centered at the middle of the
 * height. 
 *
 * Change History : 
 *
 * $Log: cylinderShadow.h,v $
 * Revision 1.1  2006-11-06 10:36:12  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbMem.h"

fbPtr_t cylinderShadow( unsigned width,
                        unsigned height,
                        double   power,           // > 1 deepens the shadow
                        double   divisor,         // > 1 makes it lighter
                        unsigned range,           // < height to fill top/bottom w/black
                        unsigned offset );        // <> 0 to shift

#endif

