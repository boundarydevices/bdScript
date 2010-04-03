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
#include <stdint.h>
#include <linux/ipu.h>
#include <stdio.h>

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

static const struct pixformat_info pix_formats[] = {
	{V4L2_PIX_FMT_YUV420,	IPU_PIX_FMT_YUV420P,	"YUV420",	1, PXF_PLANAR | PXF_PLANAR_UV_W_HALF | PXF_PLANAR_UV_H_HALF | PXF_COLORSPACE_YUV, 1, 0},
	{V4L2_PIX_FMT_YVU420,	IPU_PIX_FMT_YUV420P,	"YVU420",	1, PXF_PLANAR | PXF_PLANAR_UV_W_HALF | PXF_PLANAR_UV_H_HALF | PXF_PLANAR_V_FIRST | PXF_COLORSPACE_YUV, 1, 0},
	{V4L2_PIX_FMT_NV12,	IPU_PIX_FMT_NV12,	"NV12",		1, PXF_PLANAR | PXF_PLANAR_UV_W_HALF | PXF_PLANAR_UV_H_HALF | PXF_PLANAR_PARTIAL | PXF_COLORSPACE_YUV, 0, 0},
	{V4L2_PIX_FMT_YUV422P,	IPU_PIX_FMT_YUV422P,	"YUV422P",	1, PXF_PLANAR | PXF_PLANAR_UV_W_HALF, 0, 0},
	{fourcc('Y','V','1','6'),IPU_PIX_FMT_YVU422P,	"YVU422P",	1, PXF_PLANAR | PXF_PLANAR_UV_W_HALF | PXF_PLANAR_V_FIRST | PXF_COLORSPACE_YUV, 0, 0},
	{V4L2_PIX_FMT_SGRBG8,	IPU_PIX_FMT_SGRBG8,	"SGRBG8",	1, 0, 0, 1},
	{V4L2_PIX_FMT_SBGGR8,	IPU_PIX_FMT_SBGGR8,	"SBGGR8",	1, 0, 0, 1},
	{V4L2_PIX_FMT_SGBRG8,	IPU_PIX_FMT_SGBRG8,	"SGBRG8",	1, 0, 0, 1},
	{V4L2_PIX_FMT_SGRBG10,	IPU_PIX_FMT_SGRBG10,	"SGRBG10",	2, 0, 0, 1},
	{V4L2_PIX_FMT_SBGGR16,	IPU_PIX_FMT_SBGGR16,	"SBGGR16",	2, 0, 0, 1},
	{V4L2_PIX_FMT_RGB565,	IPU_PIX_FMT_RGB565,	"RGB565",	2, 0, 1, 0},
	{V4L2_PIX_FMT_UYVY,	IPU_PIX_FMT_UYVY,	"UYVY",		2, PXF_COLORSPACE_YUV, 1, 0},
	{V4L2_PIX_FMT_YUYV,	IPU_PIX_FMT_YUYV,	"YUYV",		2, PXF_COLORSPACE_YUV, 1, 0},
	{V4L2_PIX_FMT_RGB24,	IPU_PIX_FMT_RGB24,	"RGB24",	3, 0, 0, 0},
	{V4L2_PIX_FMT_BGR24,	IPU_PIX_FMT_BGR24,	"BGR24",	3, 0, 0, 0},
	{V4L2_PIX_FMT_RGB32,	IPU_PIX_FMT_RGB32,	"RGB32",	4, 0, 0, 0},
	{V4L2_PIX_FMT_BGR32,	IPU_PIX_FMT_BGR32,	"BGR32",	4, 0, 0, 0},
	{IPU_PIX_FMT_YUV444,	IPU_PIX_FMT_YUV444,	"YUV444",	4, PXF_COLORSPACE_YUV, 0, 0},
	{IPU_PIX_FMT_BGRA32,	IPU_PIX_FMT_BGRA32,	"BGRA",		4, 0, 0, 0},
	{IPU_PIX_FMT_RGBA32,	IPU_PIX_FMT_RGBA32,	"RGBA",		4, 0, 0, 0},
	{IPU_PIX_FMT_ABGR32,	IPU_PIX_FMT_ABGR32,	"ABGR",		4, 0, 0, 0},
	{IPU_PIX_FMT_GENERIC,   IPU_PIX_FMT_GENERIC,    "gen8",		1, 0, 0, 0},
};

static unsigned const *supported_formats = 0 ;

const struct pixformat_info *get_pixel_format_info(__u32 pixelformat)
{
	int i;
	const struct pixformat_info *pix_info = pix_formats;
	for (i=0; i< ARRAY_SIZE(pix_formats); i++, pix_info++) {
		if (pix_info->v4l2_format == pixelformat)
			return pix_info;
	}
	return NULL;
}


bool supported_fourcc(char const *arg, unsigned &fourcc){
	fourcc = fourcc_from_str(arg);
	for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
		if(fourcc == pix_formats[i].v4l2_format)
			return true ;
	}
	return false ;
}

bool supported_fourcc(unsigned fourcc){
	for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
		if(fourcc == pix_formats[i].v4l2_format)
			return true ;
	}
	return false ;
}

void supported_fourcc_formats(unsigned const *&values, unsigned &numValues)
{
	if( 0 == supported_formats ){
		unsigned *fmts = new unsigned [ARRAY_SIZE(pix_formats)];
		for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
			fmts[i] = pix_formats[i].v4l2_format ;
		}
		supported_formats = (unsigned const *)fmts ;
	}

	values = supported_formats ;
	numValues = ARRAY_SIZE(pix_formats);
}

unsigned bits_per_pixel(unsigned fourcc)
{
	int i;
	const struct pixformat_info *pix_info = pix_formats;
	for (i=0; i< ARRAY_SIZE(pix_formats); i++, pix_info++) {
		if (pix_info->v4l2_format == fourcc)
			return pix_info->bpp*8 ;
	}
	return 0;
}

bool fourccOffsets(
	unsigned fourcc,
	unsigned width,
	unsigned height,
	unsigned &ysize,
	unsigned &yoffs,
	unsigned &yadder,
	unsigned &uvsize,
	unsigned &uvrowdiv,
	unsigned &uvcoldiv,
	unsigned &uoffs, 
	unsigned &voffs, 
	unsigned &uvadder,
	unsigned &totalsize)
{
	ysize = yoffs = yadder = uvsize = uoffs = voffs = uvadder = totalsize = 0 ; // make 'em harmless
	uvrowdiv = uvcoldiv = 1 ;
	
	width = (width+31)&~31 ;	// columns multiple of 32
	height = (height+1)&~1 ;	// even number of rows

	for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
		if(fourcc == pix_formats[i].v4l2_format){
                        const struct pixformat_info *pix_info = pix_formats+i ;
			if( 0 == (pix_info->flags&PXF_COLORSPACE_YUV) )
				return false ; // not YUV
			if(pix_info->flags & PXF_PLANAR) {
				yadder = pix_info->bpp ;
				ysize  = width*height*yadder ;
				yoffs  = 0 ;
				if(pix_info->flags&PXF_PLANAR_UV_W_HALF)
					uvcoldiv = 2 ;
				if(pix_info->flags&PXF_PLANAR_UV_H_HALF)
					uvrowdiv = 2 ;
				uvsize = (ysize/uvrowdiv)/uvcoldiv;

				uoffs = ysize ;
				uvadder = 1 ;
				if(pix_info->flags&PXF_PLANAR_PARTIAL){
					voffs = uoffs+1 ;
					uvadder *= 2 ;
				}
				else
					voffs = uoffs+uvsize ;
				totalsize = uoffs+uvsize ;
				if(pix_info->flags&PXF_PLANAR_V_FIRST){
					unsigned tmp = uoffs ;
					uoffs = voffs ;
					voffs = tmp ;
				}
			} else {
				unsigned bpp = pix_info->bpp ;
				// Note that we're not handling 4:4:4 here
                                yadder = bpp ;
				ysize  = width*height*yadder ;
				uvadder = yadder*2 ;
                                if(pix_info->flags & PXF_COLORSPACE_YV_FIRST) {
					yoffs = bpp ;
					if(pix_info->flags&PXF_PLANAR_V_FIRST){
						voffs = 0 ;
						uoffs = yadder ;
					} else {
						uoffs = 0 ;
						voffs = yadder ;
					}
				} else {
					yoffs = 0 ;
					if(pix_info->flags&PXF_PLANAR_V_FIRST){
						voffs = bpp ;
						uoffs = bpp+yadder ;
					} else {
						uoffs = bpp ;
						voffs = bpp+yadder ;
					}
				}
				totalsize = ysize ;
				uvsize = width*height*uvadder ;
			}
			return true ;
		}
	}
	return false ;
}

#ifdef STANDALONE_FOURCC
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char const * const argv[]){
    if (1 < argc) {
        for( int arg = 1 ; arg < argc ; arg++ ){
	    unsigned binary = 0 ;
	    bool supported = false ;
            char const *param = argv[arg];
            if( '0' == *param ){
                binary = strtoul(param,0,0);
                supported = supported_fourcc(binary);
                printf( "fourcc(%s:0x%08x) == '%s'%s\n", param, binary, fourcc_str(binary), supported ? " supported" : " not supported");
            } else {
		binary = fourcc_from_str(param);
		supported = supported_fourcc(binary);
                printf( "fourcc(%s) == 0x%08x%s\n", param, binary, supported ? " supported" : " not supported");
	    }
	    if(supported){
			unsigned ysize ;
			unsigned yoffs ;
			unsigned yadder ;
			unsigned uvsize ;
			unsigned uvrowdiv ;
			unsigned uvcoldiv ;
			unsigned uoffs ; 
			unsigned voffs ; 
			unsigned uvadder ;
			unsigned totalsize ;
			if( fourccOffsets(binary,320,240,ysize,yoffs,yadder,uvsize,uvrowdiv,uvcoldiv,uoffs,voffs,uvadder,totalsize) ){
				printf( "\tYUV format. For 320x240 surface:\n" 
					"	ySize %u\n"
					"	yOffs %u\n"
					"	yAdder %u\n"
					"	uvSize %u\n"
					"	uvRowDiv %u\n"
					"	uvColDiv %u\n"
					"	uOffs %u\n"
					"	vOffs %u\n"
					"	uvAdder %u\n"
					"	totalsize %u\n"
					, ysize, yoffs, yadder, uvsize, uvrowdiv, uvcoldiv, uoffs, voffs, uvadder, totalsize
					);
			}
			else
				printf( "RGB format\n" );
            }
        }
    }
    else
        fprintf(stderr, "Usage: %s 0xvalue or STRG\n", argv[0]);

    return 0 ;
}
#endif
