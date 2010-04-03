/*
 * Progra v4l_camera.cpp
 *
 * This program is a test combination of the camera_t
 * class and the v4l_overlay_t class.
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "v4l_overlay.h"
#include "camera.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <ctype.h>

unsigned transparency = 0 ;
unsigned x = 0 ; 
unsigned y = 0 ;
unsigned inwidth = 480 ;
unsigned inheight = 272 ;
unsigned outwidth = 480 ;
unsigned outheight = 272 ;
char const *devName = "/dev/video16" ;
unsigned numFrames = 4 ; // isn't double-buffering enough?
camera_t::rotation_e rotation = camera_t::ROTATE_NONE ;

static void parseArgs( int &argc, char const **argv )
{
	for( int arg = 1 ; arg < argc ; arg++ ){
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
			else if( 't' == cmdchar ){
				transparency = strtoul(param+1,0,0);
			}
			else if( 'r' == cmdchar ){
				switch (tolower(param[1])) {
					case 'v': rotation = camera_t::FLIP_VERTICAL ; break ;
					case 'h': rotation = camera_t::FLIP_HORIZONTAL ; break ;
					case 'b': rotation = camera_t::FLIP_BOTH ; break ;
					case 'l': rotation = camera_t::ROTATE_90_LEFT ; break ;
					case 'r': rotation = camera_t::ROTATE_90_RIGHT ; break ;
					case 'n': rotation = camera_t::ROTATE_NONE ; break ;
					default: {
							printf("Invalid rotation character %c\n"
							       "Valid options are:\n"
							       "    v   - flip vertical\n"
							       "    h   - flip horizontal\n"
							       "    b   - flip both\n"
							       "    l   - rotate 90 degrees left (counter-clockwise)\n"
							       "    r   - rotate 90 degrees right (clockwise)\n"
							       , param[1]);
						}
				}
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

#include <sys/poll.h>
#include "tickMs.h"

int main( int argc, char const **argv ) {
	parseArgs(argc,argv);

	printf( "%ux%u -> %ux%u, device %s\n", inwidth, inheight, outwidth, outheight,devName );

    v4l_overlay_t overlay(inwidth,inheight,x,y,outwidth,outheight,transparency,V4L2_PIX_FMT_NV12);
    if( overlay.isOpen() ){
        printf( "overlay opened successfully\n" );
        printf( "strides: y:%u, uv:%u\n", overlay.yStride(), overlay.uvStride() );
        camera_t camera("/dev/video0",inwidth,inheight,30,V4L2_PIX_FMT_NV12,rotation);
        if(camera.isOpen()) {
            printf( "camera opened successfully\n");
            if( camera.startCapture() ){
                printf( "camera streaming started successfully\n");
                printf( "cameraSize %u, overlaySize %u\n", camera.imgSize(), overlay.imgSize() );
                unsigned long frameCount = 0 ;
                long long start = tickMs();
                while(1) {
                    void const *camera_frame ;
                    int index ;
                    if( camera.grabFrame(camera_frame,index) ){
                        unsigned ovIndex ;
                        unsigned char *y, *u, *v ;
                        if( overlay.getBuf(ovIndex,y,u,v) ){
                            memcpy(y,camera_frame,overlay.imgSize());
                            overlay.putBuf(ovIndex);
                            ++frameCount ;
                        } else
                            printf( "output frame drop\n" );
                        camera.returnFrame(camera_frame,index);
                    }
                    struct pollfd fds[1];
                    fds[0].fd = fileno(stdin); // STDIN
                    fds[0].events = POLLIN|POLLERR;
                    int numReady = poll(fds,1,0);
                    if( 0 < numReady ){
                        char inBuf[512];
                        if( fgets(inBuf,sizeof(inBuf),stdin) ){
                            trimCtrl(inBuf);
                            printf( "You entered <%s>\n", inBuf);
                            long long elapsed = tickMs()-start;
                            if( 0LL == elapsed )
                                elapsed = 1 ;
                            printf( "%lu frames, start %llu, elapsed %llu %llu fps\n", frameCount, start, elapsed, (frameCount*1000)/elapsed );
                            frameCount = 0 ; start = tickMs();
                        } else {
                            printf( "[eof]\n");
                            break;
                        }
                    }
                }
                long long elapsed = tickMs()-start;
                if( 0LL == elapsed )
                    elapsed = 1 ;
	            printf( "%lu frames, start %llu, elapsed %llu %llu fps\n", frameCount, start, elapsed, (frameCount*1000)/elapsed );
                    if( !camera.stopCapture() ){
                       fprintf(stderr, "Error stopping capture\n" );
                } else {
                }
            } else
                fprintf(stderr, "Error starting capture\n" );
        } else
            fprintf(stderr, "Error opening camera\n" );
    }
    else
        fprintf(stderr, "Error opening v4l output\n" );
	return 0 ;
}

