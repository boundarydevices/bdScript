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

unsigned transparency = 128 ;
unsigned x = 0 ; 
unsigned y = 0 ;
unsigned outwidth = 480 ;
unsigned outheight = 272 ;
char const *devName = "/dev/video16" ;
unsigned numFrames = 4 ; // isn't double-buffering enough?
unsigned format = V4L2_PIX_FMT_NV12 ;
bool doCopy = true ;
bool doSet = false ;

static void parseArgs( int &argc, char const **argv )
{
        for ( int arg = 1 ; arg < argc ; arg++ ) {
                if ( '-' == *argv[arg] ) {
                        char const *param = argv[arg]+1 ;
                        char const cmdchar = isalpha(*param) ? tolower(*param) : *param ;
                        if ( 'o' == cmdchar ) {
                                char const second = tolower(param[1]);
                                if ('w' == second) {
                                        outwidth = strtoul(param+2,0,0);
                                }
                                else if ('h'==second) {
                                        outheight = strtoul(param+2,0,0);
                                }
                                else
                                        printf( "unknown output option %c\n",second);
                        }
                        else if ( 'd' == cmdchar ) {
                                devName = param+1 ;
                        }
                        else if ( 'f' == cmdchar ) {
                                unsigned fcc ; 
                                if(supported_fourcc(param+1,fcc)){
                                    format = fcc ;
                                } else {
                                    fprintf(stderr, "Invalid format %s\n", param+1 );
                                    fprintf(stderr, "supported formats include:\n" );
                                    unsigned const *formats ; unsigned num_formats ;
                                    supported_fourcc_formats(formats,num_formats);
                                    while( num_formats-- ){
                                        fprintf(stderr, "\t%s\n", fourcc_str(*formats));
                                        formats++ ;
                                    }
                                    exit(1);
                                }
                        }
                        else if ( 'x' == cmdchar ) {
                                x = strtoul(param+1,0,0);
                        }
                        else if ( 'y' == cmdchar ) {
                                y = strtoul(param+1,0,0);
                        }
                        else if (('t' == cmdchar)||('a' == cmdchar)) {
                                transparency = strtoul(param+1,0,0);
                        }
                        else
                                printf( "unknown option %c-%s\n", cmdchar, param );

                        // pull from argument list
                        for ( int j = arg+1 ; j < argc ; j++ ) {
                                argv[j-1] = argv[j];
                        }
                        --arg ;
                        --argc ;
                }
        }
}

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

int main( int argc, char const **argv ) {
        parseArgs(argc,argv);

        printf( "%ux%u at %u:%u, device %s\n", outwidth, outheight,x, y, devName );
        printf( "format %s\n", fourcc_str(V4L2_PIX_FMT_NV12));
        fb2_overlay_t overlay(x,y,outwidth,outheight,transparency,V4L2_PIX_FMT_NV12);
        if ( overlay.isOpen() ) {
                printf( "overlay opened successfully: %p/%u\n", overlay.getMem(), overlay.getMemSize() );
                camera_t camera("/dev/video0",outwidth,outheight,30,V4L2_PIX_FMT_NV12);
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
                                                ++totalFrames ;
                                                ++frameCount ;
                                                if(doCopy)
                                                    memcpy(overlay.getMem(), camera_frame,(outwidth*outheight*3)/2);
                                                else if(doSet)
                                                    memset(overlay.getMem(), frameCount, outwidth*outheight);
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
                                                                printf( "%lu frames, start %llu, elapsed %llu %llu fps. %u dropped\n", frameCount, start, elapsed, (frameCount*1000)/elapsed, camera.numDropped() );
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

