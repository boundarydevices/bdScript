/*
 * voQueue.c
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "voQueue.h"
extern "C" {
#include "mpeg2dec/convert.h"
};

static void fb_draw_frame16( vo_instance_t * _instance,
			     uint8_t * const * buf, void * id)
{
   voQueue_t * instance = (voQueue_t *)_instance ;
   unsigned char *dest = instance->pixMem_ ;
   unsigned char const *src = buf[0];

   unsigned numBytes = instance->numRows_*instance->stride_ ;
/*
   printf( "draw %x bytes into %p from %p\n", numBytes, instance->pixMem_, src ); fflush( stdout );
   printf( "first %u, last %u\n", src[0], src[numBytes-1] ); // buf[0][numBytes-1] );
*/   
   memcpy( instance->pixMem_, src, numBytes );
}

static int fb_setup( vo_instance_t * _instance, 
                     int width, int height,
		     vo_setup_result_t * result)
{
   voQueue_t * instance = (voQueue_t *) _instance;
   instance->pixMem_       = 0 ;
   instance->stride_       = width*sizeof( unsigned short );
   instance->pixelsPerRow_ = width ;
   instance->numRows_      = height ;
   instance->vo.draw       = fb_draw_frame16 ;
   result->convert         = convert_rgb16 ;

   return 0 ;
}

voQueue_t *voQueueOpen( void )
{
   voQueue_t * instance;
   instance = (voQueue_t *) malloc (sizeof (voQueue_t));
   if (instance == NULL)
      return NULL;
   instance->pixMem_       = 0 ;
   instance->stride_       = 0 ;
   instance->pixelsPerRow_ = 0 ;
   instance->numRows_      = 0 ;
   instance->vo.setup      = fb_setup;
   instance->vo.setup_fbuf = NULL ; // fb_setup_fbuf;
   instance->vo.set_fbuf   = NULL;
   instance->vo.start_fbuf = NULL ;
   instance->vo.discard    = NULL ;
   instance->vo.draw       = NULL ;
   instance->vo.close      = NULL ;
   
   return instance;
}

