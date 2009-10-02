#include <stdio.h>
#include <unistd.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <time.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "tickMs.h"
#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>
#include "pxaYUV.h"
#include <ctype.h>
// #define DEBUGPRINT
#include "debugPrint.h"

#define PWC_FPS_SHIFT		16
#define PWC_FPS_MASK		0x00FF0000
#define PWC_FPS_FRMASK		0x003F0000
#define PWC_FPS_SNAPSHOT	0x00400000

static bool volatile die = false ;
static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   die = true ;
}

static char const *pixfmt_to_str(__u32 pixfmt )
{
   static char cPixFmt[sizeof(pixfmt)+1] = { 0 };
   char const *inchars = (char const *)&pixfmt ;
   memcpy(cPixFmt, inchars, sizeof(pixfmt) );
   return cPixFmt ;
}

// print a fixed-length string (safely)
int printFixed( void const *s, unsigned size )
{
	char *const temp = (char *)alloca(size+1);
	memcpy(temp,s,size);
	temp[size] = 0 ;
	debugPrint( "%s", temp );
}

#define PRINTFIXED(__s) printFixed((__s),sizeof(__s))

#define PRINTCAP(__name)        if( cap.capabilities & __name ) \
                                        debugPrint( "\t\t" #__name  "\n" )
static char const *const stdControlTypes[] = {
	"integer"
,	"boolean"
,	"menu"
,	"button"
,	"integer64"
,	"ctrl class"
};
static unsigned const numStdControlTypes = sizeof(stdControlTypes)/sizeof(stdControlTypes[0]);

char const *controlTypeName(unsigned control){
	static char __name[80];
	if( (0 < control) && (numStdControlTypes >= control))
		return stdControlTypes[control-1];
	snprintf(__name,sizeof(__name),"type %u",control);
	return __name ;
}
#define CLEAR(__x) memset(&(__x),0,sizeof(__x))

#ifdef DEBUG
static void printBuf(v4l2_buffer const &bufinfo)
{
	printf( "\tbytesused:\t%u\n", bufinfo.bytesused );
	printf( "\tflags:\t%x\n", bufinfo.flags );
	printf( "\tfield:\t%d\n", bufinfo.field );
	printf( "\tsequence:\t%d\n", bufinfo.sequence );
	printf( "\tlength:\t%u\n", bufinfo.length );
	printf( "\tinput:\t%u\n", bufinfo.input );
}
#endif

#define PXA27X_Y_CLASS "fb_y"
#define PXA27X_U_CLASS "fb_u"
#define PXA27X_V_CLASS "fb_v"

#define FOR_RGB 0
#define FOR_PACKED_YUV444 1
#define FOR_PLANAR_YUV444 2
#define FOR_PLANAR_YUV422 3
#define FOR_PLANAR_YUV420 4

struct pxa27x_overlay_t {
	unsigned for_type;
	unsigned offset_x;	/* relative to the base plane */
	unsigned offset_y;
	unsigned width;
	unsigned height;
};

/*
 * pxa27x_yuv ioctls
 */

#define BASE_MAGIC 0xBD
#define PXA27X_YUV_SET_DIMENSIONS  _IOWR(BASE_MAGIC, 0x03, struct pxa27x_overlay_t)

struct map {
    struct v4l2_buffer buf;
    void   *addr;
    size_t len;
};

static int destx = 0 ;
static int desty = 0 ;

static void parseArgs( int &argc, char const **argv )
{
	for( unsigned arg = 1 ; arg < argc ; arg++ ){
		if( '-' == *argv[arg] ){
			char const *param = argv[arg]+1 ;
			if( 'x' == tolower(*param) ){
            			destx = strtoul(param+1,0,0);
			}
			else if( 'y' == tolower(*param) ){
            			desty = strtoul(param+1,0,0);
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
                else
                   printf( "not flag: %s\n", argv[arg] );
	}
}

int main( int argc, char const **argv )
{
   int fdCamera = open( "/dev/video0", O_RDONLY );
   if( 0 <= fdCamera )
   {
      signal( SIGINT, ctrlcHandler );

      debugPrint( "camera opened\n" );
      debugPrint( "inputs\n" );

      struct v4l2_input input;
      int index;
      
      memset (&input, 0, sizeof (input));
      
      parseArgs(argc,argv);

      while( 1 ){
         if (-1 == ioctl (fdCamera, VIDIOC_ENUMINPUT, &input)) {
                 break;
         }
         
         debugPrint ("\tinput %d: %s\n", input.index, input.name);
         input.index++ ;
      }
      
      index = 0;
      if (-1 == ioctl (fdCamera, VIDIOC_S_INPUT, &index)) {
           perror ("VIDIOC_S_INPUT");
           exit (-1);
      }

      debugPrint( "formats:\n" );

      int yuyvFormatIdx = -1 ;
      struct v4l2_fmtdesc fmt ;
      memset( &fmt, 0, sizeof(fmt) );
      fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
      while( 1 ){
         if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FMT,&fmt) ){
            debugPrint( "\tfmt:%d:%s %x(%s)\n", fmt.index, fmt.description, fmt.pixelformat, pixfmt_to_str(fmt.pixelformat));
	    if(V4L2_PIX_FMT_YUYV == fmt.pixelformat)
		    yuyvFormatIdx = fmt.index ;
            fmt.index++ ;
         }
         else {
//            fprintf( stderr, "VIDIOC_ENUM_FMT:%d\n", fmt.index );
            break;
         }
      }
      struct v4l2_capability cap ;
      if( 0 == ioctl(fdCamera, VIDIOC_QUERYCAP, &cap ) ){
	      debugPrint( "have caps:\n" );
	      debugPrint( "\tdriver:\t" ); PRINTFIXED(cap.driver); debugPrint( "\n" );
	      debugPrint( "\tcard:\t" ); PRINTFIXED(cap.card); debugPrint( "\n" );
	      debugPrint( "\tbus:\t" ); PRINTFIXED(cap.bus_info); debugPrint( "\n" );
	      debugPrint( "\tversion %u (0x%x)\n", cap.version, cap.version );
	      debugPrint( "\tcapabilities: 0x%x\n", cap.capabilities );
              PRINTCAP(V4L2_CAP_VIDEO_CAPTURE);
              PRINTCAP(V4L2_CAP_VIDEO_OUTPUT);
              PRINTCAP(V4L2_CAP_VIDEO_OVERLAY);
              PRINTCAP(V4L2_CAP_VBI_CAPTURE);
              PRINTCAP(V4L2_CAP_VBI_OUTPUT);
              PRINTCAP(V4L2_CAP_SLICED_VBI_CAPTURE);
              PRINTCAP(V4L2_CAP_SLICED_VBI_OUTPUT);
              PRINTCAP(V4L2_CAP_RDS_CAPTURE);
              PRINTCAP(V4L2_CAP_VIDEO_OUTPUT_OVERLAY);
              PRINTCAP(V4L2_CAP_VIDEO_OVERLAY);
              PRINTCAP(V4L2_CAP_TUNER);
              PRINTCAP(V4L2_CAP_AUDIO);
              PRINTCAP(V4L2_CAP_RADIO);
              PRINTCAP(V4L2_CAP_READWRITE);
              PRINTCAP(V4L2_CAP_ASYNCIO);
              PRINTCAP(V4L2_CAP_STREAMING);
      }
      else
	      perror("VIDIOC_QUERYCAP" );

      if( cap.capabilities & V4L2_CAP_TUNER ){
	      v4l2_std_id standard ;
	      if( 0 == ioctl(fdCamera, VIDIOC_QUERYSTD, &standard) ){
		      printf( "standard: %08llx\n", standard );
	      }
	      else
		      perror( "VIDIOC_QUERYSTD" );
      }
      debugPrint( "controls:\n" );
      for( unsigned i = V4L2_CID_BASE ; i < V4L2_CID_LASTP1 ; i++ ){
	      v4l2_queryctrl control ;
	      memset(&control,0,sizeof(control));
	      control.id = i ;
	      if( 0 == ioctl(fdCamera,VIDIOC_QUERYCTRL,&control) ){
		      debugPrint( "\tcontrol %x:\t", i ); PRINTFIXED(control.name); debugPrint( ", type %s\n", controlTypeName(control.type) );
	      }
      }
	struct v4l2_queryctrl qctrl;
	memset(&qctrl,0,sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (0 == ioctl (fdCamera, VIDIOC_QUERYCTRL, &qctrl)) {
		debugPrint( "extended control %x:\t", qctrl.id ); 
			PRINTFIXED(qctrl.name); 
			debugPrint( ", type %s\n", controlTypeName(qctrl.type) );
	/* ... */
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}

        struct v4l2_cropcap cropcap ;
	memset(&cropcap,0,sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	if( 0 == ioctl(fdCamera,VIDIOC_CROPCAP,&cropcap)){
		debugPrint( "have crop capabilities\n" );
		debugPrint( "\tbounds %d:%d %dx%d\n", cropcap.bounds.left,cropcap.bounds.top,cropcap.bounds.width,cropcap.bounds.height );
		debugPrint( "\tdefrect %d:%d %dx%d\n", cropcap.defrect.left,cropcap.defrect.top,cropcap.defrect.width,cropcap.defrect.height );
	}

        v4l2_streamparm streamp ;
	memset(&streamp,0,sizeof(streamp));
	streamp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
        if( 0 == ioctl(fdCamera, VIDIOC_G_PARM, &streamp)){
		debugPrint( "have stream params\n" );
		debugPrint( "\tparm.capture:\n" );
		debugPrint( "\tparm.capture.capability\t0x%08x\n", streamp.parm.capture.capability );
		debugPrint( "\tparm.capture.capturemode\t0x%08x\n", streamp.parm.capture.capturemode );
		debugPrint( "\tparm.capture.extendedmode\t0x%08x\n", streamp.parm.capture.extendedmode );
		debugPrint( "\tparm.capture.readbuffers\t%u\n", streamp.parm.capture.readbuffers );
		debugPrint( "\tparm.capture.timeperframe\t%u/%u\n"
				, streamp.parm.capture.timeperframe.numerator
				, streamp.parm.capture.timeperframe.denominator );
		streamp.parm.capture.timeperframe.numerator = streamp.parm.capture.timeperframe.denominator = 1 ;
		if( 0 == ioctl(fdCamera, VIDIOC_S_PARM, &streamp)){
			debugPrint( "set stream parameters\n" );
		} else
			fprintf( stderr, "VIDIOC_S_PARAM:%m\n" );
	}
	else
		fprintf(stderr,"VIDIOC_G_PARM");
        
	unsigned yuvWidth = 0, yuvHeight = 0 ;
	if(0 <= yuyvFormatIdx){
		struct video_capability vidcap ; 
		if( 0 > ioctl( fdCamera, VIDIOCGCAP, &vidcap)){
			perror( "VIDIOCGCAP");
		}
		debugPrint( "%ux%u..%ux%u\n", vidcap.minwidth, vidcap.minheight, vidcap.maxwidth, vidcap.maxheight );
		yuvWidth = vidcap.maxwidth ;
		yuvHeight = vidcap.maxheight ;
                v4l2_format format ;
		CLEAR(format);
		format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
		format.fmt.pix.field = V4L2_FIELD_NONE ;
		format.fmt.pix.width = vidcap.maxwidth ;
		format.fmt.pix.height = vidcap.maxheight ;
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV ;
                format.fmt.pix.bytesperline = format.fmt.pix.width*2 ;
		format.fmt.pix.sizeimage = format.fmt.pix.height*format.fmt.pix.bytesperline ;
		format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB ;

		if(0 == ioctl(fdCamera,VIDIOC_S_FMT,&format)){
			debugPrint( "set YUYV capture\n" );
		}
		else
			perror("VIDIOC_S_FMT");
	
		if(0 == ioctl(fdCamera,VIDIOC_G_FMT,&format)){
			debugPrint( "YUYV capture data:\n" );
			debugPrint( "\t%ux%u.. %ubpl... %ubpp\n", format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.bytesperline, format.fmt.pix.sizeimage );
		}
		else
			perror("VIDIOC_S_FMT");
	} // supports YUYV

	struct v4l2_frmsizeenum frameSize ;
	CLEAR(frameSize);
	frameSize.pixel_format = V4L2_PIX_FMT_YVU420 ;
	while(1){
		if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMESIZES,&frameSize) ){
			debugPrint( "have frame size %d\n", frameSize.index );
			frameSize.index++ ;
		}
		else {
			debugPrint( "No more frame sizes: %m\n" );
			break;
		}
	}

        struct v4l2_frmivalenum frameIval ;
	CLEAR(frameIval);
	frameIval.pixel_format = V4L2_PIX_FMT_YVU420 ;
	while(1){
		if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMESIZES,&frameSize) ){
			debugPrint( "have frame ival %d\n", frameSize.index );
			frameSize.index++ ;
		}
		else {
			debugPrint( "No more frame ivals: %m\n" );
			break;
		}
	}

	unsigned const numBuffers = 8 ;
	v4l2_requestbuffers rb ;
	memset(&rb,0,sizeof(rb));
	rb.count = numBuffers ;
	rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	rb.memory = V4L2_MEMORY_MMAP ;
	if( 0 == ioctl(fdCamera, VIDIOC_REQBUFS, &rb) ){
#ifdef DEBUG
		printf( "have %u bufs (requested %u)\n", rb.count, numBuffers );
#endif 
	}
	else
		perror( "VIDIOC_REQBUFS" );

	struct map * const bufferMaps = new struct map [numBuffers];

	for (unsigned i = 0; i < numBuffers; ++i ) {
		struct map &m = bufferMaps[i];
		CLEAR(bufferMaps[i]);
                m.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                m.buf.memory = V4L2_MEMORY_MMAP;
                m.buf.index = i;
		if (0 >ioctl(fdCamera, VIDIOC_QUERYBUF, bufferMaps+i)) {
			fprintf( stderr, "VIDIOC_QUERYBUF(%u):%m\n", i );
			return -1 ;
		}
#ifdef DEBUG
		printBuf(m.buf);
#endif
                m.len = m.buf.length ;
                m.addr = mmap(NULL,m.len, PROT_READ, MAP_SHARED, fdCamera, m.buf.m.offset);
		if( (0==m.addr) || (MAP_FAILED==m.addr) ){
			fprintf(stderr, "MMAP(buffer %u):%u bytes:%m\n", i, m.len );
			return -1 ;
		}
                if (-1 == ioctl(fdCamera, VIDIOC_QBUF, bufferMaps+i)){
			perror("VIDIOC_QBUF");
			return -1 ;
		}
	}

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == ioctl(fdCamera, VIDIOC_STREAMON, &type)){
		perror("VIDIOC_STREAMON");
		return -1 ;
	}
	debugPrint( "queued %u buffers\n", numBuffers );

	if( (0 == yuvWidth) || (0 == yuvHeight) ){
		printf( "No YUV support, bailing\n" );
		return 0 ;
	}
	char const * const yuvDevs[] = {
		"/dev/fb_y"
	,	"/dev/fb_u"
	,	"/dev/fb_v"
	};
        
	pxaYUV_t *yuv = new pxaYUV_t( destx, desty, yuvWidth, yuvHeight );

	printf( "%ux%u @%u:%u\n", yuvWidth, yuvHeight, destx, desty );
	struct pollfd fds[2];
	fds[0].fd = fdCamera ;
	fds[0].events = POLLIN ;
	fds[1].fd = fileno(stdin);
	fds[1].events = POLLIN ;
	fcntl(fileno(stdin), F_SETFL, fcntl(fileno(stdin), F_GETFL) | O_NONBLOCK );
	fcntl(fdCamera, F_SETFL, fcntl(fdCamera, F_GETFL) | O_NONBLOCK );

	unsigned iterations = 0 ;
	while(!die){
		int numready = poll(fds,sizeof(fds)/sizeof(fds[0]),1000);
		if( 0 < numready ){
			if( fds[0].revents & POLLIN ){
				struct v4l2_buffer buf ;
				CLEAR(buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				if (-1 == ioctl(fdCamera, VIDIOC_DQBUF, &buf)) {
					switch (errno) {
						case EAGAIN:
							printf( "wait...\n" );
							break ;
						case EIO:
						/* Could ignore EIO, see spec. */
						/* fall through */
		
						default:
							perror("VIDIOC_DQBUF");
							return -1 ;
					}
				} else {
					if( yuv && yuv->isOpen() ){
						yuv->writeInterleaved( (unsigned char const *)bufferMaps[buf.index].addr );
					}
					if(0 > ioctl(fdCamera, VIDIOC_QBUF, &buf)){
						perror("VIDIOC_QBUF");
						return -1 ;
					}
				}
			} // camera ready
			if( fds[1].revents & POLLIN ){
				char inBuf[80];
				if( fgets(inBuf,sizeof(inBuf),stdin) ){
					unsigned len = strlen(inBuf);
					while( 0 < len-- ){
						if( iscntrl(inBuf[len]) )
							inBuf[len] = '\0' ;
						else
							break;
					}
					if( 'q' == tolower(inBuf[0]) ){
						break;
					}
				}
			} // stdin ready
		}
	}
   }
   else
      perror( "/dev/video0" );

   return 0 ;
}
