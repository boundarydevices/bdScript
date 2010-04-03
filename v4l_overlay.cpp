/*
 * Module v4l_overlay.cpp
 *
 * This module defines the methods of the v4l_overlay_t
 * class as declared in v4l_overlay.h
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "v4l_overlay.h"
#include <linux/videodev2.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/mxcfb.h>
#include <sys/errno.h>
#include "fourcc.h"

#ifdef ANDROID
#define GRAPHICSDEV "/dev/graphics/fb0"
#define LOG_TAG "BoundaryCameraHal"
#else
#define GRAPHICSDEV "/dev/fb0"
#include <stdarg.h>
inline int errmsg( char const *fmt, ... )
{
   va_list ap;
   va_start( ap, fmt );
   return vfprintf( stderr, fmt, ap );
}
#define ERRMSG errmsg
#endif

#define DEBUGPRINT
#include "debugPrint.h"

#define VIDEODEV "/dev/video16"

v4l_overlay_t::v4l_overlay_t
    ( unsigned inw, unsigned inh,
      unsigned outx, unsigned outy,
      unsigned outw, unsigned outh,
      unsigned transparency,
      unsigned fourcc )
    : fd_(open(VIDEODEV,O_RDWR|O_NONBLOCK))
    , ysize_(0)
    , uvsize_(0)
    , ystride_(0)
    , uvstride_(0)
    , numQueued_(0)
    , streaming_(false)
    , next_buf_(0)
    , lastIndex_(0xff)
{
    debugPrint( "v4l_overlay %ux%u -> %u:%u %ux%u\n", inw, inh, outx, outy, outw, outh);
    memset(buffers_,0,sizeof(buffers_));
	if( 0 > fd_ ){
		ERRMSG(VIDEODEV);
		return ;
	}
	struct v4l2_format fmt ; 
	memset(&fmt,0,sizeof(fmt));
	
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	int err = ioctl(fd_, VIDIOC_G_FMT, &fmt);
	if (err < 0) {
		ERRMSG("VIDIOC_G_FMT failed\n");
        close();
        return ;
	}

	struct v4l2_pix_format &pix = fmt.fmt.pix;
	debugPrint("have default window: %ux%u (%u bytes per line)\n", pix.width, pix.height, pix.bytesperline );
	debugPrint("pixelformat 0x%08x\n", pix.pixelformat );
	debugPrint("size %u\n", pix.sizeimage );
	debugPrint("field: %d\n", pix.field );
	debugPrint("colorspace: %u\n", pix.colorspace );
	debugPrint("priv: 0x%x\n", pix.priv );

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	pix.width = inw ;
	pix.height = inh ;
	pix.bytesperline = ystride_= (inw+15)&~15 ;
    uvstride_ = ((inw/2)+15)&~15 ;
    ysize_ = inh*ystride_ ;
    uvsize_ = (inh*uvstride_)/2;
	pix.sizeimage = ysize_ + (2*uvsize_);
    pix.pixelformat = fourcc; // V4L2_PIX_FMT_SGRBG8 ; // 
    fmt.fmt.pix.priv = 0 ;

    debugPrint("ysize: %u, uvsize: %u, imgsize: %u\n", ysize_, uvsize_, pix.sizeimage );

	err = ioctl(fd_, VIDIOC_S_FMT, &fmt);
	if (err < 0) {
		ERRMSG("VIDIOC_S_FMT (%08x/%s) failed: %s\n",fourcc,fourcc_str(fourcc), strerror(errno));
        close();
        return ;
	}
	debugPrint( "set format\n" );

	int out = 3 ;
	err = ioctl(fd_, VIDIOC_S_OUTPUT, &out);
	if (err < 0) {
		ERRMSG("VIDIOC_S_OUTPUT");
        close();
        return ;
	} else
        fprintf(stderr, "-------------------------------------------------------- set to YUV overlay\n" );

	debugPrint( "set output to /dev/fb2\n" );

	struct v4l2_requestbuffers reqbuf ; memset(&reqbuf,0,sizeof(reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = NUM_BUFFERS ;

	err = ioctl(fd_, VIDIOC_REQBUFS, &reqbuf);
	if (err < 0) {
		ERRMSG("VIDIOC_REQBUFS failed\n");
        close();
        return ;
	}

	if (reqbuf.count < NUM_BUFFERS) {
		ERRMSG("VIDIOC_REQBUFS: not enough buffers\n");
        close();
        return ;
	}

	debugPrint("have %u video output buffers\n", reqbuf.count);

	for (unsigned i = 0; i < NUM_BUFFERS; i++) {
		struct v4l2_buffer buffer ; memset(&buffer,0,sizeof(buffer));
		buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		err = ioctl(fd_, VIDIOC_QUERYBUF, &buffer);
		if (err < 0) {
			ERRMSG("%s:VIDIOC_QUERYBUF: not enough buffers\n", __func__ );
			close();
            return;
		}
		debugPrint("V4L2buf phy addr: %08x, size = %d\n", (unsigned int)buffer.m.offset, buffer.length);

        buffers_[i] = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buffer.m.offset);
		if (buffers_[i] == MAP_FAILED) {
			ERRMSG("mmap failed\n");
            close();
            return ;
		}
        buffer_avail[i] = true ;
	}

	debugPrint( "mapped %u buffers\n", NUM_BUFFERS );

	struct v4l2_crop crop ; memset(&crop,0,sizeof(crop));
	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c.left = outx;
	crop.c.top = outy;
	crop.c.width = outw ;
	crop.c.height = outh ;

	err = ioctl(fd_, VIDIOC_S_CROP, &crop);
	if (err < 0) {
		ERRMSG("VIDIOC_S_CROP failed\n");
		close();
        return ;
	}

	int fd_fb = open(GRAPHICSDEV, O_RDWR, 0);
	if (fd_fb < 0) {
		ERRMSG("unable to open " GRAPHICSDEV ":%s", strerror(errno));
        close(); 
        return ;
	}

	struct mxcfb_gbl_alpha alpha;
	alpha.alpha = transparency;
	alpha.enable = 1;
	if (ioctl(fd_fb, MXCFB_SET_GBL_ALPHA, &alpha) < 0) {
		ERRMSG("set alpha blending failed\n");
        ::close(fd_fb);
        close(); 
        return ;
	}
	::close(fd_fb);

	debugPrint( "set global alpha\n" );
}

v4l_overlay_t::~v4l_overlay_t( void )
{
    if(isOpen())
        close();
}
    
void v4l_overlay_t::close( void ){
    if( 0 <= fd_ ) {
        if( streaming_ ){
        	int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        	int err = ioctl(fd_, VIDIOC_STREAMOFF, &type);
        	if (err < 0) {
        		ERRMSG("VIDIOC_STREAMOFF failed\n");
        	}
            streaming_ = false ;
        }
        ::close(fd_);
        fd_ = -1 ;
    }
}

void v4l_overlay_t::pollBufs(void)
{
    struct v4l2_buffer buffer ; memset(&buffer,0,sizeof(buffer));
    int err ;
    while( 0 == (err = ioctl(fd_, VIDIOC_DQBUF, &buffer)) ){
        if( (buffer.index < NUM_BUFFERS) && !buffer_avail[buffer.index] ){
            buffer_avail[buffer.index] = true ;
            // debugPrint( "DQBUF(%u)\n", buffer.index);
            assert(0 != numQueued_);
            --numQueued_ ;
        } else {
            ERRMSG( "DQBUF weird index %d\n", buffer.index);
        }
    }
}


bool v4l_overlay_t::getBuf
    ( unsigned       &idx, 
      unsigned char *&y, 
      unsigned char *&u, 
      unsigned char *&v )
{
    y = u = v = 0 ;
    pollBufs();
    unsigned const start = next_buf_ ;
    do {
        if( buffer_avail[next_buf_] ){
            y = (unsigned char *)buffers_[next_buf_];
            u = y+ysize_ ;
            v = u+uvsize_ ;
            idx = next_buf_ ;
            return true ;
        }
        next_buf_ = (next_buf_+1)%NUM_BUFFERS ;
    } while( next_buf_ != start );
    
    return false ;
}

void v4l_overlay_t::putBuf( unsigned idx )
{
    assert(buffer_avail[idx]);

    pollBufs();

    struct v4l2_buffer buffer ; memset(&buffer,0,sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = idx ;
    
    int err = ioctl(fd_, VIDIOC_QBUF, &buffer);
    if( err < 0) {
        ERRMSG("VIDIOC_QBUF");
        return ;
    }
    buffer_avail[idx] = false ;
    numQueued_ ++ ;
    if( !streaming_ && (1 < numQueued_) ){
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    	err = ioctl(fd_, VIDIOC_STREAMON, &type);
    	if (err < 0) {
    		ERRMSG("VIDIOC_STREAMON failed\n");
    	}
        else
            streaming_ = true ;
    }
    lastIndex_ = idx ;
}


#ifdef OVERLAY_MODULETEST

#include <ctype.h>

unsigned x = 0 ; 
unsigned y = 0 ;
unsigned inwidth = 480 ;
unsigned inheight = 272 ;
unsigned outwidth = 480 ;
unsigned outheight = 272 ;
char const *devName = "/dev/video16" ;
unsigned numFrames = 4 ; // isn't double-buffering enough?

static void parseArgs( int &argc, char const **argv )
{
	for( unsigned arg = 1 ; arg < argc ; arg++ ){
		if( '-' == *argv[arg] ){
			char const *param = argv[arg]+1 ;
            char const cmdchar = tolower(*param);
			if( 'o' == cmdchar ){
                char const second = tolower(param[1]);
                if('w' == second) {
            			outwidth = strtoul(param+2,0,0);
                } else if('h'==second) {
            			outheight = strtoul(param+2,0,0);
                }
                else
                    printf( "unknown output option %c\n",second);
			}
			else if( 'i' == cmdchar ){
                char const second = tolower(param[1]);
                if('w' == second) {
            			inwidth = strtoul(param+2,0,0);
                } else if('h'==second) {
            			inheight = strtoul(param+2,0,0);
                }
                else
                    printf( "unknown output option %c\n",second);
			}
			else if( 'd' == cmdchar ){
            			devName = param+1 ;
			}
			else if( 'x' == cmdchar ){
            			x = strtoul(param+1,0,0);
			}
			else if( 'y' == cmdchar ){
            			y = strtoul(param+1,0,0);
			}
			else
				printf( "unknown option %s\n", param );

			// pull from argument list
			for( int j = arg+1 ; j < argc ; j++ ){
				argv[j-1] = argv[j];
			}
			--arg ;
			--argc ;
		}
	}
}

static void trimCtrl(char *buf){
	char *tail = buf+strlen(buf);
	// trim trailing <CR> if needed
	while( tail > buf ){
		--tail ;
		if( iscntrl(*tail) ){
			*tail = '\0' ;
		} else
			break;
	}
}

#define V4L2_PIX_FMT_SGRBG8  v4l2_fourcc('G', 'R', 'B', 'G') /*  8  GRGR.. BGBG.. */

int main( int argc, char const **argv ) {
	parseArgs(argc,argv);

	printf( "%ux%u -> %ux%u, device %s\n", inwidth, inheight, outwidth, outheight,devName );

    v4l_overlay_t overlay(inwidth,inheight,x,y,outwidth,outheight);
    if( overlay.isOpen() ){
        printf( "opened successfully\n" );
	
    unsigned char yval=128, uval=128,vval=128 ;
    char inBuf[80];
	while( fgets(inBuf,sizeof(inBuf),stdin) ){
		trimCtrl(inBuf);
		char c = tolower(inBuf[0]);
		if( '\0' == c ){
            unsigned char *y, *u, *v ;
            unsigned index ;
            if( overlay.getBuf(index,y,u,v) ){
                memset(y,yval,overlay.ySize());
                memset(u,uval,overlay.uvSize());
                memset(v,vval,overlay.uvSize());
                overlay.putBuf(index);
            } else
                printf( "no bufs\n" );
		} else if( 'y' == c ){
			yval =strtoul(inBuf+1,0,0);
		} else if( 'u' == c ){
			uval =strtoul(inBuf+1,0,0);
		} else if( 'v' == c ){
			vval =strtoul(inBuf+1,0,0);
		}
        printf("\n");
	}
    }
    else
        ERRMSG( "Error opening v4l output\n" );
	return 0 ;
}
#endif
