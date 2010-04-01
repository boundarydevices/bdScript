/*
 * Module fourcc.cpp
 *
 * This module defines the fourcc utility routines described
 * in fourcc.h
 *
 * Change History : 
 *
 * $Log$
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "fourcc.h"
#include <linux/videodev2.h>
#include <stdio.h>

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

static unsigned supported_formats[] = {
	V4L2_PIX_FMT_NV12    /* 12  Y/CbCr 4:2:0  */
,	V4L2_PIX_FMT_YUV422P /* 16  YVU422 planar */
,	V4L2_PIX_FMT_YVU420  /* 12  YVU 4:2:0     */
,	V4L2_PIX_FMT_YUYV    /* 16  YUV 4:2:2     */
,	V4L2_PIX_FMT_UYVY    /* 16  YUV 4:2:2     */
,	V4L2_PIX_FMT_Y41P    /* 12  YUV 4:1:1     */
,	V4L2_PIX_FMT_YUV444  /* 16  xxxxyyyy uuuuvvvv */
,	V4L2_PIX_FMT_YUV410  /*  9  YUV 4:1:0     */
,	V4L2_PIX_FMT_YUV420  /* 12  YUV 4:2:0     */
,	V4L2_PIX_FMT_YUV422P /* 16  YVU422 planar */
};

bool supported_fourcc(char const *arg, unsigned &fourcc){
	fourcc = fourcc_from_str(arg);
	for( unsigned i = 0 ; i < ARRAY_SIZE(supported_formats); i++ ){
		if(fourcc == supported_formats[i])
			return true ;
	}
	return false ;
}

bool supported_fourcc(unsigned fourcc)
{
	for( unsigned i = 0 ; i < ARRAY_SIZE(supported_formats); i++ ){
		if(fourcc == supported_formats[i])
			return true ;
	}
	return false ;
}

void supported_fourcc_formats(unsigned const *&values, unsigned &numValues)
{
    values = supported_formats ;
    numValues = ARRAY_SIZE(supported_formats);
}

unsigned bits_per_pixel(unsigned fourcc)
{
    unsigned bytes = 1 ;
    switch(fourcc){
    	case V4L2_PIX_FMT_SBGGR16:
    	case V4L2_PIX_FMT_RGB565:
    	case V4L2_PIX_FMT_YUYV:
    	case V4L2_PIX_FMT_UYVY:
    		bytes= 2;
            break;
    	case V4L2_PIX_FMT_BGR24:
    	case V4L2_PIX_FMT_RGB24:
    		bytes= 3;
            break;
    	case V4L2_PIX_FMT_BGR32:
    	case V4L2_PIX_FMT_RGB32:
    		bytes=4;
            break;
    }
    return bytes*8 ;
}

#ifdef STANDALONE_FOURCC
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char const * const argv[]){
    if (1 < argc) {
        for( int arg = 1 ; arg < argc ; arg++ ){
            char const *param = argv[arg];
            if( '0' == *param ){
                unsigned binary = strtoul(param,0,0);
                bool supported = supported_fourcc(binary);
                printf( "fourcc(%s:0x%08x) == '%s'%s\n", param, binary, fourcc_str(binary), supported ? " supported" : " not supported");
            } else
                printf( "fourcc(%s) == 0x%08x\n", param, fourcc_from_str(param));
        }
    }
    else
        fprintf(stderr, "Usage: %s 0xvalue or STRG\n", argv[0]);

    return 0 ;
}
#endif
