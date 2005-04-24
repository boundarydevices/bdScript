/*
 * Module yuvQueue.cpp
 *
 * This module defines the methods of the yuvQueue_t
 * class as declared in yuvQueue.h
 *
 *
 * Change History : 
 *
 * $Log: yuvQueue.cpp,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "yuvQueue.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#define DEBUGPRINT 1
#include "debugPrint.h"
#include <linux/videodev.h>
#include <linux/sm501yuv.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/fb.h>
#include <string.h>
#include <errno.h>

#define NUMENTRIES 10

#ifdef _WIN32
#define I64FMT "%I64u"
#else
#define I64FMT "%llu"
#endif

#if 0
yuvQueue_t :: yuvQueue_t
   ( unsigned width,
     unsigned height,
     unsigned framesPerAlloc )
   : width_( width ),
     height_( height ),
     framesPerAlloc_( framesPerAlloc ),
     rowStride_( width*sizeof(unsigned short) ),
     entrySize_( (offsetof(entry_t,data_)+(((rowStride_*height+3)/4)*4)) ),
     memSize_( offsetof( page_t, entries_ ) + framesPerAlloc*entrySize_ ),
     discards_( 0 ),
     pages_( 0 ),
     nextEntry_( 0 ),
     minMs_( 0xffffffff ),
     maxMs_( 0 ),
{
   full_.next_  = full_.prev_ = &full_ ;
   empty_.next_ = empty_.prev_ = &empty_ ;
   
   debugPrint( "entrySize: %u, numEntries %u, memSize %u\n", entrySize_, framesPerAlloc, memSize_ );
   allocPage();
}

void yuvQueue_t :: allocPage( void )
{
   debugPrint( "allocPage()\n" );
   page_t *p = (page_t *)malloc( memSize_ );
   p->next_ = pages_ ;
   pages_ = p ;
   
   entry_t *nextEntry = p->entries_ ;
   entry_t *end = (entry_t *)( (char *)nextEntry + (framesPerAlloc_*entrySize_ ) );

   debugPrint( "yuvQueue: %u entries at %p(%u)\n", framesPerAlloc_, p, end-nextEntry );

   while( nextEntry < end )
   {
      putEmpty( nextEntry );
      nextEntry = (entry_t *)( (char *)nextEntry + entrySize_ );
   }

}

yuvQueue_t :: ~yuvQueue_t( void )
{
    while( pages_ )
    {
       page_t *p = pages_ ;
       pages_ = p->next_ ;
       free( (void *)p );
    }
}
#else

yuvQueue_t :: yuvQueue_t
   ( int      fd,
     unsigned inWidth,
     unsigned inHeight,
     unsigned xLeft,
     unsigned yTop,
     unsigned outWidth,
     unsigned outHeight )
   : fd_( fd )
   , width_( inWidth )
   , height_( inHeight )
   , rowStride_( inWidth*sizeof(unsigned short) )
   , entrySize_( 0 )
   , memSize_( 0 )
   , mem_( 0 )
   , frames_( 0 )
   , frameData_( new unsigned char [rowStride_*inHeight] )
   , startMs_( 0 )
{
   fcntl( fd, F_SETFD, FD_CLOEXEC );
   sm501yuvPlane_t plane ;
   plane.xLeft_     = xLeft ;
   plane.yTop_      = yTop ;
   plane.inWidth_   = inWidth ;
   plane.inHeight_  = inHeight ;
   plane.outWidth_  = outWidth ;
   plane.outHeight_ = outHeight ;
   if( 0 == ioctl( fd, SM501YUV_SETPLANE, &plane ) )
   {
      debugPrint( "set plane successfully\n" );
      struct video_mbuf vmm ;
      if( 0 == ioctl( fd, VIDIOCGMBUF, &vmm ) )
      {
         debugPrint( "read mbuf params\n" );
         debugPrint( "memSize: 0x%x\n", vmm.size );

         memSize_    = vmm.size ;
         entrySize_  = vmm.offsets[1]-vmm.offsets[0];
/*
         mem_ = mmap( 0, vmm.size, PROT_WRITE|PROT_WRITE, MAP_SHARED, fd, 0 );
         if( MAP_FAILED != mem_ )
         {
            frames_ = new void *[vmm.frames];
            char *const base = (char *)mem_ ;
            for( unsigned i = 0 ; i < vmm.frames ; i++ )
            {
               debugPrint( "[%2u] == %u\n", i, vmm.offsets[i] );
               frames_[i] = base + vmm.offsets[i];
            }
         }
         else
            perror( "yuvMap" );
*/            
      }
      else
         perror( "VIDIOCGMBUF" );
   }
   else
      perror( "yuvSetPlane" );
}

yuvQueue_t :: ~yuvQueue_t( void )
{
   if( mem_ )
      munmap( mem_, memSize_ );
   if( frames_ )
      delete [] frames_ ;
}

bool yuvQueue_t :: isFull( void )
{
   return 0 == getEmpty();
}

yuvQueue_t :: entry_t *yuvQueue_t :: getEmpty( void )
{
   unsigned long index ;
   if( 0 == ioctl( fd_, SM501YUV_GETBUF, &index ) )
   {
      entry_t &next = entries_[nextEntry_];
      next.idx_  = index ;
      next.data_ = (unsigned char *)frameData_ ; // ( frames_[index] );
//debugPrint( "have buffer %u/%p\n", index, entry_.data_ );
      return &next ;
   }
   else
   {
      return 0 ;
   }
}

void yuvQueue_t :: putFull
   ( entry_t *e, 
     long long whenMs,
     mpegDecoder_t::picType_e type )
{
   if( mpegDecoder_t::ptI_e == type )
   {
      if( 0 == startMs_ )
         startMs_ = minMs_ ;
      printf( "GOP: %lu-> %lu..%lu\n", startMs_, minMs_-startMs_, maxMs_-startMs_ );
      for( unsigned i = 0 ; i < nextEntry_ ; i++ )
      {
         e = entries_ + i ;
         sm501Buffer_t buf ;
         buf.whenMs_ = e->when_ ;
         buf.index_ = e->idx_ ;
         buf.data_  = e->data_ ;
         
         if( 0 != ioctl( fd_, SM501YUV_PUTBUF, &buf ) )
            perror( "PUTBUF" );

         double whenSec = ( buf.whenMs_-startMs_ )/1000.0 ;
         debugPrint( "put buffer %5u @ %lu (%2.3f)\n", 
                     buf.index_, buf.whenMs_, whenSec );
      }
      entries_[0] = entries_[nextEntry_];
      nextEntry_ = 0 ;
      minMs_ = 0xffffffff ;
      maxMs_ = 0 ;
   } // flush

   entries_[nextEntry_++].when_ = whenMs ;

   if( whenMs < minMs_ )
      minMs_ = whenMs ;
   if( whenMs > maxMs_ )
      maxMs_ = whenMs ;
/*
*/
}

#endif 

#ifdef MODULETEST

#include "mpegStream.h"
#include "memFile.h"
#include "hexDump.h"

static char const *cFrameTypes_[] = {
     "video"
   , "audio"
   , "other"
};

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>
#include "tickMs.h"
#include <termios.h>
#include <linux/sm501yuv.h>
#include <sys/poll.h>
#include "mpegDecode.h"
#include <sys/ioctl.h>
static char const cPicTypes[] = {
   'I', 'P', 'B', 'D'
};

static char const * const cStates[] = {
   "READDATA",
   "GETFRAME",
   "FINDHEADER",
   "FEEDDECODER",
   "PARSEDATA",
   "NEEDOSPACE"
};

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      unsigned xPos = 0 ;
      unsigned yPos = 0 ;
      unsigned outWidth  = 0 ;
      unsigned outHeight = 0 ;
      if( 2 < argc )
      {
         xPos = (unsigned)strtoul( argv[2], 0, 0 );
         if( 3 < argc )
         {
            yPos = (unsigned)strtoul( argv[3], 0, 0 );
            if( 4 < argc )
            {
               outWidth = (unsigned)strtoul( argv[4], 0, 0 );
               if( 5 < argc )
               {
                  outHeight = (unsigned)strtoul( argv[5], 0, 0 );
               }
            }
         }
      }
      FILE *fIn = fopen( argv[1], "rb" );
      if( fIn )
      {
         mpegStream_t  stream ;
         mpegDecoder_t decoder ;
         yuvQueue_t   *yuvQueue = 0 ;
         unsigned numPictures = 0 ;
         unsigned pixOut = 0 ;
         unsigned long bytesPerPicture = 0 ;
         int const fdYUV = open( "/dev/yuv", O_RDWR );

         unsigned long globalOffs = 0 ;
         unsigned char inBuf[4096];
         long long startMs  = tickMs() + 5000 ;
         long long startPTS = 0LL ;
         long long endPTS = 0LL ;
         long long startTime ;
         long long slowestRead = 0LL ;
         long long totalRead = 0LL ;
         unsigned maxLoop = 0 ;
         unsigned numReads = 0 ;
         int      numRead = 0 ;
         unsigned inOffs = 0 ;
         unsigned maxDecode = 0 ;
         unsigned maxDecodeIter = 0 ;
         unsigned maxWrite = 0 ;
         unsigned maxParseIters = 0 ;
         unsigned maxDecodeMask = 0 ;
         bool eof = false ;
         yuvQueue_t::entry_t *nextOut = 0 ;
         mpegStream_t::frameType_e frameType ;
         unsigned                  frameOffs ;
         unsigned                  frameLen ;
         long long                 pts, dts ;
         unsigned char             streamId ;
         mpegDecoder_t::picType_e picType ;

         enum {
            READDATA,
            GETFRAME,
            FINDHEADER,
            FEEDDECODER,
            PARSEDATA,
            NEEDOSPACE
         } state ;

         while( !eof )
         {
//            debugPrint( "state: %s\n", cStates[state] );
            switch( state )
            {
               case READDATA:
                  {
                     long long startRead = tickMs();
                     if( 0 < ( numRead = fread( inBuf, 1, sizeof(inBuf)/2, fIn ) ) ) 
                     {
                        long long const readTime = tickMs()-startRead ;
                        numReads++ ;
                        totalRead += readTime ;
                        if( readTime > slowestRead )
                           slowestRead = readTime ;
                        inOffs = 0 ;
                        globalOffs += numRead ;
                        state = GETFRAME ;
                     }
                     else
                        eof = true ;
                     break;
                  }
               case GETFRAME :
                  {
//                     debugPrint( "numRead: %u, inOffs %u\n", numRead, inOffs );

                     unsigned decodeMask = 0 ;
                     long long startDecode = tickMs();
                     if( ( numRead > inOffs )
                         &&
                         stream.getFrame( inBuf+inOffs, 
                                          numRead-inOffs,
                                          frameType,
                                          frameOffs,
                                          frameLen,
                                          pts, dts,
                                          streamId ) )
                     {
                        unsigned end = inOffs+frameOffs+frameLen ;
                        if( end > numRead )
                        {
                           decodeMask |= 1 ;
                           unsigned left = end-numRead ;
                           unsigned max = sizeof(inBuf)-numRead ;
                           if( max > left )
                           {
                              int numRead2 = fread( inBuf+numRead, 1, left, fIn );
                              if( numRead2 == left )
                              {
                                 fprintf( stderr, "tail end %u bytes\n", left );
                                 globalOffs += numRead2 ;
                              }
                              else
                              {
                                 fprintf( stderr, "packet underflow: %d of %u bytes read\n", numRead, left );
                                 return 0 ;
                              }
                           }
                           else
                           {
                              fprintf( stderr, "packet overflow: %lu bytes needed, %lu bytes avail\n", left, max );
                              return 0 ;
                           }
                        }

                        state = ( 0 == yuvQueue ) 
                                ? FINDHEADER 
                                : FEEDDECODER ;
                     }
                     else
                        state = READDATA ;

                     break;
                  }
               case FINDHEADER:
                  {
                     state = GETFRAME ;

                     if( mpegStream_t::videoFrame_e == frameType )
                     {
                        unsigned char const *frameData = inBuf+inOffs+frameOffs ;
                        if( 0 != pts )
                           decoder.setPTS( pts );
                        decoder.feed( frameData, frameLen );
                        
                        while( !decoder.haveHeader() && 
                               decoder.parse( picType ) )
                           ;

                        if( decoder.haveHeader() )
                        {
                           yuvQueue = new yuvQueue_t( fdYUV, 
                                                      decoder.width(), 
                                                      decoder.height(), 
                                                      xPos,
                                                      yPos,
                                                      ( 0 == outWidth ) ? decoder.width() : outWidth,
                                                      ( 0 == outHeight ) ? decoder.height() : outHeight );
                           nextOut = yuvQueue->getEmpty();
                           decoder.setPictureBuf( nextOut->data_ );
                           if( 0 == startPTS )
                           {
                              startMs = tickMs();
//                              yuvQueue->setStartMs( startMs );
                              startPTS = pts ;
                           }
                        }
                     } // video frame
                        
                     inOffs += frameOffs+frameLen ;
                     break ;
                  }
               case FEEDDECODER:
                  {
                     if( mpegStream_t::videoFrame_e == frameType )
                     {
                        unsigned char const *frameData = inBuf+inOffs+frameOffs ;
                        if( 0 != pts )
                           decoder.setPTS( pts );
                        decoder.feed( frameData, frameLen );

                        state = PARSEDATA ;
                     }
                     else
                        state = GETFRAME ;
                     
                     inOffs += frameOffs+frameLen ;
                     break ;
                  }

               case PARSEDATA:
                  {
                     state = GETFRAME ;

                     void const *picture ;
                     long long startDecode = tickMs();
                     if( decoder.parse( picType ) )
                     {
                        unsigned decodeTime = tickMs()-startDecode ;
                        if( decodeTime > maxDecodeIter )
                           maxDecodeIter = decodeTime ;
                        unsigned long picPTS ;
                        void *picBuf = decoder.getPictureBuf( picPTS );
                        if( picBuf )
                        {
                           endPTS = picPTS ;

                           decoder.setPictureBuf( 0 );

unsigned long when = (picPTS/90);
printf( "%lu - %u - %c\n", when, nextOut->idx_, cPicTypes[picType] );

                           yuvQueue->putFull( nextOut, startMs+when, picType );
/*
char kBuf[80];
gets( kBuf );
*/
                           state = NEEDOSPACE ;
                        }
                        else
                           fprintf( stderr, "No decoder picture buf\n" );
                     } // got a picture

                     break;
                  }
               case NEEDOSPACE:
                  {
                     nextOut = yuvQueue->getEmpty();
                     if( 0 == nextOut )
                     {
                        pollfd filedes ;
                        filedes.fd = fdYUV ;
                        filedes.events = POLLOUT ;
                        int result = poll( &filedes, 1, 10000 );
                        if( 0 < result )
                           debugPrint( "pollin\n" );
                        else
                        {
                           fprintf( stderr, "stall:%m\n" );
                           return 0 ;
                        }
                     } // queue is full... push data out
                     else
                     {
                        decoder.setPictureBuf( nextOut->data_ );
                        state = PARSEDATA ;
                     }
                     break;
                  }
            }
         }

         long long endMs = tickMs();

         if( 0 <= fdYUV )
            close( fdYUV );

         unsigned elapsed = (unsigned)( endMs-startMs );
         printf( "%u pictures (%u out), %u ms\n", numPictures, pixOut, elapsed );
         printf( "PTS: start %lld, end %lld, duration %lldms\n", 
                 startPTS, endPTS, endPTS-startPTS );
         printf( "slowest read %lld, total %lld, count %u, avg %lld\n", slowestRead, totalRead, numReads, totalRead/numReads );
         printf( "maxLoop: %u, maxDecode %u, maxWrite %u\n", maxLoop, maxDecode, maxWrite );
         printf( "maxParseIters %u, maxIter %u, decodeMask = %x\n", maxParseIters, maxDecodeIter, maxDecodeMask );
         fclose( fIn );

      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: mpegDecode fileName [x [,y [,width [,height]]]]\n" );

   return 0 ;
}
#endif

