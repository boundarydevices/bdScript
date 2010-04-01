/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>	     /* getopt_long() */

#include <fcntl.h>	      /* low-level i/o */
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <asm/types.h>	  /* for videodev2.h */

#include <linux/fb.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <linux/mxcfb.h>

int out = 3;	//4 is main RGB, 3 is overlay

struct termios oldTermState;

void restoreTerminal()
{
	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
#if 1
  tcsetattr(STDIN_FILENO,TCSANOW,&oldTermState);
#endif
}

void initTerminal()
{
#if 1
  struct termios newTermState;
  tcgetattr(STDIN_FILENO,&oldTermState);

   /* set raw mode for keyboard input */
  newTermState = oldTermState;
  newTermState.c_lflag &= ~(ICANON|ECHO|ISIG);
  newTermState.c_iflag &= ~(IXON | IXOFF | IXANY|INLCR|ICRNL|IUCLC);	//no software flow control
  newTermState.c_oflag &= ~OPOST;			//raw output
  newTermState.c_cc[VMIN] = 1;
  newTermState.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO,TCSANOW,&newTermState);
  atexit(restoreTerminal);
#endif
  /* set non-blocking i/o so we can poll keyboard */
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

  /* set unbuffered output */
  setbuf(stdout,NULL);
}

char keyvalue=0;
int keyvaluePresent=0;

int kbhit(void)
{
  return (keyvaluePresent || (keyvaluePresent =  (read(STDIN_FILENO,&keyvalue,1) == 1)));
}
//don't wait if char not available
int my_getch(void)
{
	if (!keyvaluePresent) kbhit();
	if (keyvaluePresent)
	{
		keyvaluePresent = 0;
		return keyvalue;
	}
	return 0;
}

//wait if char not available
int my_getchar()
{
	int val=0;
	do
	{
		val = my_getch();
		if (val) break;
		usleep(100);
	} while (1);
	return val;
}

///////////////////////////////////////////

#define CLEAR(x) memset (&(x), 0, sizeof (x))

typedef enum {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
} io_method;

struct buffer {
	void *		start;
	unsigned	physaddr;
	size_t		length;
};

static io_method	io	 = IO_METHOD_MMAP;
unsigned g_half_image = 0;;

static void errno_exit(const char *s)
{
	fprintf (stderr, "%s error %d, %s\n",
		 s, errno, strerror (errno));

	exit (EXIT_FAILURE);
}

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
	usleep(500);
	return r;
}

struct dev_info {
	int dev;
	char *dev_name;
	unsigned num_frames;
	int stride;
	int width;
	int height;
	int bytes_per_pixel;
	int using_v4l2;
	unsigned pixel_format;
	unsigned pixel_order;
	int uv_size;
	unsigned u_offset;
	unsigned v_offset;
	unsigned buf_avail_mask;
	unsigned num_buffers;
	struct buffer *buffers;
};


#define BAYER unsigned

static void ConvertBayerLine_gr(unsigned char *fb_mem, const BAYER *bayer_red, const BAYER *bayer_blue, int cnt)
{
	unsigned *fb = (unsigned *)fb_mem;
	unsigned rg,gb,rb;
	do {
		rg = *bayer_red++;
		gb = *bayer_blue++;
		rb = (rg & 0xf800f800) | ((gb >> 3) & 0x001f001f);
		rg <<= 3;	//align the green portion
		gb >>= 5;
		rg &= 0x07e007e0;
		gb &= 0x07e007e0;
		rg = (rg + gb) >> 1;
		rg &= 0x07e007e0;
		*fb++ = rg | rb;
	} while ((--cnt)>0);
}

static void ConvertBayerLine_rg(unsigned char *fb_mem, const BAYER *bayer_red, const BAYER *bayer_blue, int cnt)
{
	unsigned *fb = (unsigned *)fb_mem;
	unsigned gr,bg,br;
	do {
		gr = *bayer_red++;
		bg = *bayer_blue++;
		br = ((gr << 8) & 0xf800f800) | ((bg >> (3 + 8)) & 0x001f001f);
		gr >>= 5;	//align the green portion
		bg <<= (8-5);
		gr &= 0x07e007e0;
		bg &= 0x07e007e0;
		gr = (gr + bg) >> 1;
		gr &= 0x07e007e0;
		*fb++ = gr | br;
	} while ((--cnt)>0);
}

static void ConvertBayer16Line_gr(unsigned char *fb_mem, const BAYER *bayer_red, const BAYER *bayer_blue, int cnt)
{
	unsigned short *fb = (unsigned short *)fb_mem;
	unsigned rg,gb,rb;
	do {
		rg = *bayer_red++;
		gb = *bayer_blue++;
		rb = ((rg>>16) & 0xf800) | ((gb >> (16-5)) & 0x001f);
		rg >>= 5;	//align the green portion
		gb >>= (16+5);
		rg &= 0x07e0;
		gb &= 0x07e0;
		rg = (rg + gb) >> 1;
		rg &= 0x07e0;
		*fb++ = rg | rb;
	} while ((--cnt)>0);
}

static void ConvertBayer16Line_rg(unsigned char *fb_mem, const BAYER *bayer_red, const BAYER *bayer_blue, int cnt)
{
	unsigned short *fb = (unsigned short *)fb_mem;
	unsigned gr,bg,br;
	do {
		gr = *bayer_red++;
		bg = *bayer_blue++;
		br = (gr & 0xf800) | ((bg >> (32-5)) & 0x001f);
		gr >>= (16+5);	//align the green portion
		bg >>= 5;
		gr &= 0x07e0;
		bg &= 0x07e0;
		gr = (gr + bg) >> 1;
		gr &= 0x07e0;
		*fb_mem++ = gr | br;
	} while ((--cnt)>0);
}

void black_and_white_planar(unsigned char *fb_mem, const unsigned char* p, int width)
{
	unsigned *fb = (unsigned *)fb_mem;

	while (width) {
		unsigned y = *p++;
		y |= (*p++ << 16);
		*fb++ = ((y << 8) & 0xf800f800)|((y << (8-5)) & 0x07e007e0)|((y >> 3) & 0x001f001f);
		width--;
	}
}

void black_and_white(unsigned char *fb_mem, const unsigned char* p, int width)
{
	unsigned *fb = (unsigned *)fb_mem;

	while (width) {
		unsigned y = p[1];
		y |= (p[3] << 16);
		p += 4;
		*fb++ = ((y << 8) & 0xf800f800)|((y << (8-5)) & 0x07e007e0)|((y >> 3) & 0x001f001f);
		width--;
	}
}

int yuv_streaming = -2;
void copy_yuv_frame(struct dev_info *fbdev, unsigned char *fb_mem, int stride, struct dev_info *cam, const unsigned char* p, int c_stride, int width, int height)
{
	int h;
	unsigned char *u_mem = fb_mem + fbdev->u_offset;
	unsigned char *v_mem = fb_mem + fbdev->v_offset;
	const unsigned char *pu = p + cam->u_offset;
	const unsigned char *pv = p + cam->v_offset;
	if (cam->pixel_order == 4) {
		/* interleaved */
		while (height) {
			memcpy(fb_mem, p, width<<1);
			height--;
			fb_mem += stride;
			p += c_stride;
		}
		return;
	}
	/* planar */
	h = height;
	while (h) {
		memcpy(fb_mem, p, width);
		h--;
		fb_mem += stride;
		p += c_stride;
	}
	h = height >> 1;
	while (h) {
		memcpy(u_mem, pu, width >> 1);
		h--;
		u_mem += (stride >> 1);
		pu += (c_stride >> 1);
	}
	h = height >> 1;
	while (h) {
		memcpy(v_mem, pv, width >> 1);
		h--;
		v_mem += (stride >> 1);
		pv += (c_stride >> 1);
	}
}

int queue_output(struct dev_info *fbdev, int stride, struct dev_info *cam, struct buffer *cam_buf, int c_stride, int width, int height)
{
	const unsigned char* camera_frame;
	struct v4l2_buffer buffer = {0};
	int err;
	struct buffer *out_buf;
	
	if (fbdev->buf_avail_mask) {
		buffer.index = ffs(fbdev->buf_avail_mask) - 1;
//		printf("buffer.index = %i\n", buffer.index);
		fbdev->buf_avail_mask &= ~(1 << buffer.index);
	} else {
		do {
			err = ioctl(fbdev->dev, VIDIOC_DQBUF, &buffer);
			if (!err)
				break;
			if (my_getch()) {
				printf("VIDIOC_DQBUF aborted\n");
				return -1;
			}
		} while (1);
//		printf("DQ buffer.index = %i\n", buffer.index);
	}
	if (buffer.index >= fbdev->num_frames) {
		fprintf(stderr, "DQBUF weird index %d\n", buffer.index);
		return -1;
	}
	out_buf = &fbdev->buffers[buffer.index];
	if ((c_stride == stride) && (out_buf->length == cam_buf->length)) {
		//switch buffers
		struct buffer tmp;
		tmp = *out_buf;
		*out_buf = *cam_buf;
		*cam_buf = tmp;
//		printf("switch\n");
	} else {
		copy_yuv_frame(fbdev, out_buf->start, stride,
				cam, cam_buf->start, c_stride, width, height);
	}

	buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buffer.memory = V4L2_MEMORY_MMAP;
	buffer.m.offset = out_buf->physaddr;
	err = ioctl(fbdev->dev, VIDIOC_QBUF, &buffer);
	if (err < 0) {
		perror("VIDIOC_QBUF");
		return -1 ;
	}

	if (yuv_streaming < 0) {
		yuv_streaming++;
	}
	if (!yuv_streaming) {
		int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		err = ioctl(fbdev->dev, VIDIOC_STREAMON, &type);
		if (err < 0) {
			perror("VIDIOC_STREAMON failed\n");
			return -1 ;
		}
		printf( "streaming on\n" );
		yuv_streaming = 1;
	}
	return 0;
}

typedef void (*ConvertBayer_t)(unsigned char *fb_mem, const BAYER *bayer_red, const BAYER *bayer_blue, int cnt);

static int process_image(struct dev_info *fb, struct dev_info *cam, struct buffer *buf)
{
	unsigned char* fb_mem = fb->buffers[0].start;
	int stride = fb->stride;
	int width = cam->width;
	int height = cam->height; 
	int c_stride = cam->stride;
	int double_c_stride = c_stride << 1;
	const char* p_blue;
	ConvertBayer_t convert;
//	printf("%s\n", __func__);
//	fputc ('.', stdout);
//	fflush (stdout);
	if (cam->pixel_order > 3) {
		if (width > fb->width)
			width = fb->width;
		if (g_half_image)
			height >>= 1;
		if (height > fb->height)
			height = fb->height;
		if (fb->using_v4l2)
			return queue_output(fb, stride, cam, buf, c_stride, width, height);
		if (cam->pixel_order == 4) {
			unsigned char* p = buf->start;
			width >>= 1;
			while (height) {
				black_and_white(fb_mem, p, width);
				height--;
				fb_mem += stride;
				p += c_stride;
			}
		} else {
			unsigned char* p = buf->start;
			width >>= 1;
			while (height) {
				black_and_white_planar(fb_mem, p, width);
				height--;
				fb_mem += stride;
				p += c_stride;
			}
		}
	} else {
		unsigned char* p = buf->start;
		width >>= 1;		/* count only RG and GR pairs*/
		height >>= 1;		/* 4 pixel group becomes 1 pixel*/
		if (width > fb->width)
			width = fb->width;
		if (g_half_image)
			height >>= 1;
		if (height > fb->height)
			height = fb->height;
		if (cam->pixel_order & 2) {
			p_blue = p;
			p += c_stride;
		} else {
			p_blue = p + c_stride;
		}
		if (cam->bytes_per_pixel == 1) {
			width >>= 1;	//2 is testing process 2 pixels wide at a time
			convert = (cam->pixel_order & 1) ? ConvertBayerLine_rg : ConvertBayerLine_gr;
		} else {
			convert = (cam->pixel_order & 1) ? ConvertBayer16Line_rg : ConvertBayer16Line_gr;
		}
		while (height) {
			convert(fb_mem, (BAYER *)p, (BAYER *)p_blue, width);
			height--;
			fb_mem += stride;
			p += double_c_stride;
			p_blue += double_c_stride;
		}
	}
	return 0;
}

static void check_for_dropped_bytes(struct dev_info *cam, const char *p)
{
	int stride = cam->stride;
	int w = cam->width * cam->bytes_per_pixel;
	int h = 0;
	int last_cnt = 0;
	
	printf("stride=%i, width=%i, expected zeroes=%i\n", stride, w, stride - w);
	while (h < cam->height) {
		int cnt;
		const char* end = p + stride;
		const char* q = end;
		while (q > p) {
			q--;
			if (*q) {
				q++;
				break;
			}
		}
		cnt = end - q;
		if (last_cnt != cnt) {
			last_cnt = cnt;
			printf("line %i, trailing zeroes=%i\n", h, cnt);
		}
		h++;
		p += stride;
	}
}

static void print_bayer_segment(struct dev_info *cam, const char *p)
{
	int width = 32;
	int height = 32;
	int stride = cam->stride;
	int b;
	int w = ((cam->width - width) >> 1) & ~0x7;
	int h = ((cam->height - height) >> 1) & ~0x7;
	unsigned *q;
	const char * seg;
	char buf[68];
	printf("stride=0x%x, w=0x%x, h=0x%x\r\n", stride, w, h);
	seg = p + ((stride * h) + w);
	q = (unsigned *)seg;
	buf[0] = 0x20;

	buf[width << 1] = 0;
	height >>= 1;
	while (height--) {
		b = (width<<1) - 1;
		for (w = 0; w < width; w++, b-=2) {
			buf[b] = (seg[w] >= 0x40) ? 'r' : 'g';
			if (w)
				buf[b+1] = (buf[b] == buf[b+2]) ? '*' : 0x20;
		}
		printf("%s\n", buf);
		printf("%08x%08x%08x%08x%08x%08x%08x%08x\r\n", q[7],q[6],q[5],q[4], q[3],q[2],q[1],q[0]);
		seg += stride;
		q = (unsigned *)seg;
		printf("%08x%08x%08x%08x%08x%08x%08x%08x\r\n", q[7],q[6],q[5],q[4], q[3],q[2],q[1],q[0]);
		seg += stride;
		q = (unsigned *)seg;
	}
	printf("\r\n\r\n");
	fflush(stdout);
	usleep(50000);
	fflush(stdout);
}

unsigned int first_nonzero(unsigned char* p, int mem_size)
{
	unsigned int ret = 0;
	unsigned int* q = (unsigned int*)p;
	int cnt = mem_size >> 2;
	while (cnt) {
		ret = *q++;
		if (ret)
			break;
		cnt--;
	}
	return ret;
}

static int read_frame_mmapped(struct dev_info *fb, struct dev_info *cam, int print_bayer)
{
	struct v4l2_buffer buf1, buf2;
	unsigned int i;
	unsigned val;
	int ret = 0;
	struct buffer *b;

#if 1
	fd_set rfds;
	struct timeval tv;
	int retval;

	FD_ZERO(&rfds);
	FD_SET(cam->dev, &rfds);
	/* Wait up to five ms. */
	tv.tv_sec = 1;
	tv.tv_usec = 5000;

	retval = select(cam->dev + 1, &rfds, NULL, NULL, &tv);
	if (retval == -1)
		perror("select()");
	else if (!retval)
		printf("select() timeout, retval=%x\n", retval);
//	else	printf("select() good!!!, retval=%x\n", retval);
#endif
	do {
		CLEAR(buf2);
		buf2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf2.memory = V4L2_MEMORY_MMAP;
		if (-1 == xioctl (cam->dev, VIDIOC_DQBUF, &buf2)) {
			if (errno != EAGAIN)
				errno_exit ("VIDIOC_DQBUF");
			if (!ret)
				printf("%s: IO_METHOD_MMAP EAGAIN\n", __func__);
			break;
		}
		if (ret) {
			if (-1 == xioctl (cam->dev, VIDIOC_QBUF, &buf1))
				errno_exit ("VIDIOC_QBUF");
		}
		ret++;
		buf1 = buf2;
	} while (1);

	if (!ret)
		return ret;
//	printf("%s: IO_METHOD_MMAP\n", __func__);
//	fflush (stdout);

	assert (buf1.index < cam->num_buffers);

	b = &cam->buffers[buf1.index];
#if 0
	val = first_nonzero(b->start, b->length);
	if (!val)
		printf("zero image\n");
	else {
		printf("got data 0x%x\n", val);
		process_image(fb, cam, b);
	}
#else
	process_image(fb, cam, b);
	if (print_bayer) {
//		print_bayer_segment(cam, b->start);
		check_for_dropped_bytes(cam, b->start);
	}
//	memset(b->start, 0, b->length);
#endif
	buf1.m.offset = b->physaddr;
	if (-1 == xioctl (cam->dev, VIDIOC_QBUF, &buf1))
		errno_exit ("VIDIOC_QBUF");

	return ret;
}

static int read_frame(struct dev_info *fb, struct dev_info *cam, int print_bayer)
{
	struct v4l2_buffer buf;
	unsigned char* p;
	unsigned int i;
	unsigned val;
	switch (io) {
	case IO_METHOD_READ:
		printf("%s: IO_METHOD_READ\n", __func__);
		fflush (stdout);
		if (-1 == read (cam->dev, cam->buffers[0].start, cam->buffers[0].length)) {
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit ("read");
			}
		}

		process_image(fb, cam, cam->buffers[0].start);

		break;

	case IO_METHOD_MMAP:
		return read_frame_mmapped(fb, cam, print_bayer);
	case IO_METHOD_USERPTR:
		printf("%s: IO_METHOD_USERPTR\n", __func__);
		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl (cam->dev, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit ("VIDIOC_DQBUF");
			}
		}

		for (i = 0; i < cam->num_buffers; ++i)
			if (buf.m.userptr == (unsigned long) cam->buffers[i].start
			    && buf.length == cam->buffers[i].length)
				break;

		assert (i < cam->num_buffers);

		process_image(fb, cam, (void *) buf.m.userptr);

		if (-1 == xioctl (cam->dev, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");

		break;
	}

	return 1;
}

static void mainloop(struct dev_info *fb, struct dev_info *cam, int count)
{
	int loop = 10;
	int frames = 0;
	int shown_frames = 0;
	int cnt;
	struct timeval start;
	struct timeval current;
	gettimeofday(&start, NULL);
	while (1) {
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO (&fds);
		FD_SET (cam->dev, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select (cam->dev + 1, &fds, NULL, NULL, &tv);

		if (-1 == r) {
			if (EINTR == errno)
				continue;
			errno_exit ("select");
		}

		if (0 == r) {
			fprintf (stderr, "select timeout\n");
			exit (EXIT_FAILURE);
		}
		cnt = read_frame(fb, cam, (count<=1)? 1 : 0); 
		if (cnt) {
			if (count <= 1)
				break;
			count -= cnt;
			frames += cnt;
			shown_frames++;
			if (frames >= 100) {
				unsigned long long start_usec;
				unsigned long long curr_usec;
				unsigned long long t;
				unsigned elapsed, fps;
				gettimeofday(&current, NULL);
				curr_usec = current.tv_usec + (current.tv_sec * 1000000);
				start_usec = start.tv_usec + (start.tv_sec * 1000000);
				elapsed = curr_usec - start_usec;
				t = 100000000ull;
				t *= frames;
				fps = t / elapsed;
				printf("%i frames, %i.%02i fps, dropped frames %i\n", frames, fps/100, fps%100, frames - shown_frames);
				frames = 0;
				shown_frames = 0;
				start = current;
			}
		}
		if (loop)
			loop--;
		else {
			loop = 10;
			if (my_getch()) {
				if (count > 1) {
					count = 1;
					loop = 100;
				} else
					break;
			}
		}
	}
	printf("Frames shown=%i\n",shown_frames);
}

static void stop_capturing(struct dev_info *cam)
{
	enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (cam->dev, VIDIOC_STREAMOFF, &type))
			errno_exit ("VIDIOC_STREAMOFF");

		break;
	}
}

static void start_capturing(struct dev_info *cam)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < cam->num_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR (buf);

			buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory      = V4L2_MEMORY_MMAP;
			buf.index       = i;

			if (-1 == xioctl (cam->dev, VIDIOC_QBUF, &buf))
				errno_exit ("VIDIOC_QBUF");
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (cam->dev, VIDIOC_STREAMON, &type))
			errno_exit ("VIDIOC_STREAMON");

		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < cam->num_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR (buf);

			buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory      = V4L2_MEMORY_USERPTR;
			buf.index       = i;
			buf.m.userptr   = (unsigned long) cam->buffers[i].start;
			buf.length      = cam->buffers[i].length;

			if (-1 == xioctl (cam->dev, VIDIOC_QBUF, &buf))
				errno_exit ("VIDIOC_QBUF");
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (cam->dev, VIDIOC_STREAMON, &type))
			errno_exit ("VIDIOC_STREAMON");

		break;
	}
}

static void uninit_device(struct dev_info *cam)
{
	unsigned int i;

	switch (io) {
	case IO_METHOD_READ:
		free (cam->buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < cam->num_buffers; ++i)
			if (-1 == munmap (cam->buffers[i].start, cam->buffers[i].length))
				errno_exit ("munmap");
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < cam->num_buffers; ++i)
			free (cam->buffers[i].start);
		break;
	}

	free (cam->buffers);
}

static void init_read(struct dev_info *cam, unsigned int buffer_size)
{
	cam->buffers = calloc (1, sizeof (*cam->buffers));

	if (!cam->buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}

	cam->buffers[0].length = buffer_size;
	cam->buffers[0].start = malloc (buffer_size);

	if (!cam->buffers[0].start) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}
}

static void init_mmap(struct dev_info *cam)
{
	struct v4l2_requestbuffers req;
	int i;
	CLEAR (req);

	req.count	       = 4;
	req.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory	      = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (cam->dev, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
				 "memory mapping\n", cam->dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf (stderr, "Insufficient buffer memory on %s\n",
			 cam->dev_name);
		exit (EXIT_FAILURE);
	}

	cam->buffers = calloc (req.count, sizeof (*cam->buffers));

	if (!cam->buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}
	cam->num_buffers = req.count;

	for (i = 0; i < req.count; i++) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;

		if (-1 == xioctl (cam->dev, VIDIOC_QUERYBUF, &buf))
			errno_exit ("VIDIOC_QUERYBUF");

		cam->buffers[i].length = buf.length;
		cam->buffers[i].physaddr = buf.m.offset;
		cam->buffers[i].start = mmap(NULL, buf.length,
			      PROT_READ | PROT_WRITE /* required */,
			      MAP_SHARED /* recommended */,
			      cam->dev, buf.m.offset);
		if (MAP_FAILED == cam->buffers[i].start)
			errno_exit ("mmap");
		memset(cam->buffers[i].start, 0, buf.length);
	}
}

static void init_userp(struct dev_info *cam, unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;
	unsigned int page_size;
	int i;

	page_size = getpagesize ();
	buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

	CLEAR (req);

	req.count	       = 4;
	req.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory	      = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl (cam->dev, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
				 "user pointer i/o\n", cam->dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_REQBUFS");
		}
	}

	cam->buffers = calloc (4, sizeof (*cam->buffers));

	if (!cam->buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}

	for (i = 0; i < 4; ++i) {
		cam->buffers[i].length = buffer_size;
		cam->buffers[i].start = memalign (/* boundary */ page_size,
						     buffer_size);

		if (!cam->buffers[i].start) {
			fprintf (stderr, "Out of memory\n");
			exit (EXIT_FAILURE);
		}
	}
}

#define V4L2_PIX_FMT_SGRBG8	v4l2_fourcc('G', 'R', 'B', 'G')
//#define V4L2_PIX_FMT_SBGGR8	v4l2_fourcc('B', 'A', '8', '1')
#define V4L2_PIX_FMT_SGBRG8	v4l2_fourcc('G', 'B', 'R', 'G')
#define V4L2_PIX_FMT_SGRBG10	v4l2_fourcc('B', 'A', '1', '0')
#define V4L2_PIX_FMT_SBGGR16	v4l2_fourcc('B', 'Y', 'R', '2')

struct format_desc {
	unsigned pixelformat;
	unsigned pixel_order;
};
struct format_desc format_table[] = {
	{ V4L2_PIX_FMT_YUV420, 5},	//0
	{ V4L2_PIX_FMT_UYVY, 4},	//1
	{ V4L2_PIX_FMT_NV12, 5},	//2
	{ V4L2_PIX_FMT_YUV422P, 5},	//3
	{ V4L2_PIX_FMT_SGRBG8, 0},	//4
	{ V4L2_PIX_FMT_SBGGR8, 2},	//5
	{ V4L2_PIX_FMT_SGBRG8, 3},	//6
	{ V4L2_PIX_FMT_SGRBG10, 0},	//7
	{ V4L2_PIX_FMT_SBGGR16, 2},	//8
};
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

unsigned find_format(unsigned pixelformat)
{
	unsigned i = 0;
	while (i < ARRAY_SIZE(format_table)) {
		if (pixelformat == format_table[i].pixelformat)
			break;
		i++;
	}
	return i;
}

static void init_device(struct dev_info *cam, unsigned format_index, unsigned width, unsigned height, unsigned fps, int input)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_frmivalenum fi;
	unsigned int min;
	unsigned mem_size;
	int r;
	struct v4l2_streamparm stream_parm;

	if (-1 == xioctl (cam->dev, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s is no V4L2 device\n",
				 cam->dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "%s is no video capture device\n",
			 cam->dev_name);
		exit (EXIT_FAILURE);
	}

	switch (io) {
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
			fprintf (stderr, "%s does not support read i/o\n",
				 cam->dev_name);
			exit (EXIT_FAILURE);
		}

		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			fprintf (stderr, "%s does not support streaming i/o\n",
				 cam->dev_name);
			exit (EXIT_FAILURE);
		}

		break;
	}


	/* Select video input, video standard and tune here. */
	r = xioctl (cam->dev, VIDIOC_S_INPUT, &input);
	if (r) {
		fprintf (stderr, "%s does not support input#%i, ret=0x%x\n", cam->dev_name, input, r);
	}


	CLEAR (cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl (cam->dev, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl (cam->dev, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	CLEAR(fi);
	fi.pixel_format = format_table[format_index].pixelformat;
	fi.width = width;
	fi.height = height;
	r = xioctl (cam->dev, VIDIOC_ENUM_FRAMEINTERVALS, &fi);
	if (!r) {
		unsigned long long t;
		unsigned fps_min, fps_max;
		t = 10000;
		t *= fi.stepwise.max.denominator;
		t /= fi.stepwise.max.numerator;
		fps_min = t;
		t = 10000;
		t *= fi.stepwise.min.denominator;
		t /= fi.stepwise.min.numerator;
		fps_max = t;
		
		printf("fps range of %i.%04i - %i.%04i\n",
			fps_min/10000, fps_min % 10000,
			fps_max/10000, fps_max % 10000);
	} else {
		printf("VIDIOC_ENUM_FRAMEINTERVALS returned %i, errno=%i\n", r, errno);
	}
	CLEAR (fmt);

	fmt.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = width;
	fmt.fmt.pix.height      = height;
	if (format_index > (ARRAY_SIZE(format_table) - 1))
		format_index = ARRAY_SIZE(format_table) - 1;
	fmt.fmt.pix.pixelformat = format_table[format_index].pixelformat;
	
	fmt.fmt.pix.field       = 0; //V4L2_FIELD_INTERLACED;

	if (-1 == xioctl (cam->dev, VIDIOC_S_FMT, &fmt))
		errno_exit ("VIDIOC_S_FMT");

	format_index = find_format(fmt.fmt.pix.pixelformat);
	/* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	cam->width = fmt.fmt.pix.width;
	cam->height = fmt.fmt.pix.height;
	cam->stride = fmt.fmt.pix.bytesperline;
	mem_size = fmt.fmt.pix.sizeimage;
	cam->pixel_format = fmt.fmt.pix.pixelformat;
	cam->bytes_per_pixel = (cam->stride >= (cam->width << 1)) ? 2 : 1;
	cam->pixel_order = format_table[format_index].pixel_order;
	if (cam->pixel_order == 5) {
		cam->u_offset = cam->stride * cam->height;
		cam->v_offset = cam->u_offset + (cam->u_offset >> 2);
	}
	printf("camera width=%i, height=%i, stride=%i, imgsize=0x%x bytes_per_pixel=%i order=%i\n",
			cam->width, cam->height, cam->stride, mem_size,
			cam->bytes_per_pixel, cam->pixel_order);

	if (-1 == xioctl (cam->dev, VIDIOC_G_PARM, &stream_parm))
		errno_exit ("VIDIOC_G_PARM");

	stream_parm.parm.capture.timeperframe.numerator = 1;
	stream_parm.parm.capture.timeperframe.denominator = fps;
	if (-1 == xioctl (cam->dev, VIDIOC_S_PARM, &stream_parm))
		errno_exit ("VIDIOC_S_PARM");
	

	switch (io) {
	case IO_METHOD_READ:
		init_read(cam, fmt.fmt.pix.sizeimage);
		break;

	case IO_METHOD_MMAP:
		init_mmap(cam);
		break;

	case IO_METHOD_USERPTR:
		init_userp(cam, fmt.fmt.pix.sizeimage);
		break;
	}
}

static void close_device(struct dev_info *cam)
{
	if (-1 == close (cam->dev))
		errno_exit ("close");

	cam->dev = -1;
}

static void open_device(struct dev_info *cam)
{
	struct stat st;

	if (-1 == stat (cam->dev_name, &st)) {
		fprintf (stderr, "Cannot identify '%s': %d, %s\n",
			 cam->dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (!S_ISCHR (st.st_mode)) {
		fprintf (stderr, "%s is no device\n", cam->dev_name);
		exit (EXIT_FAILURE);
	}

	cam->dev = open(cam->dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == cam->dev) {
		fprintf (stderr, "Cannot open '%s': %d, %s\n",
			 cam->dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}
}

#ifndef uint32_t 
typedef unsigned uint32_t ;
#endif

struct v4l2_mxc_offset {
	uint32_t u_offset;
	uint32_t v_offset;
};


static int open_yuv_output(struct dev_info *fbdev, int width, int height, int left, int top, int out_width, int out_height, unsigned pixel_format)
{
	int err;
	struct v4l2_format fmt ; 
	struct v4l2_pix_format *pix = &fmt.fmt.pix;
	struct v4l2_mxc_offset off = {0};
	struct v4l2_requestbuffers reqbuf = {0};
	unsigned i;
	struct v4l2_crop crop;
	int fd;
	unsigned mem_size;
	unsigned uv_size = 0;

	fd = fbdev->dev = open(fbdev->dev_name, O_RDWR|O_NONBLOCK);
	if (fd < 0) {
		perror(fbdev->dev_name);
		return -1 ;
	}
	printf( "device %s opened\n", fbdev->dev_name);

	err = ioctl(fd, VIDIOC_S_OUTPUT, &out);
	if (err < 0) {
		perror("VIDIOC_S_OUTPUT");
		return -1 ;
	}
	printf( "set output to /dev/fb2\n" );
	
	memset(&fmt,0,sizeof(fmt));
		
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	err = ioctl(fd, VIDIOC_G_FMT, &fmt);
	if (err < 0) {
		perror("VIDIOC_G_FMT failed\n");
		return -1 ;
	}
	printf("have default window: %ux%u (%u bytes per line)\n", pix->width, pix->height, pix->bytesperline );
	printf("pixelformat 0x%08x\n", pix->pixelformat);
	printf("size %u\n", pix->sizeimage );
	printf("field: %d\n", pix->field );
	printf("colorspace: %u\n", pix->colorspace );
	printf("priv: 0x%x\n", pix->priv );

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	pix->width = width ;
	pix->height = height ;
	pix->pixelformat = pixel_format;
	switch (pixel_format) {
	case V4L2_PIX_FMT_YUV420:
		pix->bytesperline = (width + 15) & ~15;
		off.u_offset = pix->bytesperline * height;
		pix->sizeimage = off.u_offset * 3 / 2;
		fbdev->uv_size = (off.u_offset / 4);
		off.v_offset = off.u_offset + fbdev->uv_size;
		fmt.fmt.pix.priv = (unsigned long) &off;
		fbdev->u_offset = off.u_offset;
		fbdev->v_offset = off.v_offset;
		break;
	case V4L2_PIX_FMT_NV12:
		pix->bytesperline = (width + 15) & ~15;
		off.u_offset = pix->bytesperline * height;
		pix->sizeimage = off.u_offset * 3 / 2;
		fbdev->uv_size = (off.u_offset / 2);
		off.v_offset = 0;
		fmt.fmt.pix.priv = (unsigned long) &off;
		fbdev->u_offset = off.u_offset;
		fbdev->v_offset = off.v_offset;
		break;
	case V4L2_PIX_FMT_YUV422P:
		pix->bytesperline = (width + 15) & ~15;
		off.u_offset = pix->bytesperline * height;
		pix->sizeimage = off.u_offset * 2;
		fbdev->uv_size = (off.u_offset / 2);
		off.v_offset = off.u_offset + fbdev->uv_size;
		fmt.fmt.pix.priv = (unsigned long) &off;
		fbdev->u_offset = off.u_offset;
		fbdev->v_offset = off.v_offset;
		break;
	case V4L2_PIX_FMT_UYVY:
	default:
		fmt.fmt.pix.priv = 0;
		pix->bytesperline = ((width << 1) + 15) & ~15;
		pix->sizeimage = width * height;
		break;
	}

	err = ioctl(fd, VIDIOC_S_FMT, &fmt);
	if (err < 0) {
		perror("VIDIOC_S_FMT failed\n");
		return -1 ;
	}
	
	width = pix->width;
	height = pix->height;
	fbdev->stride = pix->bytesperline;
	fbdev->width = width;
	fbdev->height = height;
	mem_size = pix->sizeimage;

	if (pix->pixelformat == V4L2_PIX_FMT_YUV420) {
		fbdev->uv_size = (pix->bytesperline * height / 4);
		fbdev->u_offset = pix->bytesperline * height;
		fbdev->v_offset = fbdev->u_offset + fbdev->uv_size;
	}
	printf( "set format %ix%i, stride=%i\n", width, height, fbdev->stride);
		
	memset(&crop,0,sizeof(crop));
	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c.left = left;
	crop.c.top = top;
	crop.c.width = out_width;
	crop.c.height = out_height;

	err = ioctl(fd, VIDIOC_S_CROP, &crop);
	if (err < 0) {
		perror("VIDIOC_S_CROP failed\n");
		return -1 ;
	}

	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = fbdev->num_frames ;

	err = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	if (err < 0) {
		perror("VIDIOC_REQBUFS failed\n");
		return -1 ;
	}

	if (reqbuf.count < fbdev->num_frames) {
		fprintf(stderr,"VIDIOC_REQBUFS: not enough buffers\n");
		return -1 ;
	}

	printf("have %u video output buffers\n", reqbuf.count);


	fbdev->num_frames = reqbuf.count ;
	fbdev->buffers = calloc(fbdev->num_frames, sizeof (*fbdev->buffers));

	for (i = 0; i < fbdev->num_frames; i++) {
		struct v4l2_buffer buffer = {0};
		buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		err = ioctl(fd, VIDIOC_QUERYBUF, &buffer);
		if (err < 0) {
			perror("VIDIOC_QUERYBUF: not enough buffers\n");
			return -1 ;
		}

		fbdev->buffers[i].length = buffer.length;
		fbdev->buffers[i].physaddr = buffer.m.offset;
		printf("V4L2buf phy addr: %08x, size = %d\n",
				(unsigned int)buffer.m.offset, buffer.length);
		fbdev->buffers[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, buffer.m.offset);

		if (fbdev->buffers[i].start == MAP_FAILED) {
			perror("mmap failed\n");
			return -1 ;
		}
		memset(fbdev->buffers[i].start, i ? 0x80 : 0,
				buffer.length);
	}
	printf( "mapped %u buffers\n", fbdev->num_frames);
	fbdev->buf_avail_mask = (1 << fbdev->num_frames) - 1;
	return 0;
}


int close_yuv_output(struct dev_info *fb)
{
	if (fb->dev) {
		int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		int err = ioctl(fb->dev, VIDIOC_STREAMOFF, &type);
		if (err < 0) {
			perror("VIDIOC_STREAMOFF failed\n");
			return -1 ;
		}
		printf( "streaming off\n" );
		close(fb->dev);
	}
	return 0 ;
}


int open_fbdev(struct dev_info *fb)
{
	fb->dev = open(fb->dev_name, O_RDWR);
	if (fb->dev) {
		struct fb_fix_screeninfo fixed_info;
		int err = ioctl( fb->dev, FBIOGET_FSCREENINFO, &fixed_info);
		if( 0 == err ) {
			struct fb_var_screeninfo variable_info;
			err = ioctl( fb->dev, FBIOGET_VSCREENINFO, &variable_info );
			if( 0 == err ) {
				struct buffer * b;
				unsigned size;
				fb->num_frames = 1;
				fb->buffers = b = calloc(1, sizeof (*fb->buffers));
				if (!b) {
					printf("%s out of memory\n", __func__);
					return -1;
				}
				fb->stride = fixed_info.line_length;
				fb->width   = variable_info.xres ;
				fb->height  = variable_info.yres ;
				b->length = fixed_info.smem_len ;
				b->physaddr = 0;
				size = fb->stride * fb->height;
				b->start = (unsigned char *) mmap( 0, size, PROT_WRITE, MAP_SHARED, fb->dev, 0);
				if (b->start)
					return 0;
			}
		}
	}
	return -1;
}

void close_fbdev(struct dev_info *fb)
{
	if (fb->dev)
		close(fb->dev);
}
int enable_alpha_blend(struct dev_info *fb)
{
	struct mxcfb_gbl_alpha alpha;
	alpha.alpha = 0;
	alpha.enable = 1;
	if (ioctl(fb->dev, MXCFB_SET_GBL_ALPHA, &alpha) < 0) {
		perror("set alpha blending failed\n");
		return -1;
	}
	printf( "set global alpha\n" );
}


static void usage(FILE *fp, int argc, char **argv)
{
	fprintf (fp,
		"Usage: %s [options]\n\n"
		"Options:\n"
		"-d | --device name   Video device name [/dev/video0]\n"
		"-? | --help	Print this message\n"
		"-m | --mmap	Use memory mapped buffers\n"
		"-r | --read	Use read() calls\n"
		"-u | --userp	Use application allocated buffers\n"
		"-f | --format	0-5\n"
		"-w | --width	20-25925\n"
		"-h | --height	2-1944\n"
		"-p | --fps	1-10000\n"
		"-2 | --half	display top half only\n"
		"-c | --count	# of frames to display\n"
		"-i | --input	input device\n"
		"-o | --output	output frame (RGB or YUV)\n"
		"",
		argv[0]);
}

static const char short_options [] = "d:?mruf:w:h:p:2nc:i:o:";

static const struct option long_options [] = {
	{ "device",	required_argument,  NULL,	   'd' },
	{ "help",	no_argument,	    NULL,	   '?' },
	{ "mmap",	no_argument,	    NULL,	   'm' },
	{ "read",	no_argument,	    NULL,	   'r' },
	{ "userp",	no_argument,	    NULL,	   'u' },
	{ "format",	required_argument,  NULL,	   'f' },
	{ "width",	required_argument,  NULL,	   'w' },
	{ "height",	required_argument,  NULL,	   'h' },
	{ "fps",	required_argument,  NULL,	   'p' },
	{ "half",	no_argument,	    NULL,	   '2' },
	{ "no_v4l2",	no_argument,	    NULL,	   'n' },
	{ "count",	required_argument,  NULL,	   'c' },
	{ "input",	required_argument,  NULL,	   'i' },
	{ "output",	required_argument,  NULL,	   'o' },
	{ 0, 0, 0, 0 }
};

#if 1
#define MULT_BY 12	//12 - 2592x1944, 6 - 1296x972, 3 - 648x486, 1 - 216x162
#define CAMERA_WIDTH (216*MULT_BY)
#define CAMERA_HEIGHT (162*MULT_BY)
#else
//#define CAMERA_WIDTH (2048)
//#define CAMERA_HEIGHT (1536)
//#define CAMERA_WIDTH 2064
//#define CAMERA_HEIGHT 1552
//#define CAMERA_WIDTH 2432
//#define CAMERA_HEIGHT 1824
#define CAMERA_WIDTH 2584
#define CAMERA_HEIGHT 1938
//#define CAMERA_WIDTH 2304
//#define CAMERA_HEIGHT 1728
#endif
int main(int argc, char **argv)
{
	int format_index = 0;
	unsigned width = CAMERA_WIDTH;
	unsigned height = CAMERA_HEIGHT;
	unsigned fps = 100;
	unsigned count = 10000;
	int input = 1;
	int no_v4l2 = 0;
	struct dev_info cam;
	struct dev_info fb_dev;
	struct dev_info fb_yuvdev;

	memset(&cam, 0, sizeof(cam));
	memset(&fb_dev, 0, sizeof(fb_dev));
	memset(&fb_yuvdev, 0, sizeof(fb_yuvdev));

	cam.dev_name = "/dev/video0";
	fb_dev.dev_name = "/dev/fb0";
	fb_yuvdev.dev_name = "/dev/video16";
	fb_yuvdev.num_frames = 4;
	int err;

	for (;;) {
		int index;
		int c;

		c = getopt_long (argc, argv,
				 short_options, long_options,
				 &index);

		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'd':
			cam.dev_name = optarg;
			break;
		case 'f':
			sscanf(optarg,"%i",&format_index);
			if (format_index > (ARRAY_SIZE(format_table) - 1))
				format_index = ARRAY_SIZE(format_table) - 1;
			printf("format_index:%i\n", format_index);
			break;
		case 'w':
			sscanf(optarg,"%i",&width);
			if (width > 2592)
				width = 2592;
			break;
		case 'h':
			sscanf(optarg,"%i",&height);
			if (height > 1944)
				height = 1944;
			break;
		case 'p':
			sscanf(optarg,"%i",&fps);
			printf("fps=%i\n", fps);
			if (fps > 10000)
				fps = 10000;
			if (fps < 1)
				fps = 1;
			break;
		case 'c':
			sscanf(optarg,"%i",&count);
			if (count < 1)
				count = 1;
			break;
		case 'i':
			sscanf(optarg,"%i",&input);
			if (input < 0)
				input = 1;
			break;
		case '2':
			g_half_image = 1;
			break;
		case 'n' :
			no_v4l2 = 1;
			break;
		case '?':
			usage (stdout, argc, argv);
			exit (EXIT_SUCCESS);

		case 'm':
			io = IO_METHOD_MMAP;
			break;

		case 'r':
			io = IO_METHOD_READ;
			break;

		case 'u':
			io = IO_METHOD_USERPTR;
			break;

		case 'o':
			if('y' == *optarg)
				out = 3 ;
			else if('r' == *optarg)
				out = 4 ;
			else
				fprintf(stderr, "Invalid option for -o. Use RGB or YUV\n" );
			break;

		default:
			usage(stderr, argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	initTerminal();
	open_device(&cam);
	open_fbdev(&fb_dev);
	init_device(&cam, format_index, width, height, fps, input);
	if ((!no_v4l2) && (cam.pixel_order > 3)) {
		int left, top, out_width, out_height;
		width = cam.width;
		height = cam.height;
		if (width > fb_dev.width)
			width = fb_dev.width;
		if (height > fb_dev.height)
			height = fb_dev.height;
		out_width = width;
//		out_width >>= 1;
		out_height = height;
//		out_height >>= 1;
		left = (fb_dev.width - out_width) >> 1;
		top =  (fb_dev.height - out_height) >> 1;
		err = open_yuv_output(&fb_yuvdev, width, height, left, top, out_width, out_height, cam.pixel_format);
		if (err)
			exit(EXIT_FAILURE);
		enable_alpha_blend(&fb_dev);
		fb_yuvdev.using_v4l2 = 1;
	}
	start_capturing(&cam);
	if (fb_yuvdev.using_v4l2) {
		mainloop(&fb_yuvdev, &cam, count);
	} else {
		mainloop(&fb_dev, &cam, count);
	}
	stop_capturing(&cam);
	uninit_device(&cam);
	close_device(&cam);
	close_yuv_output(&fb_yuvdev);
	close_fbdev(&fb_dev);
	restoreTerminal();
	exit(EXIT_SUCCESS);
	return 0;
}

