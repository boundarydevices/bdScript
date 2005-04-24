/*
 * Module mp3Queue.cpp
 *
 * This module defines the methods of the mp3Queue_t
 * class as declared in mp3Queue.h
 *
 *
 * Change History : 
 *
 * $Log: mp3Queue.cpp,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "mp3Queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "tickMs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// #define DEBUGPRINT 1
#include "debugPrint.h"

mp3Queue_t :: page_t *mp3Queue_t :: allocPage( void )
{
   page_t *p = (page_t *)malloc( offsetof(page_t,data_)+fragSize_ );
   p->max_ = fragSize_ ;
   p->numFilled_ = 0 ;
   p->next_ = 0 ;
   return p ;
}

mp3Queue_t :: mp3Queue_t( int fdAudio )
   : fdAudio_( fdAudio )
   , fragSize_( 0 )
   , numFrags_( 0 )
   , bytesQueued_( 0 )
   , started_( false )
   , pcmHead_( 0 )
   , pcmTail_( 0 )
   , free_( 0 )
{
   if( 0 == ioctl( fdAudio_, SNDCTL_DSP_GETOSPACE, &ai_) ) 
   {
debugPrint( "read OSPACE: %d frags, %d total, %d bytes/frag, %d bytes avail\n",
            ai_.fragments, ai_.fragstotal, ai_.fragsize, ai_.bytes );

      fragSize_ = ai_.fragsize ;
      numFrags_ = ai_.fragstotal*8 ;
      for( unsigned f = 0 ; f < numFrags_ ; f++ )
      {
         page_t *page = allocPage();
         page->next_ = free_ ;
         free_ = page ;
      }
   }
   else
   {
      fprintf( stderr, "ioctl(GETOSPACE):%m\n" );
      exit(3);
   }
}

mp3Queue_t :: ~mp3Queue_t( void )
{
   freePages( pcmHead_ );
   freePages( free_ );
}

void mp3Queue_t :: queueData
   ( void const *mp3Data, 
     unsigned    numBytes,
     long long   whenMs )
{
   decoder_.feed( mp3Data, numBytes );
// debugPrint( "feed %u bytes\n", numBytes );

   while( decoder_.getData() )
   {
      while( 0 < decoder_.numSamples() )
      {
// debugPrint( "%u samples in decoder\n", decoder_.numSamples() );
         //
         // where does data go?
         //    
         //
         page_t *page = pcmTail_ ;
         if( ( 0 == page ) || ( page->numFilled_ == page->max_ ) )
         {
            if( free_ )
            {
               page = free_ ;
               free_ = page->next_ ;
               page->next_      = 0 ;
               page->numFilled_ = 0 ;
            }
            else
               page = allocPage();
   
            if( pcmTail_ )
            {
               pcmTail_->next_ = page ;
               pcmTail_ = page ;
            }
            else
               pcmHead_ = pcmTail_ = page ;
         }

         unsigned availBytes = page->max_ - page->numFilled_ ;
         unsigned availSamples = ( availBytes / sizeof( unsigned short ) );
         if( 0 < availSamples ) 
         {
debugPrint( "%u bytes/%u samples in page\n", availBytes, availSamples );
            unsigned short *samples = (unsigned short *)( page->data_ + page->numFilled_ );
            unsigned numRead ;
            if( decoder_.readSamples( samples, availSamples, numRead ) )
            {
               unsigned numFilled = numRead * sizeof(samples[0]);
               
               page->numFilled_ += numFilled ;
               bytesQueued_ += numFilled ;
debugPrint( "filled %u/%u\n", numRead, numFilled );
debugPrint( "pageCount %u\n", numPages() );
            }
         }
         else
         {
            fprintf( stderr, "Huh?\n" );
            exit(3);
         }
      }
   }

   if( started_ )
      poll();
}
   
bool mp3Queue_t :: queueIsFull( void )
{
   return 0 == free_ ;
}

long long mp3Queue_t :: start( void )
{
   long long bufferTime = 0LL ;

   started_ = true ;
   
   unsigned nChannels = decoder_.numChannels();
   unsigned sampleRate = decoder_.sampleRate();
   
   int const format = AFMT_S16_LE ;
   if( 0 != ioctl( fdAudio_, SNDCTL_DSP_SETFMT, &format) ) 
      fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
   
   if( 0 == ioctl( fdAudio_, SNDCTL_DSP_CHANNELS, &nChannels ) )
      debugPrint( "%u channels\n", nChannels );
   else
      fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );

   if( 0 == ioctl( fdAudio_, SNDCTL_DSP_SPEED, &sampleRate ) )
      debugPrint( "%u samples/second\n", sampleRate );
   else
      fprintf( stderr, "Error setting sampling rate to %d:%m\n", sampleRate );                              
   
   poll();
   
   return tickMs();
}

void mp3Queue_t :: poll( void )
{
   if( started_ )
   {
      if( 0 == ioctl( fdAudio_, SNDCTL_DSP_GETOSPACE, &ai_) ) 
      {
         if( ai_.fragments >= ai_.fragstotal-1 )
            fprintf( stderr, "Stall: %lu bytes\n", bytesQueued() );
         while( 0 < ai_.fragments )
         {
            page_t *page = pcmHead_ ;
            if( 0 != page )
            {
               pcmHead_ = page->next_ ;
               if( 0 == pcmHead_ )
                  pcmTail_ = 0 ;

               int numWritten = write( fdAudio_, page->data_, page->numFilled_ );
               if( numWritten != page->numFilled_ )
                  perror( "writeAudio" );

               bytesQueued_ -= page->numFilled_ ;
               page->next_ = free_ ;
               free_ = page ;
               ai_.fragments-- ;
            }
            else
            {
//               debugPrint( "no data\n" );
               break;
            }
         }
      }
      else
         perror( "OSPACE" );
   }
}

void mp3Queue_t :: freePages( page_t *head )
{
   while( head )
   {
      page_t *tmp = head ;
      head = tmp->next_ ;
      free( tmp );
   }
}

bool mp3Queue_t :: done( void )
{
   if( 0 == pcmHead_ )
   {
      if( 0 == ioctl( fdAudio_, SNDCTL_DSP_GETOSPACE, &ai_) ) 
      {
//         debugPrint( "%d of %d frags avail\n", ai_.fragments, ai_.fragstotal );
         return ai_.fragments >= ai_.fragstotal-1 ;
      }
      else
      {
         perror( "GETOSPACE" );
         return true ;
      }
   }
//   else
//      debugPrint( "data still queued\n" );

   return false ;
}

unsigned mp3Queue_t :: numPages( void ) const 
{
   unsigned count = 0 ;
   page_t *tmp = pcmHead_ ;
   while( tmp )
   {
      ++count ;
      tmp = tmp->next_ ;
   }
   tmp = free_ ;
   while( tmp )
   {
      ++count ;
      tmp = tmp->next_ ;
   }

   return count ;
}


#ifdef MODULETEST
#include <poll.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      int const fIn = open( argv[1], O_RDONLY );
      if( 0 <= fIn )
      {
         fprintf( stderr, "%s opened\n", argv[1] );
         
         int const fdAudio = open( "/dev/dsp", O_WRONLY );
         
         if( 0 <= fdAudio )
         {
            fprintf( stderr, "opened audio device\n" );
            
            mp3Queue_t mp3Q( fdAudio );
            unsigned long totalRead = 0 ;
            bool eof = false ;

            while( !( eof || mp3Q.queueIsFull() ) )
            {
               char inBuf[512];
               int numRead = read( fIn, inBuf, sizeof( inBuf ) );
               if( 0 < numRead )
               {
                  totalRead += numRead ;
                  mp3Q.queueData( inBuf, numRead, 0LL );
               }
               else
               {
                  debugPrint( "eof after %lu bytes\n", totalRead );
                  eof = true ;
               }
            }

            printf( "%u pages queued\n", mp3Q.numPages() );

            mp3Q.start();
            
            printf( "queue is full or eof after %lu bytes\n", totalRead );

            while( !mp3Q.done() )
            {
               if( !eof )
               {
                  if( !mp3Q.queueIsFull() )
                  {
                     char inBuf[512];
                     int numRead = read( fIn, inBuf, sizeof( inBuf ) );
                     if( 0 < numRead )
                     {
                        totalRead += numRead ;
                        mp3Q.queueData( inBuf, numRead, 0LL );
                     }
                     else
                     {
                        debugPrint( "eof after %lu bytes\n", totalRead );
                        perror( argv[1] );
                        eof = true ;
                     }
                  }
                  else
                  {
                     pollfd filedes ;
                     filedes.fd = fdAudio ;
                     filedes.events = POLLOUT ;
                     poll( &filedes, 1, 100 );
                  }
               }
               mp3Q.poll();

//               sleep( 1 );
            }

            debugPrint( "done with playback\n" );

/*
            unsigned totalOut = 0 ;
            mp3Queue_t::page_t *page = mp3Q.pcmHead_ ;
            while( page )
            {
               int const numWritten = write( fdAudio, page->data_, page->numFilled_ );
               totalOut += numWritten ;
               page = page->next_ ;
            }

            debugPrint( "%u bytes written\n", totalOut );

            while( 1 )
               sleep( 1 );
*/

            close( fdAudio );
         }
         else
            perror( "/dev/dsp" );
         
         close( fIn );
      }
      else
         perror( argv[1] );

   }
   else
      fprintf( stderr, "Usage: %s fileName\n", argv[0] );
   return 0 ;
}

#elif defined( MODULETEST2 )

#include "mpegStream.h"
#include "memFile.h"
#include "hexDump.h"
#include "yuvQueue.h"

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
#include "mpegDecode.h"
#include <sys/ioctl.h>
static char const cPicTypes[] = {
   'I', 'P', 'B', 'D'
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
         int const fdAudio = open( "/dev/dsp", O_WRONLY );
         
         if( 0 <= fdAudio )
         {
            mp3Queue_t mp3Q( fdAudio );
            mpegStream_t  stream ;
            mpegDecoder_t decoder ;
            yuvQueue_t   *yuvQueue = 0 ;
            unsigned numPictures = 0 ;
            long long startMs = tickMs();
            unsigned long bytesPerPicture = 0 ;
            int const fdYUV = open( "/dev/yuv", O_WRONLY );
   
            unsigned long globalOffs = 0 ;
            unsigned char inBuf[32768];
            int  numRead ;
            while( 0 < ( numRead = fread( inBuf, 1, sizeof(inBuf)/2, fIn ) ) )
            {
               unsigned                  inOffs = 0 ;
               
               mpegStream_t::frameType_e type ;
               unsigned                  frameOffs ;
               unsigned                  frameLen ;
               long long                 pts ;
               long long                 dts ;
               unsigned char             streamId ;
   
               while( ( numRead > inOffs )
                      &&
                      stream.getFrame( inBuf+inOffs, 
                                     numRead-inOffs,
                                     type,
                                     frameOffs,
                                     frameLen,
                                     pts, dts,
                                     streamId ) )
               {
                  unsigned end = inOffs+frameOffs+frameLen ;
                  if( end > numRead )
                  {
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
   
                  if( mpegStream_t::videoFrame_e == type )
                  {
                     unsigned char const *frameData = inBuf+inOffs+frameOffs ;
                     decoder.feed( frameData, frameLen );
                     
                     if( 0 == yuvQueue )
                     {
                        if( decoder.haveHeader() )
                        {
                           yuvQueue = new yuvQueue_t( decoder.width(), decoder.height(), 10 );
                           bytesPerPicture = decoder.width() * decoder.height() * 2;
                           printf( "%u x %u: %u bytes per picture\n", 
                                   decoder.width(), decoder.height(), bytesPerPicture );
                           yuvQueue_t::entry_t *entry = yuvQueue->getEmpty();
                           decoder.setPictureBuf( entry->data_ );
                           
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
                        }
                     }
                     
                     void const *picture ;
                     mpegDecoder_t::picType_e picType ;
                     while( decoder.parse( picType ) )
                     {
                        ++numPictures ;
                        if( yuvQueue )
                        {
                           unsigned long picPTS ;
                           void *picBuf = decoder.getPictureBuf(picPTS);
                           if( picBuf )
                           {
                              yuvQueue_t::entry_t *entry = yuvQueue_t::entryFromData(picBuf);
                              entry->when_ms_ = picPTS ;
                              entry->type_    = type ;
                              yuvQueue->putFull( entry );
   
                              entry = yuvQueue->getEmpty();
                              decoder.setPictureBuf( entry->data_ );
                           }
                           else
                              fprintf( stderr, "No decoder picture buf\n" );
                        }
                        else
                           printf( "picture without header\n" );
                     }
                  } // video frame
                  else if( mpegStream_t::audioFrame_e == type )
                  {
                     unsigned char const *frameData = inBuf+inOffs+frameOffs ;
                     mp3Q.queueData( frameData, frameLen, 0LL );

                     if( mp3Q.queueIsFull() && !mp3Q.started() )
                     {
                        mp3Q.start();
                     }
                  }
                  
                  if( mp3Q.started() )
                  {
                     mp3Q.poll();
                     yuvQueue_t::entry_t *entry = yuvQueue->getFull();
                     if( entry )
                     {
                        if( 0 <= fdYUV )
                        {
                           int const numWritten = write( fdYUV, entry->data_, bytesPerPicture );
                           if( numWritten != bytesPerPicture )
                           {
                              printf( "write %d of %u bytes\n", numWritten, bytesPerPicture );
                           }
                        }

                        yuvQueue->putEmpty( entry );
                     }
                  }
                  
                  inOffs += frameOffs+frameLen ;
               }
               globalOffs += numRead ;
            }
   
            long long endMs = tickMs();
            
            if( 0 <= fdYUV )
               close( fdYUV );
   
            unsigned elapsed = (unsigned)( endMs-startMs );
            printf( "%u pictures, %u ms\n", numPictures, elapsed );

            while( !mp3Q.done() )
            {
               mp3Q.poll();
            }
            
            endMs = tickMs();
            elapsed = (unsigned)( endMs-startMs );
            printf( "end of audio at %u ms\n", elapsed );
         }
         else
            perror( "/dev/dsp" );
         
         fclose( fIn );

      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: mpegDecode fileName [x [,y [,width [,height [,volume]]]]]\n" );

   return 0 ;
}
#endif

