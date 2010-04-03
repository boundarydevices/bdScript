/*
 * Module camera.cpp
 *
 * This module defines the methods of the camera_t class
 * as declared in camera.h
 *
 *
 * Change History : 
 *
 * $Log$
 *
 * Copyright Boundary Devices, Inc. 2010
 */


#include "camera.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <stdint.h>
#include <linux/mxc_v4l2.h>

#ifdef ANDROID
	#define LOG_TAG "BoundaryCameraHal"
	#define ERRMSG LOGE
#else
	#include <stdarg.h>
inline int errmsg( char const *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	return vfprintf( stderr, fmt, ap );
}
	#define ERRMSG errmsg
#endif

// #define DEBUGPRINT
#include "debugPrint.h"

static int xioctl(int fd, int request, void *arg)
{
	int r;
	//	printf("%s: request %x\n", __func__, request);
	//	fflush (stdout);

	do {
		r = ioctl (fd, request, arg);
	} while (-1 == r && EINTR == errno);
	//	printf("%s: result %x\n", __func__, r);
	//	fflush (stdout);
	//	usleep(500);
	//	printf("%s: return %d\n", __func__, r);
	return r;
}

camera_t::camera_t
( char const *devName,
  unsigned    width,
  unsigned    height,
  unsigned    fps,
  unsigned    pixelformat,
  rotation_e  rotation )
: fd_( -1 )
, w_(width)
, h_(height)
, buffers_(0)
, n_buffers_(0)
, buffer_length_(0)
, numRead_(0)
, frame_drops_(0)
, lastRead_(0xffffffff)
{
	struct stat st;

	if (-1 == stat (devName, &st)) {
		ERRMSG( "Cannot identify '%s': %d, %s\n",
			devName, errno, strerror (errno));
		return ;
	}

	if (!S_ISCHR (st.st_mode)) {
		ERRMSG( "%s is no device\n", devName );
		return ;
	}

	fd_ = open (devName, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (0 > fd_) {
		ERRMSG( "Cannot open '%s': %d, %s\n",
			devName, errno, strerror (errno));
		return ;
	}

	struct v4l2_capability cap;
	if (-1 == xioctl (fd_, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			ERRMSG( "%s is no V4L2 device\n", devName);
		}
		else {
			ERRMSG( "VIDIOC_QUERYCAP:%m");
		}
		goto bail ;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		ERRMSG( "%s is no video capture device\n", devName);
		goto bail ;
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		ERRMSG( "%s does not support streaming i/o\n", devName);
		goto bail ;
	}

	int input ;
	int r;

	input = 0 ;

	/* Select video input, video standard and tune here. */
	r = xioctl (fd_, VIDIOC_S_INPUT, &input);
	if (r) {
		ERRMSG( "%s does not support input#%i, ret=0x%x\n", devName, input, r);
		goto bail ;
	}

	struct v4l2_cropcap cropcap ; memset(&cropcap,0,sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl (fd_, VIDIOC_CROPCAP, &cropcap)) {
		struct v4l2_crop crop ; memset(&crop,0,sizeof(crop));
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl (fd_, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	}
	else {
		/* Errors ignored. */
	}

	memset(&fmt_,0,sizeof(fmt_));

	fmt_.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt_.fmt.pix.width       = width ;
	fmt_.fmt.pix.height      = height ;
	fmt_.fmt.pix.pixelformat = pixelformat ;

	if (-1 == xioctl (fd_, VIDIOC_S_FMT, &fmt_)) {
		perror("VIDIOC_S_FMT");
		goto bail ;
	}

	ERRMSG("%s: sizeimage == %u\n", __func__, fmt_.fmt.pix.sizeimage);

	if ( (width != fmt_.fmt.pix.width)
	     ||
	     (height != fmt_.fmt.pix.height) ) {
		ERRMSG( "%ux%u not supported: %ux%u\n", width, height,fmt_.fmt.pix.width, fmt_.fmt.pix.height);
		goto bail ;
	}

	struct v4l2_control rotate_control ; memset(&rotate_control,0,sizeof(rotate_control));
	rotate_control.id = V4L2_CID_MXC_ROT ;
	rotate_control.value = rotation ;
	if ( 0 > xioctl(fd_, VIDIOC_S_CTRL, &rotate_control) ) {
		perror( "VIDIOC_S_CTRL(rotation)");
		goto bail ;
	}

	struct v4l2_streamparm stream_parm;
	if (-1 == xioctl (fd_, VIDIOC_G_PARM, &stream_parm)) {
		perror("VIDIOC_G_PARM");
		goto bail ;
	}

	stream_parm.parm.capture.timeperframe.numerator = 1;
	stream_parm.parm.capture.timeperframe.denominator = (fps? fps : 30);
	if (-1 == xioctl (fd_, VIDIOC_S_PARM, &stream_parm))
		perror ("VIDIOC_S_PARM");

	struct v4l2_requestbuffers req ; memset(&req,0,sizeof(req));

	req.count       = 4;
	req.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory      = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (fd_, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			ERRMSG( "%s does not support memory mapping\n", devName);
		}
		else {
			perror("VIDIOC_REQBUFS");
		}
		goto bail ;
	}

	if (req.count < 2) {
		ERRMSG( "Insufficient buffer memory on %s\n", devName);
		goto bail ;
	}

	buffers_ = (unsigned char **)calloc (req.count, sizeof (buffers_[0]));

	if (!buffers_) {
		ERRMSG( "Out of memory\n");
		goto bail ;
	}

	for (n_buffers_ = 0; n_buffers_ < req.count; ++n_buffers_) {
		struct v4l2_buffer buf ; memset(&buf,0,sizeof(buf));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers_ ;

		if (-1 == xioctl (fd_, VIDIOC_QUERYBUF, &buf)) {
			perror ("VIDIOC_QUERYBUF");
			goto bail; 
		}

		assert((0 == buffer_length_)||(buf.length == buffer_length_)); // only handle single buffer size
                buffer_length_ = buf.length;
		buffers_[n_buffers_] = (unsigned char *)
		mmap (NULL /* start anywhere */,
		      buf.length,
		      PROT_READ | PROT_WRITE /* required */,
		      MAP_SHARED /* recommended */,
		      fd_, buf.m.offset);

		if (MAP_FAILED == buffers_[n_buffers_]) {
			perror("mmap");
			goto bail ;
		}

		memset(buffers_[n_buffers_], 0, fmt_.fmt.pix.sizeimage);
		if (fmt_.fmt.pix.sizeimage > buf.length)
			ERRMSG("camera_imgsize=%x but buf.length=%x\n", fmt_.fmt.pix.sizeimage, buf.length);
	}
	pfd_.fd = fd_ ;
	pfd_.events = POLLIN ;

	return ;

	bail:
	close(fd_); fd_ = -1 ;

}

camera_t::~camera_t(void) {
	ERRMSG( "in camera destructor");
	if ( buffers_ ) {
		while ( 0 < n_buffers_ ) {
			munmap(buffers_[n_buffers_-1],buffer_length_);
			--n_buffers_ ;
		}
		free(buffers_);
		buffers_ = 0 ;
	}
	if (isOpen()) {
		close(fd_);
		fd_ = -1 ;
	}
}

// capture interface
bool camera_t::startCapture(void)
{
	if ( !isOpen() )
		return false ;

	unsigned int i;

	for (i = 0; i < n_buffers_; ++i) {
		struct v4l2_buffer buf ; memset(&buf,0,sizeof(buf));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;
		buf.m.offset    = 0;

		if (-1 == xioctl (fd_, VIDIOC_QBUF, &buf)) {
			perror ("VIDIOC_QBUF");
			return false ;
		}
		else {
			debugPrint( "queued buffer %u\n", i );
		}
	}

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (fd_, VIDIOC_STREAMON, &type)) {
		perror ("VIDIOC_STREAMON");
		return false ;
	}
	else {
		debugPrint( "streaming started\n" );
	}
	return true ;
}

bool camera_t::grabFrame(void const *&data,int &index) {

	int timeout = 100 ; // 1/10 second max 
	index = -1 ;
	while (1) {
		int numReady = poll(&pfd_, 1, timeout);
		if ( 0 < numReady ) {
			timeout = 0 ;
			struct v4l2_buffer buf ;
			memset(&buf,0,sizeof(buf));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			int rv ;
			if (0 == (rv = xioctl (fd_, VIDIOC_DQBUF, &buf))) {
				++ numRead_ ;
				if (0 <= index) {
					debugPrint("camera frame drop\n");
					++frame_drops_ ;
					returnFrame(data,index);
				}
				assert (buf.index < n_buffers_);
				data = buffers_[buf.index];
				index = buf.index ;
				lastRead_ = index ;
				continue;
			}
			else if ((errno != EAGAIN)&&(errno != EINTR)) {
				ERRMSG("VIDIOC_DQBUF");
			}
			else {
				if (EAGAIN != errno)
					ERRMSG("%s: rv %d, errno %d\n", __func__, rv, errno);
			}
		}
		break; // continue from middle
	}
	return (0 <= index);
}

void camera_t::returnFrame(void const *data, int index) {
	struct v4l2_buffer buf ;
	memset(&buf,0,sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index  = index ;
	if (0 != xioctl (fd_, VIDIOC_QBUF, &buf))
		perror("VIDIOC_QBUF");
	else {
		// debugPrint( "returned frame %p/%d\n", data, index );
	}
}

bool camera_t::stopCapture(void){
	if ( !isOpen() )
		return false ;

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl (fd_, VIDIOC_STREAMOFF, &type)) {
		perror ("VIDIOC_STREAMOFF");
		return false ;
	}
	else {
		debugPrint( "capture stopped\n" );
	}

	return true ;
}

#ifdef STANDALONE_CAMERA

	#include <ctype.h>
	#include "tickMs.h"
	#include <sys/poll.h>

static char const *devName = "/dev/video0" ;
unsigned width = 1296 ;
unsigned height = 972 ;
unsigned fps = 30 ;
unsigned iterations = 1 ;
int saveFrame = -1 ;
struct v4l2_window *win = 0 ;

static v4l2_window *create_window(char const *param)
{
	unsigned left, top, width, height ;
	if ( 4 == sscanf(param,"%ux%u+%u+%u", &width,&height,&left,&top) ) {
		struct v4l2_window *w = new struct v4l2_window ;
					memset(w,0,sizeof(*w));
		w->w.left   = left ;
		w->w.top    = top ;
		w->w.width  = width ;
		w->w.height = height ;
		return w ;
	}
	else
		return 0 ;
}

static void parseArgs( int &argc, char const **argv )
{
	for ( unsigned arg = 1 ; arg < argc ; arg++ ) {
		if ( '-' == *argv[arg] ) {
			char const *param = argv[arg]+1 ;
			if ( 'd' == tolower(*param) ) {
				devName = param+1 ;
			}
			else if ( 'w' == tolower(*param) ) {
				width = strtoul(param+1,0,0);
			}
			else if ( 'h' == tolower(*param) ) {
				height = strtoul(param+1,0,0);
			}
			else if ( 'i' == tolower(*param) ) {
				iterations = strtoul(param+1,0,0);
			}
			else if ( 's' == tolower(*param) ) {
				saveFrame = strtol(param+1,0,0);
			}
			else if ( 'o' == tolower(*param) ) {
				win = create_window(param+1);
			}
			else
				ERRMSG( "unknown option %s\n", param );

			// pull from argument list
			for ( int j = arg+1 ; j < argc ; j++ ) {
				argv[j-1] = argv[j];
			}
			--arg ;
			--argc ;
		}
	}
}

	#define V4L2_PIX_FMT_SGRBG8	v4l2_fourcc('G', 'R', 'B', 'G')

int main(int argc, char const **argv) {
	int rval = -1 ;
	parseArgs(argc,argv);
	long long start = tickMs();
	{
		camera_t camera(devName,width,height,fps,V4L2_PIX_FMT_NV12,win);
		long long end = tickMs();

		if ( camera.isOpen() ) {
			printf( "camera opened in %llu ms\n",end-start );
			start = tickMs();
			if ( camera.startCapture() ) {
				end = tickMs();
				printf( "started capture in %llu ms\n", end-start);

				long long startCapture = end ;
				long long maxGrab = 0, maxRelease = 0 ;
				for ( unsigned i = 0 ; i < iterations ; i++ ) {
					start = tickMs();
					void const *data ;
					int         index ;
					while ( !camera.grabFrame(data,index) )
						;
					end = tickMs();
					if (i == (unsigned)saveFrame) {
						FILE *fOut = fopen("/tmp/camera.out","w+");
						if (fOut) {
							fwrite(data,1,camera.imgSize(),fOut);
							fclose(fOut);
							printf( "data saved to /tmp/camera.out\n");
						}
						else
							perror("/tmp/camera.out");
					}
					long long elapsed = end-start ;
					debugPrint( "frame %p:%d, %llu ms\n", data, index, elapsed );
					if ( elapsed > maxGrab )
						maxGrab = elapsed ;
					start = tickMs();
					camera.returnFrame(data,index);
					end = tickMs();
					elapsed = end-start ;
					if ( elapsed > maxRelease )
						maxRelease = elapsed ;
				}

				start = tickMs();
				if ( camera.stopCapture() ) {
					end=tickMs();
					printf( "closed capture in %llu ms\n", end-start);
					printf( "maxGrab: %llu ms, maxRelease: %llu ms\n", maxGrab, maxRelease );
					rval = 0 ;
				}
				else
					ERRMSG( "Error stopping capture\n" );
			}
			else
				ERRMSG( "Error starting capture\n" );
		}
		start = tickMs();
	}
	long long end = tickMs();
	printf( "closed in %llu ms\n", end-start );

	return rval ;
}

#endif
