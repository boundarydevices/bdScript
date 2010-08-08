/*
 * Progra v4l_camera.cpp
 *
 * This program is a test combination of the camera_t
 * class and the v4l_overlay_t class.
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "fb2_overlay.h"
#include "camera.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "fourcc.h"

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

bool doCopy = true ;
bool doSet = false ;

class stringSplit_t {
public:
        enum {
                MAXPARTS = 16 
        };

        stringSplit_t( char *line );

        unsigned getCount(void){ return count_ ;}
        char const *getPtr(unsigned idx){ return ptrs_ [idx];}

private:
        stringSplit_t(stringSplit_t const &);

        unsigned count_ ;
        char    *ptrs_[MAXPARTS];
};

stringSplit_t::stringSplit_t( char *line )
: count_( 0 )
{
        while ( (count_ < MAXPARTS) && (0 != *line) ) {
                ptrs_[count_++] = line++ ;
                while ( isgraph(*line) )
                        line++ ;
                if ( *line ) {
                        *line++ = 0 ;
                        while ( isspace(*line) )
                                line++ ;
                }
                else
                        break;
        }
}

static bool getFraction(char const *cFrac, unsigned max, unsigned &offs ){
        unsigned numerator ;
        if ( isdigit(*cFrac) ) {
                numerator = 0 ;
                while (isdigit(*cFrac)) {
                        numerator *= 10 ;
                        numerator += (*cFrac-'0');
                        cFrac++ ;
                }
        }
        else
                numerator = 1 ;

        if ( '/' == *cFrac ) {
                cFrac++ ;
                unsigned denominator = 0 ;
                while ( isdigit(*cFrac) ) {
                        denominator *= 10 ;
                        denominator += (*cFrac-'0');
                        cFrac++ ;
                }
                if ( denominator && (numerator <= denominator)) {
                        offs = (max*numerator)/denominator ;
                        return true ;
                }
        }
        else if ( '\0' == *cFrac ) {
                offs = numerator ;
                return true ;
        }

        return false ;
}

static void trimCtrl(char *buf){
        char *tail = buf+strlen(buf);
        // trim trailing <CR> if needed
        while ( tail > buf ) {
                --tail ;
                if ( iscntrl(*tail) ) {
                        *tail = '\0' ;
                }
                else
                        break;
        }
}

#include <linux/fb.h>
#include <sys/ioctl.h>

static void process_command(char *cmd,int fd,void *fbmem,unsigned fbmemsize)
{
        trimCtrl(cmd);
        stringSplit_t split(cmd);
        printf( "%s: %u parts\n", cmd, split.getCount() );
        if ( 0 < split.getCount() ) {
                switch (tolower(split.getPtr(0)[0])) {
                        case 'c': {
                            doCopy = !doCopy ;
                            printf( "%scopying frames to overlay\n", doCopy ? "" : "not " );
                            break;
                        }
                        case 's': {
                            doSet = !doSet ;
                            printf( "%ssetting frames to overlay\n", doSet ? "" : "not " );
                            break;
                        }
                        case 'f': {
                                        struct fb_var_screeninfo variable_info;
                                        int err = ioctl( fd, FBIOGET_VSCREENINFO, &variable_info );
                                        if ( 0 == err ) {
                                                variable_info.yoffset = (0 != variable_info.yoffset) ? 0 : variable_info.yres ;
                                                err = ioctl( fd, FBIOPAN_DISPLAY, &variable_info );
                                                if ( 0 == err ) {
                                                        printf( "flipped to offset %d\n", variable_info.yoffset );
                                                }
                                                else
                                                        perror( "FBIOPAN_DISPLAY" );
                                        }
                                        else
                                                perror( "FBIOGET_VSCREENINFO");
                                        break;
                                }
                        case 'y': {
                                        if ( 1 < split.getCount()) {
                                                unsigned yval = strtoul(split.getPtr(1),0,0);
                                                unsigned start = 0 ;
                                                unsigned end = fbmemsize ;
                                                if ( 2 < split.getCount() ) {
                                                        if ( !getFraction(split.getPtr(2),fbmemsize,start) ) {
                                                                fprintf(stderr, "Invalid fraction %s\n", split.getPtr(2));
                                                                break;
                                                        }
                                                        if ( 3 < split.getCount() ) {
                                                                if ( !getFraction(split.getPtr(3),fbmemsize,end) ) {
                                                                        fprintf(stderr, "Invalid fraction %s\n", split.getPtr(3));
                                                                        break;
                                                                }
                                                        }
                                                }
                                                if ( (end > start) && (end <= fbmemsize) ) {
                                                        printf( "set y buffer [%u..%u] out of %u to %u (0x%x) here\n", start, end, fbmemsize, yval, yval );
                                                        memset(((char *)fbmem)+start,yval,end-start);
                                                }
                                        }
                                        else
                                                fprintf(stderr, "Usage: y yval [start [end]]\n" );
                                        break;
                                }
                        case '?': {
                                        printf( "available commands:\n"
                                                "\tf	- flip buffers\n" 
                                                "\tc	- toggle copy\n" 
                                                "\ty yval [start [end]] - set y buffer(s) to specified value\n" 
                                                "\n"
                                                "most start and end positions can be specified in fractions.\n" 
                                                "	/2 or 1/2 is halfway into buffer or memory\n" 
                                                "	/4 or 1/4 is a quarter of the way into buffer or memory\n" 
                                              );
                                }
                }
        }
}

#include <sys/poll.h>
#include "tickMs.h"
#include "cameraParams.h"

static void phys_to_fb2
	( void const     *cameraMem,
	  fb2_overlay_t  &overlay,
	  cameraParams_t &params )
{
	if (V4L2_PIX_FMT_YUYV == params.getCameraFourcc()) {
		unsigned camera_bpl = params.getCameraWidth() * 2 ;
		unsigned char const *cameraIn = (unsigned char *)cameraMem ;
		unsigned char *fbOut = (unsigned char *)overlay.getMem();
		unsigned fb_bpl = 2*params.getPreviewWidth();
		unsigned maxWidth = params.getPreviewWidth()>params.getCameraWidth() ? params.getCameraWidth() : params.getPreviewWidth();
		unsigned hskip = ((2*params.getPreviewWidth()) <= params.getCameraWidth()) ? params.getCameraWidth()/params.getPreviewWidth() : 0 ;
		unsigned vskip = (params.getPreviewHeight() < params.getCameraHeight())
				? (params.getCameraHeight()+params.getPreviewHeight()-1) / params.getPreviewHeight() 
				: 1 ;
		for( unsigned y = 0 ; y < params.getCameraHeight(); y += vskip ){
			if((y/vskip) >= params.getPreviewHeight())
				break;
			if(hskip) {
				for( unsigned mpix = 0 ; mpix*2 < params.getCameraWidth(); mpix++ ){
					unsigned inoffs = mpix*4*hskip ;
					unsigned outoffs = mpix*4 ;
					memcpy(fbOut+outoffs,cameraIn+inoffs,4); // one macropix
				}
			} else
				memcpy(fbOut,cameraIn,2*maxWidth);
	
			cameraIn += vskip*camera_bpl ;
			fbOut += fb_bpl ;
		}
	} else
		printf ("unsupported video format %s\n", fourcc_str(params.getCameraFourcc()));
}

int main( int argc, char const **argv ) {
        
	cameraParams_t params(argc,argv);
	params.dump();

        printf( "format %s\n", fourcc_str(params.getCameraFourcc()));
        fb2_overlay_t overlay(params.getPreviewX(),
			      params.getPreviewY(),
			      params.getPreviewWidth(),
			      params.getPreviewHeight(),
			      params.getPreviewTransparency(),
			      params.getCameraFourcc());
        if ( overlay.isOpen() ) {
                printf( "overlay opened successfully: %p/%u\n", overlay.getMem(), overlay.getMemSize() );
                camera_t camera("/dev/video0",params.getCameraWidth(),
				params.getCameraHeight(),params.getCameraFPS(),
				params.getCameraFourcc());
                if (camera.isOpen()) {
                        printf( "camera opened successfully\n");
                        if ( camera.startCapture() ) {
                                printf( "camera streaming started successfully\n");
                                printf( "cameraSize %u, overlaySize %u\n", camera.imgSize(), overlay.getMemSize() );
                                unsigned long frameCount = 0 ;
                                unsigned totalFrames = 0 ;
                                unsigned outDrops = 0 ;
                                long long start = tickMs();
                                while (1) {
                                        void const *camera_frame ;
                                        int index ;
                                        if ( camera.grabFrame(camera_frame,index) ) {
						if ( (int)totalFrames == params.getSaveFrameNumber() ) {
							printf( "saving %u bytes of img %u to /tmp/camera.out\n", camera.imgSize(), index );
							FILE *fOut = fopen( "/tmp/camera.out", "wb" );
							if( fOut ) {
								fwrite(camera_frame,1,camera.imgSize(),fOut);
								fclose(fOut);
								printf("done\n");
							} else
								perror("/tmp/camera.out" );
						}
                                                ++totalFrames ;
                                                ++frameCount ;
						phys_to_fb2(camera_frame,overlay,params);
                                                camera.returnFrame(camera_frame,index);
                                        }
                                        if ( isatty(0) ) {
                                                struct pollfd fds[1];
                                                fds[0].fd = fileno(stdin); // STDIN
                                                fds[0].events = POLLIN|POLLERR;
                                                int numReady = poll(fds,1,0);
                                                if ( 0 < numReady ) {
                                                        char inBuf[512];
                                                        if ( fgets(inBuf,sizeof(inBuf),stdin) ) {
                                                                trimCtrl(inBuf);
                                                                printf( "You entered <%s>\n", inBuf);
                                                                process_command(inBuf, overlay.getFd(), overlay.getMem(), overlay.getMemSize());
                                                                long long elapsed = tickMs()-start;
                                                                if ( 0LL == elapsed )
                                                                        elapsed = 1 ;
								unsigned whole_fps = (frameCount*1000)/elapsed ;
								unsigned frac_fps = ((frameCount*1000000)/elapsed)%1000 ;
                                                                printf( "%lu frames, start %llu, elapsed %llu %u.%03u fps. %u dropped\n", 
									frameCount, start, elapsed, whole_fps, frac_fps, camera.numDropped() );
                                                                frameCount = 0 ; start = tickMs();
                                                        }
                                                        else {
                                                                printf( "[eof]\n");
                                                                break;
                                                        }
                                                }
                                        }
                                }
                        }
                        else
                                fprintf(stderr, "Error starting capture\n" );
                }
                else
                        fprintf(stderr, "Error opening camera\n" );
        }
        else
                fprintf(stderr, "Error opening v4l output\n" );
        return 0 ;
}

