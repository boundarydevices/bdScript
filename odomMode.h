#ifndef __ODOMMODE_H__
#define __ODOMMODE_H__ "$Id: odomMode.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomMode.h
 *
 * This header file declares the odometerMode_e
 * enumeration (for separation from the image and
 * sm501alpha declarations)
 *
 *
 * Change History : 
 *
 * $Log: odomMode.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "sm501alpha.h"
#include "fbImage.h"

enum odometerMode_e {
   graphicsLayer = sm501alpha_t::rgba44,
   alphaLayer    = sm501alpha_t::rgba4444
};

inline fbImage_t::mode_t imageMode(odometerMode_e mode){ return (fbImage_t::mode_t)mode ; }
inline sm501alpha_t::mode_t alphaMode(odometerMode_e mode){ return (sm501alpha_t::mode_t)mode ; }

#endif

