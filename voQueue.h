#ifndef __VOQUEUE_H__
#define __VOQUEUE_H__ "$Id: voQueue.h,v 1.1 2003-02-18 03:09:57 ericn Exp $"

/*
 * voQueue.h
 *
 * This header file declares the voQueue_t data structure
 * and the voQueueOpen() routine for use in writing data
 * to a multi-media (sound+video) queue
 *
 *
 * Change History : 
 *
 * $Log: voQueue.h,v $
 * Revision 1.1  2003-02-18 03:09:57  ericn
 * -Initial import
 *
 * Revision 1.1  2002/11/21 14:09:52  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "mpeg2dec/video_out.h"

typedef struct {
    vo_instance_t  vo;
    unsigned char *pixMem_ ;     // fill in prior to draw() call
    unsigned       stride_ ;
    unsigned       pixelsPerRow_ ;
    unsigned       numRows_ ;
} voQueue_t;

voQueue_t *voQueueOpen( void );

#endif

