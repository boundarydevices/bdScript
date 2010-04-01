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

int main(int argc, char **argv)
{
    if( 1 < argc ){
        int fd = open(argv[1],O_RDWR | O_NONBLOCK, 0);
        if( 0 <= fd ){
            int ipu_ch ;
            int rv = ioctl(fd,MXCFB_GET_FB_IPU_CHAN,&ipu_ch);
            if( 0 == rv ){
                printf("ipu_ch(%s) == %d, 0x%x\n", argv[1],ipu_ch,ipu_ch);
                unsigned shift = 24 ;
                unsigned mask = 0x3f << shift ;
                while( mask ){
                    printf( "%2d  ", (ipu_ch & mask)>>shift );
                    shift -= 6 ;
                    mask >>= 6 ;
                }
                printf( "\n");
            }
            else
                perror("MXCFB_GET_FB_IPU_CHAN");
            close(fd);
        }
        else
            perror(argv[1]);
    }
    else
        fprintf(stderr,"Usage: %s /dev/fb0\n", argv[0]);

    return 0 ;
}

