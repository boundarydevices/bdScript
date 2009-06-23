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
	printf( "%s", temp );
}

#define PRINTFIXED(__s) printFixed((__s),sizeof(__s))

#define PRINTCAP(__name)        if( cap.capabilities & __name ) \
                                        printf( "\t\t" #__name  "\n" )
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

static void printBuf(v4l2_buffer const &bufinfo)
{
	printf( "have bufinfo\n" );
	printf( "\tbytesused:\t%u\n", bufinfo.bytesused );
	printf( "\tflags:\t%x\n", bufinfo.flags );
	printf( "\tfield:\t%d\n", bufinfo.field );
	printf( "\tsequence:\t%d\n", bufinfo.sequence );
	printf( "\tlength:\t%u\n", bufinfo.length );
	printf( "\tinput:\t%u\n", bufinfo.input );
}

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

int main( int argc, char const * const argv[] )
{
   int fdCamera = open( "/dev/video0", O_RDONLY );
   if( 0 <= fdCamera )
   {
      signal( SIGINT, ctrlcHandler );

      printf( "camera opened\n" );
      printf( "inputs\n" );

      struct v4l2_input input;
      int index;
      
      memset (&input, 0, sizeof (input));
      
      while( 1 ){
         if (-1 == ioctl (fdCamera, VIDIOC_ENUMINPUT, &input)) {
                 break;
         }
         
         printf ("\tinput %d: %s\n", input.index, input.name);
         input.index++ ;
      }
      
      index = 0;
      if (-1 == ioctl (fdCamera, VIDIOC_S_INPUT, &index)) {
           perror ("VIDIOC_S_INPUT");
           exit (-1);
      }

      printf( "formats:\n" );

      int yuyvFormatIdx = -1 ;
      struct v4l2_fmtdesc fmt ;
      memset( &fmt, 0, sizeof(fmt) );
      fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
      while( 1 ){
         if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FMT,&fmt) ){
            fprintf( stderr, "\tfmt:%d:%s %x(%s)\n", fmt.index, fmt.description, fmt.pixelformat, pixfmt_to_str(fmt.pixelformat));
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
	      printf( "have caps:\n" );
	      printf( "\tdriver:\t" ); PRINTFIXED(cap.driver); printf( "\n" );
	      printf( "\tcard:\t" ); PRINTFIXED(cap.card); printf( "\n" );
	      printf( "\tbus:\t" ); PRINTFIXED(cap.bus_info); printf( "\n" );
	      printf( "\tversion %u (0x%x)\n", cap.version, cap.version );
	      printf( "\tcapabilities: 0x%x\n", cap.capabilities );
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
      printf( "controls:\n" );
      for( unsigned i = V4L2_CID_BASE ; i < V4L2_CID_LASTP1 ; i++ ){
	      v4l2_queryctrl control ;
	      memset(&control,0,sizeof(control));
	      control.id = i ;
	      if( 0 == ioctl(fdCamera,VIDIOC_QUERYCTRL,&control) ){
		      printf( "\tcontrol %x:\t", i ); PRINTFIXED(control.name); printf( ", type %s\n", controlTypeName(control.type) );
	      }
      }
/* Not supported on laptop/Ubuntu
      struct v4l2_ext_controls ext_controls ;
      memset(&ext_controls,0,sizeof(ext_controls));
      ext_controls.ctrl_class = V4L2_CTRL_CLASS_CAMERA ;
      if( 0 == ioctl(fdCamera,VIDIOC_G_EXT_CTRLS,&ext_controls))
	      printf( "have extended camera controls\n" );
      else
	      printf( "no extended camera controls\n" );
*/
	struct v4l2_queryctrl qctrl;
	memset(&qctrl,0,sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (0 == ioctl (fdCamera, VIDIOC_QUERYCTRL, &qctrl)) {
		printf( "extended control %x:\t", qctrl.id ); 
			PRINTFIXED(qctrl.name); 
			printf( ", type %s\n", controlTypeName(qctrl.type) );
	/* ... */
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}

        struct v4l2_cropcap cropcap ;
	memset(&cropcap,0,sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	if( 0 == ioctl(fdCamera,VIDIOC_CROPCAP,&cropcap)){
		printf( "have crop capabilities\n" );
		printf( "\tbounds %d:%d %dx%d\n", cropcap.bounds.left,cropcap.bounds.top,cropcap.bounds.width,cropcap.bounds.height );
		printf( "\tdefrect %d:%d %dx%d\n", cropcap.defrect.left,cropcap.defrect.top,cropcap.defrect.width,cropcap.defrect.height );
	}

        v4l2_streamparm streamp ;
	memset(&streamp,0,sizeof(streamp));
	streamp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
        if( 0 == ioctl(fdCamera, VIDIOC_G_PARM, &streamp)){
		printf( "have stream params\n" );
		printf( "\tparm.capture:\n" );
		printf( "\tparm.capture.capability\t0x%08x\n", streamp.parm.capture.capability );
		printf( "\tparm.capture.capturemode\t0x%08x\n", streamp.parm.capture.capturemode );
		printf( "\tparm.capture.extendedmode\t0x%08x\n", streamp.parm.capture.extendedmode );
		printf( "\tparm.capture.readbuffers\t%u\n", streamp.parm.capture.readbuffers );
		printf( "\tparm.capture.timeperframe\t%u/%u\n"
				, streamp.parm.capture.timeperframe.numerator
				, streamp.parm.capture.timeperframe.denominator );
		streamp.parm.capture.timeperframe.numerator = streamp.parm.capture.timeperframe.denominator = 1 ;
		if( 0 == ioctl(fdCamera, VIDIOC_S_PARM, &streamp)){
			printf( "set stream parameters\n" );
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
		printf( "%ux%u..%ux%u\n", vidcap.minwidth, vidcap.minheight, vidcap.maxwidth, vidcap.maxheight );
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
			printf( "set YUYV capture\n" );
		}
		else
			perror("VIDIOC_S_FMT");
	
		if(0 == ioctl(fdCamera,VIDIOC_G_FMT,&format)){
			printf( "YUYV capture data:\n" );
			printf( "\t%ux%u.. %ubpl... %ubpp\n", format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.bytesperline, format.fmt.pix.sizeimage );
		}
		else
			perror("VIDIOC_S_FMT");
	} // supports YUYV

	struct v4l2_frmsizeenum frameSize ;
	CLEAR(frameSize);
	frameSize.pixel_format = V4L2_PIX_FMT_YVU420 ;
	while(1){
		if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMESIZES,&frameSize) ){
			printf( "have frame size %d\n", frameSize.index );
			frameSize.index++ ;
		}
		else {
			printf( "No more frame sizes: %m\n" );
			break;
		}
	}

        struct v4l2_frmivalenum frameIval ;
	CLEAR(frameIval);
	frameIval.pixel_format = V4L2_PIX_FMT_YVU420 ;
	while(1){
		if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMESIZES,&frameSize) ){
			printf( "have frame ival %d\n", frameSize.index );
			frameSize.index++ ;
		}
		else {
			printf( "No more frame ivals: %m\n" );
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
		printf( "have %u bufs (requested %u)\n", rb.count, numBuffers );
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
		printBuf(m.buf);
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
	printf( "queued %u buffers\n", numBuffers );

	if( (0 == yuvWidth) || (0 == yuvHeight) ){
		printf( "No YUV support, bailing\n" );
		return 0 ;
	}
	char const * const yuvDevs[] = {
		"/dev/fb_y"
	,	"/dev/fb_u"
	,	"/dev/fb_v"
	};
	int fdYUV[3] = {0};

	struct pxa27x_overlay_t overlay ;
	overlay.for_type = FOR_PLANAR_YUV420 ;
	overlay.offset_x = 0 ;	/* relative to the base plane */
	overlay.offset_y = 0 ;
	overlay.width = yuvWidth ;
	overlay.height = yuvHeight ;

	for( int i = 0 ; i < 3 ; i++ ){
		fdYUV[i] = open( yuvDevs[i], O_RDWR );
		if( 0 > fdYUV[i] ){
			perror( yuvDevs[i] );
			return -1 ;
		}
	}
	printf( "opened yuv devs\n" );
	if( 0 != ioctl(fdYUV[0],PXA27X_YUV_SET_DIMENSIONS,&overlay)){
		perror( "PXA27X_YUV_SET_DIMENSIONS" );
		return -1 ;
	}
	printf( "set output dimensions to %ux%u (expected %ux%u)\n", overlay.width, overlay.height, yuvWidth, yuvHeight );

	unsigned yBytesOut = overlay.width*overlay.height ;
	unsigned uvBytesOut = yBytesOut>>2 ;
	unsigned const mapBytes[] = {
		yBytesOut
	,	uvBytesOut
	,	uvBytesOut
	};
	void *outMaps[3] = { 0 };
	for( unsigned i = 0 ; i < 3 ; i++ ){
		unsigned length = mapBytes[i];
		outMaps[i] = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fdYUV[i], 0);
		if( !outMaps[i] || (MAP_FAILED == outMaps[i]) ){
			perror( "mmap" );
			return -1 ;
		}
	}
	printf( "mapped %u/%u/%u bytes of display memory\n", mapBytes[0],mapBytes[1],mapBytes[2]);

	char inBuf[512];
	fgets(inBuf,sizeof(inBuf),stdin);
//	return 0 ;
	unsigned iterations = 0 ;
	while(1){
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
			printBuf(buf);
			if(0 > ioctl(fdCamera, VIDIOC_QBUF, &buf)){
				perror("VIDIOC_QBUF");
				return -1 ;
			}
		}
		if( 10 < ++iterations )
			break;
	}
   }
   else
      perror( "/dev/video0" );

   return 0 ;
}
