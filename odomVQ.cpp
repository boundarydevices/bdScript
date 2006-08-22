/*
 * Module odomVQ.cpp
 *
 * This module defines the methods of the odometerVideoQueue_t
 * class as declared in odomVQ.h
 *
 *
 * Change History : 
 *
 * $Log: odomVQ.cpp,v $
 * Revision 1.2  2006-08-22 15:51:03  ericn
 * -remove GOP pseudo-pic from mpegDecode
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomVQ.h"
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "tickMs.h"
#include <stdio.h>
#include "debugPrint.h"
#include "odomPlaylist.h"

#define BADPTS (0LL)

#define INITTING      1
#define PULLING       2
#define PUTTING       4
#define PUTTINGFULL   8
unsigned long volatile trace_ = 0 ;

odometerVideoQueue_t::odometerVideoQueue_t()
   : frameMem_( 0 )
   , frameSize_( 0 )
   , dropped_( 0 )
   , addFree_( 0 )
   , takeFree_( 0 )
   , addFull_( 0 )
   , takeFull_( 0 )
   , firstPTS_( BADPTS )
   , flags_( 0 )
{
}

odometerVideoQueue_t::~odometerVideoQueue_t( void )
{
   flags_ |= HEAPINUSE ;
   if( frameMem_ )
      free( frameMem_ );
}

void odometerVideoQueue_t::init( unsigned w, unsigned h )
{
   flags_ |= HEAPINUSE ;
trace_ |= INITTING ;
   unsigned yuvSize = w*h*2 ;
   if( yuvSize != frameSize_ ){
      frameSize_ = yuvSize ;
      if( frameMem_ )
         free( frameMem_ );
      frameMem_ = malloc(yuvSize*MAXFRAMES-1);
   }

   takeFree_ = 0 ;
   addFree_  = MAXFRAMES-1 ;

   unsigned char *next = (unsigned char *)frameMem_ ;
   for( unsigned f = 0 ; f < MAXFRAMES-1 ; f++ ){
      freeFrames_[f].data_ = next ;
      next += yuvSize ;
   }

   addFull_ = 
   takeFull_ = 0 ;
   
   firstPTS_ = BADPTS ;
trace_ &= ~INITTING ;
   flags_ &= ~HEAPINUSE ;
}

unsigned char *odometerVideoQueue_t::pullEmpty( void )
{
   flags_ |= HEAPINUSE ;
trace_ |= PULLING ;
   unsigned char *rval = 0 ;
   if( addFree_ != takeFree_ ){
      rval = freeFrames_[takeFree_].data_ ;
      takeFree_ = (takeFree_+1) % MAXFRAMES ;
   }
trace_ &= ~PULLING ;
   flags_ &= ~HEAPINUSE ;
   return rval ;
}

void odometerVideoQueue_t::putFull( unsigned char *frame, long long pts )
{
   flags_ |= HEAPINUSE ;
trace_ |= PUTTINGFULL ;

   entry_t &entry = fullFrames_[addFull_];
   entry.data_ = frame ;
   entry.pts_  = pts + firstPTS_;

   addFull_ = ( addFull_ + 1 ) % MAXFRAMES ;

   assert( addFull_ != takeFull_ );

trace_ &= ~PUTTINGFULL ;
   flags_ &= ~HEAPINUSE ;
}

void odometerVideoQueue_t::start( void ) // first pts is calculated here
{
   flags_ |= HEAPINUSE ;
   assert( addFull_ != takeFull_ );

   long long const now = tickMs();
   long long const nowOffs = now - fullFrames_[takeFull_].pts_ ;

   // adjust previously 
   for( unsigned i = takeFull_ ; i != addFull_ ; i = (i+1)%MAXFRAMES )
      fullFrames_[i].pts_ += nowOffs ;

   firstPTS_ = nowOffs ;
   
   flags_ &= ~HEAPINUSE ;
}

bool odometerVideoQueue_t::started( void ) const {
   return BADPTS != firstPTS_ ;
}

unsigned char *odometerVideoQueue_t::pullHeap( 
   long long  now, 
   long long *pulledPts 
)
{
   unsigned char *rval = 0 ;
   if( addFull_ != takeFull_ ){
      long long pts = fullFrames_[takeFull_].pts_ ;
      long long diff = pts-now ;
      if( 0 >= diff ){
         rval = fullFrames_[takeFull_].data_ ;
         takeFull_ = (takeFull_ + 1)%MAXFRAMES ;
      } // time to display this one
   }

   return rval ;
}

extern unsigned maxSigDepth ;

unsigned char *odometerVideoQueue_t::pull( 
   unsigned   vsync,
   long long *pulledPts )
{
   long long prevPts = 0LL ;
   unsigned char *prev = 0 ;

   if( (0 == (flags_ & HEAPINUSE) )
       &&
       ( BADPTS != firstPTS_ ) ){
      flags_ |= HEAPINUSE ;
      long long nextPts ;
      unsigned char *next ;
      long long now = tickMs();

      while( 0 != ( next = pullHeap( now, &nextPts ) ) ){
         if( prev ){
            printf( "!!!!!!!!!!!!!!!!!!\n"
                    "!!!!!!!!!!!!!!!!!!\n"
                    "!!!!!!!!!!!!!!!!!!\n"
                    "drop %p, next == %p\n"
                    "now %llu, next %llu\n"
                    "add %u, take %u\n", 
                    prev, next,
                    now, nextPts,
                    addFree_, takeFree_ );
if( prev == next ){
      fprintf( stderr, "Bad drop: %u/%u\n",
               addFree_, takeFree_ );
      fprintf( stderr, "max sig depth: %u\n", maxSigDepth );
      for( unsigned i = 0 ; i < MAXFRAMES+1 ; i++ )
         fprintf( stderr, "   [%u] = %p\n", i, freeFrames_[i].data_ );
      fprintf( stderr, "mem %p/%lu bytes\n", frameMem_, frameSize_ );
      fprintf( stderr, "dropped %lu\n", dropped_ );
      fprintf( stderr, "firstPTS: %llu\n", firstPTS_ );
      fprintf( stderr, "trace: %lx\n", trace_ );
      fprintf( stderr, "%u entries on heap\n", (addFull_-takeFull_)%MAXFRAMES );
      fprintf( stderr, "%u -> %u\n", takeFull_, addFull_ );
      dumpHeap();

      if( lastPlaylistInst_ ){
         lastPlaylistInst_->dump();
      }
      exit(1);
}
            ++dropped_ ;
            putEmpty(prev);
            prev = 0 ;
         } // drop the previous one
         prev = next ;
         prevPts = nextPts ;
      }
      flags_ &= ~HEAPINUSE ;
   } // we've started

   if( prev && ( 0 != pulledPts ) ){
      *pulledPts = prevPts ;
   }

   return prev ;
}

void odometerVideoQueue_t::putEmpty( unsigned char *frame )
{
   flags_ |= HEAPINUSE ;
trace_ |= PUTTING ;
   freeFrames_[addFree_].data_ = frame ;
   addFree_ = ( addFree_ + 1 )%MAXFRAMES;

   if( addFree_ == takeFree_ ){
      fprintf( stderr, "Free frame overflow: %u/%u/%p\n",
               addFree_, takeFree_, frame );
      fprintf( stderr, "max sig depth: %u\n", maxSigDepth );
      for( unsigned i = 0 ; i < MAXFRAMES ; i++ )
         printf( "   [%u] = %p\n", i, freeFrames_[i].data_ );
      fprintf( stderr, "mem %p/%lu bytes\n", frameMem_, frameSize_ );
      fprintf( stderr, "dropped %lu\n", dropped_ );
      fprintf( stderr, "firstPTS: %llu\n", firstPTS_ );
      fprintf( stderr, "trace: %lx\n", trace_ );
      fprintf( stderr, "%u entries on heap\n", (addFull_-takeFull_)%MAXFRAMES );
      fprintf( stderr, "%u -> %u\n", takeFull_, addFull_ );
      dumpHeap();

      if( lastPlaylistInst_ ){
         lastPlaylistInst_->dump();
      }
      exit(1);
   }
trace_ &= ~PUTTING ;
   flags_ &= ~HEAPINUSE ;
}

void odometerVideoQueue_t::dumpHeap(void){
   for( unsigned i = takeFull_ ; i != addFull_ ; i = (i+1)%MAXFRAMES ){
      printf( "%u - %llu:%p\n", i, fullFrames_[i].pts_, fullFrames_[i].data_ );
   }
}

#ifdef MODULETEST

#include "mpDemux.h"
#include "memFile.h"

static char const * const frameTypes_[] = {
   "video",
   "audio",
   "endOfFile"
};

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>
#include "tickMs.h"
#include <termios.h>
#include <linux/sm501yuv.h>
#include <mpeg2dec/mpeg2.h>
#include "mpegDecode.h"
#include <stdio.h>
#include <linux/sm501yuv.h>
#include <sys/ioctl.h>
#include <linux/sm501-int.h>
#include <fcntl.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      unsigned xPos = 0 ;
      unsigned yPos = 0 ;
      unsigned outWidth  = 0 ;
      unsigned outHeight = 0 ;
      unsigned picTypeMask = (unsigned)-1 ;
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
                  if( 6 < argc ){
                     picTypeMask = (unsigned)strtoul( argv[6], 0, 0 );
                     printf( "picTypeMask == 0x%08X\n", picTypeMask );
                  }
               }
            }
         }
      }

      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         unsigned long numPackets[2] = { 0, 0 };
         unsigned long numBytes[2] = { 0, 0 };
         unsigned long crc[2] = { 0, 0 };

         mpegDemux_t demuxer( fIn.getData(), fIn.getLength() );

         memset( &numPackets, 0, sizeof( numPackets ) );
         memset( &numBytes, 0, sizeof( numBytes ) );
         memset( &crc, 0, sizeof( crc ) );
         printf( "reading in bulk\n" );
         mpegDemux_t::bulkInfo_t const * const bi = demuxer.getFrames();
         printf( "%u streams\n", bi->count_ );

         int videoIdx = -1 ;

         for( unsigned char sIdx = 0 ; sIdx < bi->count_ ; sIdx++ )
         {
            mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[sIdx];
            mpegDemux_t::frameType_e ft = sAndF.sInfo_.type ;
            printf( "%s stream %u: %u frames\n", frameTypes_[ft], sIdx, sAndF.numFrames_ );
            if( mpegDemux_t::videoFrame_e == ft )
               videoIdx = sIdx ;
            else if( mpegDemux_t::audioFrame_e == ft )
            {
               if( 0 < sAndF.numFrames_ )
               {
                  printf( "   start: %llu, end %llu\n", 
                          sAndF.frames_[0].when_ms_,
                          sAndF.frames_[sAndF.numFrames_-1].when_ms_ );
               }
            }
            numPackets[ft] += sAndF.numFrames_ ;
         }
         
         if( 0 <= videoIdx )
         {   
            struct termios oldTermState ;
            tcgetattr( 0, &oldTermState );
            struct termios raw = oldTermState ;
            raw.c_lflag &= ~(ICANON | ECHO | ISIG );
            tcsetattr( 0, TCSANOW, &raw );
            fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

            odometerVideoQueue_t vq ;

            mpegDecoder_t decoder ;
            mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[videoIdx];
            mpegDemux_t::frame_t const *next = sAndF.frames_ ;
            int numLeft = sAndF.numFrames_ ;
            unsigned numPictures = 0 ;
            long long startMs = tickMs();
            bool haveHeader = false ;
            unsigned long bytesPerPicture = 0 ;
            int const fdYUV = open( "/dev/yuv", O_WRONLY );
            if( 0 > fdYUV ){
               perror( "/dev/yuv" );
               return -1 ;
            }
            char keyvalue = '\0' ;

            do {
               long long pts = next->when_ms_ ;
               decoder.feed( next->data_, next->length_ );
               next++ ;
               numLeft-- ;
               void const *picture ;
               mpegDecoder_t::picType_e picType ;
               if( !haveHeader )
               {
                  haveHeader = decoder.haveHeader();
                  if( haveHeader )
                  {
                     bytesPerPicture = decoder.width() * decoder.height() * 2;
                     printf( "%u x %u: %lu bytes per picture\n", 
                             decoder.width(), decoder.height(), bytesPerPicture );
                     struct sm501yuvPlane_t plane ;
                     plane.xLeft_     = xPos ;
                     plane.yTop_      = yPos ;
                     plane.inWidth_   = decoder.width();
                     plane.inHeight_  = decoder.height();
                     plane.outWidth_  = ( 0 == outWidth ) ? decoder.width() : outWidth ;
                     plane.outHeight_ = ( 0 == outHeight ) ? decoder.height() : outHeight ;
                     if( 0 != ioctl( fdYUV, SM501YUV_SETPLANE, &plane ) )
                     {
                        perror( "setPlane" );
                        return 0 ;
                     }
                     else {
                        printf( "setPlane success\n" );
                        vq.init( decoder.width(), decoder.height() );
                     }
                  }
                  else
                     printf( "not yet\n" );
               }

               while( decoder.getPicture( picture, picType ) )
               {
printf( "%c - %llu\n", decoder.getPicType(picType), pts );

                  ++numPictures ;
                  if( haveHeader )
                  {
                     if( picTypeMask & ( 1 << picType ) ){
                        unsigned char *out = vq.pullEmpty();
                        if( !out ){
                           if( !vq.started() ){
                              vq.start();
                              printf( "start: %llu\n", vq.startPTS() );
                              startMs = tickMs();
                           }
   
                           // empty one
                           long long pulledPTS ;
                           unsigned char *filled ;
                           do {
                              filled = vq.pull(0,&pulledPTS);
                              if( 0 == filled ){
                                 usleep(1);
                              }
                              else
                                 break ;
                           } while( 1 );

                           printf( "%llu\n", pulledPTS );
                           int const numWritten = write( fdYUV, filled, bytesPerPicture );
                           if( (unsigned)numWritten != bytesPerPicture )
                           {
                              printf( "write %d of %lu bytes\n", numWritten, bytesPerPicture );
                              numLeft = 0 ;
                              return -1 ;
                           }

                           vq.putEmpty(filled);
                           out = vq.pullEmpty();
                           if( 0 == out ){
                              printf( "Error clearing data\n" );
                              return -1 ;
                           }
                        }

                        memcpy( out, picture, bytesPerPicture );
                        vq.putFull( out, pts );

                        while( ( 'g' != keyvalue ) 
                               && 
                               ( '\x03' != keyvalue ) 
                               && 
                               ( 0 >= read(0,&keyvalue,1) ) )
                           ;

                        keyvalue = tolower( keyvalue );
                        if( ( 'x' == keyvalue ) || ( '\x03' == keyvalue ) )
                        {
                           numLeft = 0 ;
                           break;
                        }
                     }
                     else
                        printf( "skip\n" );
                  }
                  else
                     printf( "picture without header\n" );
               }
            } while( 0 < numLeft );

            while( !vq.isEmpty() ){
               long long pulledPTS ;
               unsigned char *filled = vq.pull(0,&pulledPTS);
               if( 0 == filled ){
                  usleep(1);
               }
               else {
                  int const numWritten = write( fdYUV, filled, bytesPerPicture );
                  if( (unsigned)numWritten != bytesPerPicture )
                  {
                     printf( "write %d of %lu bytes\n", numWritten, bytesPerPicture );
                     numLeft = 0 ;
                     return -1 ;
                  }
                  vq.putEmpty(filled);
               }
            }

            long long endMs = tickMs();
            
            while( ( '\x03' != keyvalue ) 
                   && 
                   ( 0 >= read(0,&keyvalue,1) ) )
               ;

            if( 0 <= fdYUV )
               close( fdYUV );

            unsigned elapsed = (unsigned)( endMs-startMs );
            printf( "%u pictures, %u ms\n", numPictures, elapsed );
            if( decoder.haveHeader() )
            {
            }
            
            fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~O_NONBLOCK);
            tcsetattr( 0, TCSANOW, &oldTermState );
         }

         printf( "done\ndeleting\n" );
         bi->clear( bi );
         printf( "done\n" );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: mpegDecode fileName [x [,y [,width [,height]]]]\n" );

   return 0 ;
}


#endif
