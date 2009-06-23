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
#include "jpegToYUV.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "IA_Unilever.h"
#include "CImg.h"
#include "fbDev.h"
#include "bdGraph/Scale16.h"
#include "cameraToYUV.h"

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   die = true ;
}

static int lookupBLT(int desired_h_res, int desired_v_res)
{
	int i = sizeof(BLTTable)/sizeof(BLTTable[0]);
	int diff = (BLTTable[i-1][1] - desired_h_res) +
			(BLTTable[i-1][2] - desired_v_res);
	int BLTIdx = 8;
	i--;
	for(;i != 0; i--) {
		int l_diff = (BLTTable[i-1][1] - desired_h_res) +
				(BLTTable[i-1][2] - desired_v_res);
		if(l_diff > 0)
			break;
		if(diff < l_diff) {
			diff = l_diff;
			BLTIdx = i-1;
		}
	}
	return BLTTable[BLTIdx][0];
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

static bool query_frame_dim(int fdCamera, int frm_size_index,
			unsigned int &width, unsigned int &height)
{
	struct v4l2_frmsizeenum frameSize ;
	CLEAR(frameSize);
	//VALLI
	//frameSize.pixel_format = V4L2_PIX_FMT_MJPEG;
	frameSize.pixel_format = V4L2_PIX_FMT_JPEG;
	frameSize.index = frm_size_index;
	printf("frm_size_index: %d\n", frm_size_index);
	if(0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMESIZES,&frameSize)) {
		printf("ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");
		if(frameSize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			width = frameSize.discrete.width;
			height = frameSize.discrete.height;
			printf("Frame dimensions: (%dx%d)\n", width, height);
			return true;
		}
	}
	else {
		printf("Error querying frame dimensions. errno: %d\n", errno);
	}
	return false;
}

static bool query_yuyv_frame_dim(int fdCamera, int frm_size_index,
			unsigned int &width, unsigned int &height)
{
	struct v4l2_frmsizeenum frameSize ;
	CLEAR(frameSize);
	frameSize.pixel_format = V4L2_PIX_FMT_YUYV;
	frameSize.index = frm_size_index;
	if(0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMESIZES,&frameSize)) {
		if(frameSize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			width = frameSize.discrete.width;
			height = frameSize.discrete.height;
			printf("Frame dimensions: (%dx%d)\n", width, height);
			return true;
		}
	}
	return false;
}

static void query_supported_frameintervals(int fdCamera)
{
        struct v4l2_frmivalenum frameIval ;
	CLEAR(frameIval);
	//Valli
	//frameIval.pixel_format = V4L2_PIX_FMT_MJPEG;
	frameIval.pixel_format = V4L2_PIX_FMT_JPEG;
	while(1){
		if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMEINTERVALS,&frameIval) ){
			printf( "have frame ival %d\n", frameIval.index );
			frameIval.index++ ;
		}
		else {
			printf( "No more frame ivals: %m\n" );
			break;
		}
	}
}

static void query_input(int fdCamera)
{
	struct v4l2_input input;
	memset (&input, 0, sizeof (input));

	while(1) {
		if(-1 == ioctl(fdCamera, VIDIOC_ENUMINPUT, &input))
			break;
		printf ("\tinput %d: %s\n", input.index++, input.name);
	}
}

static void query_controls(int fdCamera)
{
	printf( "controls:\n" );
	for( unsigned i = V4L2_CID_BASE ; i < V4L2_CID_LASTP1 ; i++ ){
		v4l2_queryctrl control ;
		memset(&control,0,sizeof(control));
		control.id = i ;
		if( 0 == ioctl(fdCamera,VIDIOC_QUERYCTRL,&control) )
			printf( "\tcontrol %x:\t", i ); PRINTFIXED(control.name); printf( ", type %s\n", controlTypeName(control.type) );
	}
	struct v4l2_queryctrl qctrl;
	memset(&qctrl,0,sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (0 == ioctl (fdCamera, VIDIOC_QUERYCTRL, &qctrl)) {
		printf( "extended control %x:\t", qctrl.id ); 
			PRINTFIXED(qctrl.name); 
			printf( ", type %s\n", controlTypeName(qctrl.type) );
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
}

static void query_capabilities(int fdCamera)
{
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
		if( 0 == ioctl(fdCamera, VIDIOC_QUERYSTD, &standard) )
			printf( "standard: %08llx\n", standard );
		else
			perror( "VIDIOC_QUERYSTD" );
	}
}

static void query_cropcap(int fdCamera)
{
	struct v4l2_cropcap cropcap ;
	memset(&cropcap,0,sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	if( 0 == ioctl(fdCamera,VIDIOC_CROPCAP,&cropcap)){
		printf( "have crop capabilities\n" );
		printf( "\tbounds %d:%d %dx%d\n", cropcap.bounds.left,cropcap.bounds.top,cropcap.bounds.width,cropcap.bounds.height );
		printf( "\tdefrect %d:%d %dx%d\n", cropcap.defrect.left,cropcap.defrect.top,cropcap.defrect.width,cropcap.defrect.height );
	}
}

static int query_max_framesizes(int fdCamera, unsigned int &width,
				unsigned int &height)
{
	int frmsize_idx = 0;
	unsigned int l_width, l_height;

	while(query_frame_dim(fdCamera, frmsize_idx, l_width, l_height)) {
		width = l_width;
		height = l_height;
		frmsize_idx++;
	}
	return (frmsize_idx - 1);
}

static void query_max_yuyvframesizes(int fdCamera)
{
        int frmsize_idx = 0;
        unsigned int l_width, l_height;

        while(query_yuyv_frame_dim(fdCamera, frmsize_idx, l_width, l_height)) {
                frmsize_idx++;
        }
}

static void set_format(int fdCamera, unsigned int width, unsigned int height)
{
	v4l2_format format ;
	CLEAR(format);
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	format.fmt.pix.field = V4L2_FIELD_NONE ;
	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	//VALLI
	//format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG ;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG ;
	format.fmt.pix.bytesperline = format.fmt.pix.width*2 ;
	format.fmt.pix.sizeimage = format.fmt.pix.height*format.fmt.pix.bytesperline ;
	format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB ;

	if(0 != ioctl(fdCamera,VIDIOC_S_FMT,&format))
		perror("VIDIOC_S_FMT");

#ifdef CAMERATOYUV_DEBUG	
	if(0 == ioctl(fdCamera,VIDIOC_G_FMT,&format)){
		printf( "JPEG capture data:\n" );
		printf( "\t%ux%u.. %ubpl... %ubpp\n", format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.bytesperline, format.fmt.pix.sizeimage );
	}
	else
		perror("VIDIOC_S_FMT");
#endif
}

/*
 * Low quality streaming video
 */
static bool set_timeperframe(int fdCamera, int framespersec)
{
	v4l2_streamparm streamp ;
	memset(&streamp,0,sizeof(streamp));
	streamp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	streamp.parm.capture.capability = 0x1000;
	streamp.parm.capture.capturemode=V4L2_MODE_HIGHQUALITY;
	streamp.parm.capture.timeperframe.numerator = 1;
	streamp.parm.capture.timeperframe.denominator = framespersec;
	if(ioctl(fdCamera, VIDIOC_S_PARM, &streamp)){
		fprintf(stderr, "VIDIOC_S_PARAM:%m\n");
		return false;
	}
	return true;
}

static unsigned int request_buffers(int fdCamera, unsigned int &numBuffers)
{
	v4l2_requestbuffers rb;
	CLEAR(rb);
	rb.count = numBuffers;
	rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rb.memory = V4L2_MEMORY_MMAP;
	if(0 == ioctl(fdCamera, VIDIOC_REQBUFS, &rb))
		return rb.count;
	return 0;
}

static bool alloc_and_queue_bufs(int fdCamera, unsigned int &numBuffers,
				struct map *&bufferMaps)
{
	request_buffers(fdCamera, numBuffers);
	for(unsigned i = 0; i < numBuffers; ++i) {
		struct map &m = bufferMaps[i];
		CLEAR(bufferMaps[i]);
                m.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
               	m.buf.memory = V4L2_MEMORY_MMAP;
		m.buf.index = i;
		if(0 >ioctl(fdCamera, VIDIOC_QUERYBUF, bufferMaps+i))
			return false;
		//printBuf(m.buf);
		printf("Length: %d\n", m.buf.length);
		fflush(stdout);
               	m.len = m.buf.length ;
		m.addr = mmap(NULL, m.len, PROT_READ, MAP_SHARED, fdCamera, m.buf.m.offset);
		if((0==m.addr) || (MAP_FAILED==m.addr))
			return false;
       	        if(-1 == ioctl(fdCamera, VIDIOC_QBUF, bufferMaps+i))
			return false;
	}

	return true;
}

static bool set_stream(int fdCamera, bool on_or_off)
{
	v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == ioctl(fdCamera, on_or_off ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type)) {
		printf("%s\n", on_or_off ? "VIDIOC_STREAMON" : "VIDIOC_STREAMOFF");
		return -1 ;
	}
}

static int create_JPEG_from_data(int &fdCamera, unsigned char *&buffer,
				struct map *&bufferMaps, int &cur_size)
{
	struct v4l2_buffer buf;
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	/*
 	 * We do not handle the EAGAIN case as we did not open the
 	 * device with the O_NONBLOCK flag. In case we change it
 	 * we would have to handle EAGAIN error.
 	 */
	if(-1 == ioctl(fdCamera, VIDIOC_DQBUF, &buf)) {
		fprintf(stderr, "unrecoverable error\n");
		return 0;
	}

	int size = buf.bytesused + DHT_SIZE;
	int buf_idx = buf.index;
	printf("Size: %d cur_size: %d\n", size, cur_size);

	if(cur_size < size) {
		cur_size = size;
		buffer = (unsigned char *) realloc (buffer, (size_t) cur_size);
		if(!buffer) {
			fprintf(stderr, "Error allocating buffer of size %d\n", cur_size);
			return 0;
		}
	}

	unsigned char *header = buffer;
	unsigned char *dht = buffer + JPEG_HEADER;
	unsigned char *jpeg_data = buffer + JPEG_HEADER + DHT_SIZE;
	unsigned char *collected_header = (unsigned char*) bufferMaps[buf_idx].addr;
	unsigned char *collected_jpeg_data = ((unsigned char *)
					collected_header +
					JPEG_HEADER);

	memset(buffer, 0, size);
	memcpy(header, collected_header, JPEG_HEADER);
	memcpy(dht, dht_data, DHT_SIZE);
	memcpy(jpeg_data, collected_jpeg_data,
		(buf.bytesused - JPEG_HEADER));


	{
		int jpegfd = open("/mmc/js/images/highres.jpg", O_WRONLY|O_CREAT);
		printf("Wrote %d number of bytes\n", write(jpegfd, buffer, size));
		close(jpegfd);
	}

	if(0 > ioctl(fdCamera, VIDIOC_QBUF, &buf)){
		perror("VIDIOC_QBUF");
		return 0;
	}

	return size;
}

int main( int argc, char const * const argv[] )
{
	int fdCamera = open( "/dev/video0", O_RDONLY );
	int frmsize_idx = 0;
	unsigned short disp_width = 0, disp_height = 0, disp_size = 0;
	unsigned startx = 0, starty = 0;
	int rotate90 = 0;
	unsigned yuvWidth = 0, yuvHeight = 0;
	unsigned int maxWidth = 0, maxHeight = 0, maxFrmsizeIdx = 0;
	struct map * bufferMaps;

	if(argc < 4) {
		printf("Usage:\n\
\tcameraToYUV frame_size_index disp_width disp_height [rotate90_image]\n\
\twhere\n\
\t\tframe_size_index   -  frame size index\n\
\t\tdisp_width         -  desired width of displayed image\n\
\t\tdisp_height        -  desired height of displayed image\n\
\t\trotate90_image     -  rotate image by 90 CCW\n");
		return -1;
	}

	frmsize_idx = strtoll(argv[1], 0, 0);
	disp_width = (unsigned short) atoi(argv[2]);
	disp_width = ROUND_UP16(disp_width);
	disp_height = (unsigned short) atoi(argv[3]);
	disp_height = ROUND_UP16(disp_height);
	printf("width: %d height: %d\n", disp_width, disp_height);

	if(argc >= 5)
		rotate90 = strtol(argv[4], 0, 0);

	if(argc >= 7) {
		startx = atoi(argv[5]);
		starty = atoi(argv[6]);
	}

	if( 0 <= fdCamera )
	{
		signal(SIGINT, ctrlcHandler);

		int index = 0;
		if(-1 == ioctl (fdCamera, VIDIOC_S_INPUT, &index)) {
			perror ("VIDIOC_S_INPUT");
			return -1;
		}

		//Query supported formats
/*
		{
			struct v4l2_fmtdesc fmt;
			memset(&fmt, 0, sizeof(fmt));
			fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			printf("PIXEL FORMATS: \n");
			while(1){
				if(0 == ioctl(fdCamera,VIDIOC_ENUM_FMT,&fmt)){
					printf("PIXEL FORMAT: %d\n", fmt.pixelformat);
					fmt.index++;
				}
				else
					break;
			}
		}
*/

		query_capabilities(fdCamera);
		//VALLI
		//if(!is_mpeg_supported(fdCamera) || !set_timeperframe(fdCamera, 4))
		if(!is_jpeg_supported(fdCamera) || !set_timeperframe(fdCamera, 1))
			return -1;

		maxFrmsizeIdx = query_max_framesizes(fdCamera, maxWidth, maxHeight);
		printf("maxWidth: %d maxHeight: %d\n", maxWidth, maxHeight);

		if(maxFrmsizeIdx < frmsize_idx) {
			printf("Invalid framesize index. Max supported frame size index : %d \
corresponding frame dimension: %d x %d\n", maxFrmsizeIdx, maxWidth, maxHeight);
			return -1;
		}

		query_frame_dim(fdCamera, frmsize_idx, yuvWidth, yuvHeight);

		unsigned numBuffers = 4;

		//set_format(fdCamera, yuvWidth, yuvHeight);	
		printf("maxWidth: %d maxHeight: %d\n", maxWidth, maxHeight);
		set_format(fdCamera, maxWidth, maxHeight);	

		bufferMaps = new struct map [numBuffers];
		if(!alloc_and_queue_bufs(fdCamera, numBuffers, bufferMaps)) {
			printf("Unable to alloc buffers\n");
			return -1;
		}
		set_stream(fdCamera, true);

		char const * const yuvDevs[] = {
			"/dev/fb_y"
		,	"/dev/fb_u"
		,	"/dev/fb_v"
		};
		int fdYUV[3] = {0};

		if(rotate90) {
			if(yuvHeight < disp_width)
				disp_width = yuvHeight;
			if(yuvWidth < disp_height)
				disp_height = yuvWidth;
		}
		else {
			if(yuvWidth < disp_width)
				disp_width = yuvWidth;
			if(yuvHeight < disp_height)
				disp_height = yuvHeight;
		}
		disp_size = disp_width * disp_height;

		/*
 		 * Prep YUV layer to display image
 		 */
		struct pxa27x_overlay_t overlay ;
		overlay.for_type = FOR_PLANAR_YUV420 ;
		overlay.offset_x = startx;	/* relative to the base plane */
		overlay.offset_y = starty;
		overlay.width = disp_width;
		overlay.height = disp_height;

		void *outMaps[3] = { 0 };
		unsigned length[3] = { 0 };

		/*
 		 * Allocate space based on max requirement
 		 */
		fbDevice_t &fb = getFB();
		char inBuf[1024];
		char *ptr; /* used when processing commands */
		unsigned maxLength = maxWidth * maxHeight;
		unsigned char *yuvbuf = (unsigned char *) malloc(3*maxLength);
		unsigned char *ybuf = yuvbuf;
		unsigned char *ubuf = yuvbuf + maxLength;
		unsigned char *vbuf = yuvbuf + (2 * maxLength);
		unsigned imgSize = 0;
		unsigned short *rgbimg = NULL;
		unsigned char *bimg = (unsigned char *)calloc(disp_size, 1);
		unsigned char *alpha = (unsigned char *) calloc(disp_size, 1);
		unsigned short *pixData = (unsigned short *) calloc(disp_size, sizeof(unsigned short));
		unsigned short *newimg = (unsigned short *) calloc(disp_size, sizeof(unsigned short));
		unsigned short *scaledimg = (unsigned short *) calloc(disp_size, sizeof(unsigned short));
		/*
		 * color to be used while drawing wrinkles and spots
		 */
		unsigned short penColor = fb.get16(240, 0, 250);
		unsigned RGB = (240 << 16) | 250;
		unsigned int camera_mode = SWITCHED_OFF;
		unsigned char *buffer = NULL; //will contain captured data
		unsigned short *capturedImgRGB = (unsigned short *) calloc(maxLength, sizeof(unsigned short));
		unsigned int analyze_image_x = 0, analyze_image_y = 0;
		int cur_size = 0;
		int size = 0;
		pollfd filedes[2];
		filedes[0].fd = fileno(stdin);
		filedes[0].events = POLLIN;
		filedes[1].fd = fdCamera;
		filedes[1].events = POLLIN;

		for(int idx = 0; idx < disp_size; idx++)
			pixData[idx] = penColor;

		while(1){
			int polled_desc_count = (camera_mode == STREAM_MODE || camera_mode == STILL_MODE) ? 2 : 1;

			if(poll(filedes, polled_desc_count, 100) <= 0)
				continue;

			if(filedes[0].revents & POLLIN) {
				camera_mode = INVALID;
				memset(inBuf, 0, 1024);
				fgets(inBuf,sizeof(inBuf),stdin);
				printf("Received command: %s\n", inBuf);
				fflush(stdout);
				ptr = strtok(inBuf, " ");
				switch(ptr[0]) {
					case 's': {
						switch(ptr[2]) {
							case 'a': 
								camera_mode = START_STREAMING;
								break;
							case 'o':
								camera_mode = SWITCHED_OFF;
								break;
							default:
								break;
						}
						break;
					}
					case 'q' :
						camera_mode = QUIT;
						break;
					case 'c': {
						switch(ptr[1]) {
							case 'a':
								camera_mode = SETTO_STILL_MODE;
								break;
							case 'r':
								camera_mode = CROP_MODE;
								break;
							default:
								break;
						}
						break;
					}
					case 'w': {
						camera_mode = ANALYZE_WRINKLE;
						break;
					}
					case 'm': {
						camera_mode = ANALYZE_MHP;
						break;
					}
					case 'p': {
						camera_mode = ANALYZE_PINKISHWHITE;
						break;
					}
					default:
						break;
				}
			}

			printf("camera_mode: %d\n", camera_mode);
			fflush(stdout);
			if(camera_mode == QUIT) {
				if(rgbimg)
					free(rgbimg);
				rgbimg = NULL;
				if(newimg)
					free(newimg);
				newimg = NULL;
				if(scaledimg)
					free(scaledimg);
				scaledimg = NULL;
				if(bimg)
					free(bimg);
				bimg = NULL;
				if(alpha)
					free(alpha);
				alpha = NULL;
				if(pixData)
					free(pixData);
				pixData = NULL;
				break;
			}
			else if(camera_mode == INVALID)
				continue;

			switch(camera_mode) {
				case SWITCH_OFF:
					camera_mode = SWITCHED_OFF;
				case SWITCHED_OFF:
					break;
				case START_STREAMING: {
#if 0
					printf("Stating Streaming Mode\n");
					for(int i = 0 ; i < 3 ; i++) {
						fdYUV[i] = open(yuvDevs[i], O_RDWR);
						if(0 > fdYUV[i]) {
							perror(yuvDevs[i]);
							return -1 ;
						}
					}

					if( 0 != ioctl(fdYUV[0],PXA27X_YUV_SET_DIMENSIONS,&overlay)){
						perror( "PXA27X_YUV_SET_DIMENSIONS" );
						return -1 ;
					}

					unsigned yBytesOut = overlay.width*overlay.height ;
					length[0] = yBytesOut;
					length[1] = yBytesOut >> 2;
					length[2] = yBytesOut >> 2;

					for( unsigned i = 0 ; i < 3 ; i++ ){
						outMaps[i] = mmap(NULL, length[i], PROT_READ | PROT_WRITE, MAP_SHARED, fdYUV[i], 0);
						if( !outMaps[i] || (MAP_FAILED == outMaps[i]) ){
							perror( "mmap" );
							return -1 ;
						}
					}

#endif
					camera_mode = STREAM_MODE;
					break;
				}
				case STREAM_MODE:
					printf("Streaming data\n");
					if((size = create_JPEG_from_data(fdCamera, buffer, bufferMaps, cur_size)) == 0)
						break;
#if 0
					jpegToYUVBuf(buffer, size, ybuf, ubuf, vbuf, disp_width, disp_height, rotate90);
					memcpy(outMaps[0], ybuf, length[0]);
					memcpy(outMaps[1], ubuf, length[1]);
					memcpy(outMaps[2], vbuf, length[2]);
#endif
					break;
				case SETTO_STILL_MODE: {
					camera_mode = STILL_MODE;
					break;
				}
				case STILL_MODE: {
					memset(capturedImgRGB, 0, maxLength * 3);
					if((size = create_JPEG_from_data(fdCamera, buffer, bufferMaps, cur_size)) == 0)
						break;
					printf("Created JPEG\n");
					fflush(stdout);
					jpegTo16RGB(buffer, size, capturedImgRGB, maxWidth, maxHeight, 0, 0, maxWidth, maxHeight);
					Scale16::scale(scaledimg, disp_width, disp_height, disp_width * sizeof(unsigned short),
							capturedImgRGB, maxWidth, maxHeight, 0, 0, 0, 0);
					fb.render(154, 50, disp_width, disp_height, scaledimg);
					camera_mode = SWITCHED_OFF;
					break;
				}
				case CROP_MODE: {
					/*
 					 * coordinates[0] - startx
 					 * coordinates[1] - starty
 					 * coordinates[2] - endx
 					 * coordinates[3] - endy
 					 */
					int coordinates[4];
					int coord_idx = 0;
					int disp_coord = 0;
					int disp_coordinates[2];

					while(coord_idx < 4) {
						if((ptr = strtok(NULL, " ")) == NULL) {
							fprintf(stderr, "inadequate number of parameters passed");
							break;
						}
						coordinates[coord_idx++] = strtol(ptr, 0, 0);
					}

					while(disp_coord < 2) {
						if((ptr = strtok(NULL, " ")) == NULL) {
							fprintf(stderr, "inadequate number of parameters passed");
							break;
						}
						disp_coordinates[disp_coord++] = strtol(ptr, 0, 0);
					}

					if(coordinates[0] < 0 || coordinates[2] < 0 ||
						coordinates[1] < 0 || coordinates[3] < 0) {
						fprintf(stderr, "coordinates are out of bounds\n");
						break;
					}

					/* remap coordinates */
					coordinates[0] = coordinates[0] * 2;
					coordinates[2] = coordinates[2] * 2;

					coordinates[1] = coordinates[1] * 2;
					coordinates[3] = coordinates[3] * 2;

					/* allocate buffer space */
					int column_count = coordinates[2] - coordinates[0];
					int row_count = coordinates[3] - coordinates[1];

					if(imgSize < (column_count * row_count)) {
						imgSize = column_count * row_count;
						rgbimg = (unsigned short *) realloc(rgbimg, imgSize * sizeof(unsigned short));
						memset(rgbimg, 0, imgSize * sizeof(unsigned short));
					}

					if(rgbimg == NULL) {
						printf("unable to allocate space to process image. exiting....\n");
						return -1;
					}

					memset(rgbimg, 0, column_count * row_count * sizeof(unsigned short));

					jpegTo16RGB(buffer, size, rgbimg, maxWidth, maxHeight, coordinates[0],
							coordinates[1], coordinates[2], coordinates[3]);

					Scale16::scale(newimg, disp_width, disp_height, disp_width * sizeof(unsigned short),
							rgbimg, column_count, row_count, 0, 0, 0, 0);

					unsigned int pixIdx = 0;
					while(pixIdx < disp_size)
						bimg[pixIdx++] = fb.getBlue(newimg[pixIdx]);

					if(outMaps[0]) {
						munmap(outMaps[0], length[0]);
						outMaps[0] = 0;
					}

					if(outMaps[1]) {
						munmap(outMaps[1], length[1]);
						outMaps[1] = 0;
					}

					if(outMaps[2]) {
						munmap(outMaps[2], length[2]);
						outMaps[2] = 0;
					}

					if(fdYUV[0])
						close(fdYUV[0]);
					if(fdYUV[1])
						close(fdYUV[1]);
					if(fdYUV[2])
						close(fdYUV[2]);

					if(disp_coordinates[0] > fb.getWidth() ||
						disp_coordinates[1] > fb.getHeight()) {
						printf("Image cannot be displayed outside the screen bounds. Exiting .....\n");
						break;
					}
					analyze_image_x = disp_coordinates[0];
					analyze_image_y = disp_coordinates[1];

					/*
					 * allocate space for alpha and pixData. these will be
					 * used by analysis code. will be freed when camera
					 * switches to streaming mode.
					 */
					fb.render(disp_coordinates[0], disp_coordinates[1], disp_width, disp_height, newimg);
					break;
				}
				case ANALYZE_WRINKLE: {
					cimg_library::CImg<float> analImg(bimg, disp_width, disp_height);

					fb.render(analyze_image_x, analyze_image_y, disp_width, disp_height, newimg);
					ptr = strtok(NULL, " ");
					double threshold = 20.0;
					double wrinkleScore = 0.0;
					int x = 0, y = 0;

					memset(alpha, 0, disp_size);
					cimg_library::CImg<float> procImg(disp_width, disp_height);
				
					if(ptr) {
						threshold = strtold(ptr, NULL);
						ptr = strtok(NULL, " ");
						if(ptr) {
							RGB = atoi(ptr);
							penColor = fb.get16(RGB);
							for(int idx = 0; idx < disp_size; idx++)
								pixData[idx] = penColor;
						}
					}

					bdcalcWrinkle(analImg, procImg, wrinkleScore, threshold, /*RGB,*/ alpha);
					/*
 					 * PLEASE PRESERVE THIS OUTPUT FORMAT AS
 					 * JAVASCRIPT END OF THE APP LOOKS FOR
 					 * THIS SPECIFIC STRING.
 					 */
					printf("DONE. SCORE: %2.2f .\n", wrinkleScore);
					fflush(stdout);

					fb.render(analyze_image_x, analyze_image_y, disp_width, disp_height, pixData, alpha);
					analImg.assign();
					procImg.assign();
					break;
				}
				case ANALYZE_MHP: {
					cimg_library::CImg<float> analImg(bimg, disp_width, disp_height);

					fb.render(analyze_image_x, analyze_image_y, disp_width, disp_height, newimg);
					ptr = strtok(NULL, " ");
					double threshold = 1.0;
					double mhpScore = 0.0;
					int x = 0, y = 0;

					memset(alpha, 0, disp_size);
					cimg_library::CImg<float> procImg(disp_width, disp_height, 1, 1, 255);
					procImg = analImg;
				
					if(ptr) {
						threshold = strtold(ptr, NULL);
						ptr = strtok(NULL, " ");
						if(ptr) {
							RGB = atoi(ptr);
							penColor = fb.get16(RGB);
							for(int idx = 0; idx < disp_width, disp_height; idx++)
								pixData[idx] = penColor;
						}
					}

					bdcalcMHP(analImg, procImg, mhpScore, threshold, /*RGB,*/ alpha);
					/*
 					 * PLEASE PRESERVE THIS OUTPUT FORMAT AS
 					 * JAVASCRIPT END OF THE APP LOOKS FOR
 					 * THIS SPECIFIC STRING.
 					 */
					printf("DONE. SCORE: %2.2f .\n", mhpScore);
					fflush(stdout);

					fb.render(analyze_image_x, analyze_image_y, disp_width, disp_height, pixData, alpha);
					analImg.assign();
					procImg.assign();
					break;
				}
				case ANALYZE_PINKISHWHITE: {
					unsigned short *dispImg = (unsigned short *) calloc(disp_size, sizeof(unsigned short));
					cimg_library::CImg<float> analImg(disp_height, disp_width, 3);
					int im_Xsize = analImg.dimx();
					int im_Ysize = analImg.dimy();

					for (int x = 0; x < im_Xsize; x++)
					{
						for (int y = 0; y < im_Ysize; y++)
						{
							analImg(x, y, 0) = fb.getRed(*(scaledimg + (x * im_Ysize + y)));
							analImg(x, y, 1) = fb.getGreen(*(scaledimg + (x * im_Ysize + y)));
							analImg(x, y, 2) = fb.getBlue(*(scaledimg + (x * im_Ysize + y)));
						}
					}

					fb.render(analyze_image_x, analyze_image_y, disp_width, disp_height, scaledimg);

					float delta_L = 4.0, delta_a = 4.0, delta_b = 4.0, perBLT = 0.30;
					int bltSize = lookupBLT(disp_height, disp_width);

					ptr = strtok(NULL, " ");
					int x = 0, y = 0;

					if(ptr) {
						perBLT = strtold(ptr, NULL);
						ptr = strtok(NULL, " ");
						if(ptr)
							delta_L = strtold(ptr, NULL);
						if(ptr)
							delta_a = strtold(ptr, NULL);
						if(ptr)
							delta_b = strtold(ptr, NULL);
					}

					bdPinkishWhite(analImg, perBLT, delta_L, delta_a, delta_b, bltSize);

					for (int x=0; x<im_Xsize; x++)
					{
						for (int y=0; y<im_Ysize; y++)
						{
							dispImg[x*im_Ysize+y] = fb.get16((char) analImg(x,y,0),
											(char) analImg(x,y,1),
											(char) analImg(x,y,2));
						}
					}

					fb.render(analyze_image_x, analyze_image_y, disp_width, disp_height, dispImg);

					printf("DONE. SCORE: .\n");
					fflush(stdout);

					analImg.assign();
					free(dispImg);
					break;
				}
				default:
					break;
			}
		}

		if(buffer)
			free(buffer);
		if(yuvbuf)
			free(yuvbuf);
	}
	else
		perror( "/dev/video0" );

	return 0 ;
}
