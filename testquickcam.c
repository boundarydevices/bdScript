/*
 * Test quickcam: Logitech Quickcam Express Video Camera driver test tool.
 * Copyright (C) 2001 Nikolas Zimmermann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * <wildfox@kde.org>
 *
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <errno.h>

#include <linux/videodev.h>


#define VERSION "$Id: testquickcam.c,v 1.2 2003-05-16 07:25:19 tkisky Exp $"

int open_camera(const char *devicename,int* device_fd)
{
    *device_fd = open(devicename, O_RDWR);
    if (*device_fd <= 0)
    {
	printf("Device %s couldn't be opened\n", devicename);
	return 0;
    }
    return 1;
}

void close_camera(int device_fd)
{
    close(device_fd);
}

void get_camera_info(int device_fd,struct video_capability* vidcap, struct video_window *vidwin, struct video_picture* vidpic, struct video_clip *vidclips)
{
    ioctl(device_fd, VIDIOCGCAP, vidcap);
    ioctl(device_fd, VIDIOCGWIN, vidwin);
    ioctl(device_fd, VIDIOCGPICT, vidpic);

    vidwin->clips = vidclips;
    vidwin->clipcount = 0;
}

void print_camera_info(struct video_capability* vidcap, struct video_window *vidwin, struct video_picture* vidpic)
{

    printf("    *** Camera Info ***\n");
    printf("Name:           %s\n", vidcap->name);
    printf("Type:           %d\n", vidcap->type);
    printf("Minimum Width:  %d\n", vidcap->minwidth);
    printf("Maximum Width:  %d\n", vidcap->maxwidth);
    printf("Minimum Height: %d\n", vidcap->minheight);
    printf("Maximum Height: %d\n", vidcap->maxheight);
    printf("X:              %d\n", vidwin->x);
    printf("Y:              %d\n", vidwin->y);
    printf("Width:          %d\n", vidwin->width);
    printf("Height:         %d\n", vidwin->height);
    printf("Depth:          %d\n", vidpic->depth);

    if(vidcap->type & VID_TYPE_MONOCHROME)
	printf("Color           false\n");
    else
	printf("Color           true\n");
    printf("Version:        %s\n", VERSION);
}

static void hexdump_data(const unsigned char *data, int len)
{
    const int bytes_per_line = 32;
    char tmp[128];
    int i = 0, k = 0;

    for(i = 0; len > 0; i++, len--)
    {
	if(i > 0 && ((i % bytes_per_line) == 0))
	{
    	    printf("%s\n", tmp);
            k = 0;
        }
        if ((i % bytes_per_line) == 0)
    	    k += sprintf(&tmp[k], "[%04x]: ", i);
        k += sprintf(&tmp[k], "%02x ", data[i]);
    }

    if (k > 0)
	printf("%s\n", tmp);
}

void read_test(int device_fd, struct video_capability* vidcap, int quiet)
{
    int len = vidcap->maxwidth * vidcap->maxheight * 3;
    int i;
    unsigned char *buffer;
    if (len) {
	buffer = malloc(len);
	for (i=0; i<20; i++) {
	    len = read(device_fd, buffer, len);
#if 1
	    if (!quiet) {
		printf(" *** read() test ***\n");
		printf("Read length: %d\n", len);
		printf("Raw data: \n\n");
		hexdump_data(buffer, len);
	    }
#endif
	}
	free(buffer);
    } else {
	printf("0 length maxwidth or maxheight\n");
    }
}
static inline int min(int x,int y)
{
   return (x<y)? x : y;
}
static inline unsigned short * getPixel(unsigned short *fbMem, int fbWidth,int xPos, int yPos)
{
    return fbMem + ((yPos*fbWidth) + xPos);
}
static inline void ConvertRgb24(unsigned short* fbMem, unsigned char const *video,int cnt)
{
    do {
	*fbMem++ = (video[0]>>3) | ((video[1]&0xfc)<<(5-2)) | ((video[2]&0xf8)<<(5+6-3));
	video += 3;
    } while ((--cnt)>0);
}
void renderRgb24(unsigned short *fbMem,int fbWidth, int fbHeight,
     int xPos,				//placement on screen
     int yPos,
     int w,
     int h,	 			// width and height of image
     unsigned char const *pixels,
     int imagexPos,			//offset within image to start display
     int imageyPos,
     int imageDisplayWidth,		//portion of image to display
     int imageDisplayHeight
   )
{
   int minWidth;
   int minHeight;
   if (xPos<0)
   {
      imagexPos -= xPos;		//increase offset
      imageDisplayWidth += xPos;	//reduce width
      xPos = 0;
   }
   if (yPos<0)
   {
      imageyPos -= yPos;
      imageDisplayHeight += yPos;
      yPos = 0;
   }
   if ((imageDisplayWidth <=0)||(imageDisplayWidth >(w-imagexPos))) imageDisplayWidth = w-imagexPos;
   if ((imageDisplayHeight<=0)||(imageDisplayHeight>(h-imageyPos))) imageDisplayHeight = h-imageyPos;

   pixels += ((w*imageyPos)+imagexPos)*3;

   minWidth = min(fbWidth-xPos,imageDisplayWidth);	// << 1;	//2 bytes/pixel
   minHeight= min(fbHeight-yPos,imageDisplayHeight);
   if ((minWidth > 0) && (minHeight > 0))
   {
      unsigned short *pix = getPixel(fbMem,fbWidth,xPos,yPos);
      w = w*3;
      do
      {
         ConvertRgb24(pix,pixels,minWidth);
	 pix += fbWidth;
         pixels += w;
      } while (--minHeight);
   }
}
int mmap_test(int device_fd, struct video_capability* vidcap, int quiet)
{
    struct video_mbuf vidbuf;
    struct video_mmap vm;
    int numframe = 0;
    unsigned char const *buffer;
    unsigned short *fbMem = NULL;
    int fbWidth,fbHeight,memSize;
    int frameCnt=0;
    int fbDev=0;
    int left,top;

    memset(&vm, 0, sizeof(vm));
    if (!quiet) {
	printf(" *** mmap() test ***\n");
    }
    if (ioctl(device_fd, VIDIOCGMBUF, &vidbuf) < 0) return;
    buffer = mmap(NULL, vidbuf.size, PROT_READ, MAP_SHARED, device_fd, 0);
    if (!buffer) return;

    fbDev = open( "/dev/fb0", O_RDWR );
    if (fbDev)
    {
	struct fb_fix_screeninfo fixed_info;
	int err = ioctl( fbDev, FBIOGET_FSCREENINFO, &fixed_info);
	if( 0 == err ) {
	    struct fb_var_screeninfo variable_info;
	    err = ioctl( fbDev, FBIOGET_VSCREENINFO, &variable_info );
	    if( 0 == err ) {
		fbWidth   = variable_info.xres ;
		fbHeight  = variable_info.yres ;
		memSize = fixed_info.smem_len ;
		printf("screen width:%d, Screen height:%d\n",fbWidth,fbHeight);
		fbMem = mmap( 0, memSize, PROT_WRITE, MAP_SHARED, fbDev, 0 );
	    }
	}
        vm.format = VIDEO_PALETTE_RGB24;
        vm.frame  = 0;
        vm.width  = min(vidcap->maxwidth,fbWidth);
        vm.height = min(vidcap->maxheight,fbHeight);
        if ( (err=ioctl(device_fd, VIDIOCMCAPTURE, &vm))<0) {
		printf("VIDIOCMCAPTURE ret:%d, errno:%d, width:%d, height:%d\n",err,errno,vm.width,vm.height);
	}

	left = (fbWidth-vm.width)>>1;
	top = (fbHeight-vm.height)>>1;
        while (1) {
	    if ( (err=ioctl(device_fd, VIDIOCSYNC, &numframe)) < 0 ) {
		printf("VIDIOCSYNC ret:%d, errno:%d, frame:%d\n",err,errno,frameCnt);
	    	break;
	    }
//	    printf("%d ",frameCnt);
	    frameCnt++;
	    renderRgb24(fbMem,fbWidth,fbHeight,left,top,vm.width,vm.height,buffer,0,0,vm.width,vm.height);
#if 0
	    if(!quiet) {
	        printf("Read length: %d\n", vidbuf.size);
	        printf("Raw data: \n\n");
	        hexdump_data(buffer, vidbuf.size);
	    }
#endif

        }
	close(fbDev);
    }
    return device_fd;
}

void read_loop(int device_fd, struct video_capability* vidcap)
{
    int loop = 0;
    while(1)
    {
	loop++;
	read_test(device_fd,vidcap,1);
	printf("*** Read frames: %d times!\n", loop);
    }
}

int main(int argc, char *argv[])
{
    int device_fd;
    struct video_capability vidcap;
    struct video_window vidwin;
    struct video_picture vidpic;
    struct video_clip vidclips[32];

    char *switchtwo = "";
    char *switchthree = "";
    char *switchfour = "";

    memset(vidclips, 0, sizeof(struct video_clip) * 32);

    if(argc == 1)
    {
	printf(" *** Usage ***\n");
	printf("./testquickcam DEVICE [ -r | -m | -l ]\n\n");
	printf(" -r reads one frame via read() from the camera\n");
	printf(" -m reads one frame via mmap() from the camera\n");
	printf(" -l read() loop...good for debugging gain etc \n");
	exit(1);
    }

    if(argc >= 3)
        switchtwo = argv[2];

    if(argc >= 4)
        switchthree = argv[3];

    if(open_camera(argv[1],&device_fd) == 1)
    {
	get_camera_info(device_fd, &vidcap, &vidwin, &vidpic, vidclips);
	print_camera_info(&vidcap, &vidwin, &vidpic);
	if(strcmp(switchtwo, "-r") == 0)
	    read_test(device_fd, &vidcap, 0);

	if(strcmp(switchtwo, "-m") == 0)
	    device_fd = mmap_test(device_fd, &vidcap, 1);

	if(strcmp(switchtwo, "-l") == 0)
	    read_loop(device_fd, &vidcap);

	if(strcmp(switchthree, "-r") == 0)
	    read_test(device_fd, &vidcap, 0);

	if(strcmp(switchthree, "-m") == 0)
	    device_fd = mmap_test(device_fd, &vidcap, 0);

	if(strcmp(switchthree, "-l") == 0)
	    read_loop(device_fd, &vidcap);

	if(strcmp(switchfour, "-r") == 0)
	    read_test(device_fd, &vidcap, 0);

	if(strcmp(switchfour, "-m") == 0)
	    device_fd = mmap_test(device_fd, &vidcap, 0);

	if(strcmp(switchfour, "-l") == 0)
	    read_loop(device_fd, &vidcap);

	if (device_fd) close_camera(device_fd);
    }
    exit(1);
}

