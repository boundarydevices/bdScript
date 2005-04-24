/*
 * Module mpegPlay.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: mpegPlay.cpp,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "mp3Queue.h"
#include <stdio.h>
#include "rawKbd.h"
#include "mpegStream.h"
#include "yuvDev.h" 
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

// #define DEBUGPRINT
#include "debugPrint.h"
#include "mpegYUV.h"
#include "mpegFile.h"

static void interleaveYUV
   ( int                  width, 
     int                  height, 
     unsigned char       *yuv, 
     unsigned char const *y, 
     unsigned char const *u, 
     unsigned char const *v )
{
   unsigned char const * const startU = u ;
   for( unsigned row = 0 ; row < height ; row++ )
   {
      unsigned char const *uRow = u ;
      unsigned char const *vRow = v ;
      for( unsigned col = 0 ; col < width ; col++ )
      {
         *yuv++ = *y++ ;
         if( 0 == ( col & 1 ) )
            *yuv++ = *uRow++ ;
         else
            *yuv++ = *vRow++ ;

//         yuv += 2 ;
      }
      
      if( 1 == ( row & 1 ) )
      {
         u += width/2 ;
         v += width/2 ;
      }
   }

}
               

unsigned const AUDIOLOWWATER  = (1<<18);
unsigned const AUDIOHIGHWATER = (1<<19);

int main( int argc, char const * const argv[] )
{
   debugPrint( "Hello, %s\n", argv[0] );
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

      mpegFile_t fIn( argv[1] );
      if( fIn.isOpen() )
      {
         yuvDev_t     yuvDev ;
         if( yuvDev.isOpen() )
         {
            int const fdAudio = open( "/dev/dsp", O_WRONLY );
            if( 0 <= fdAudio )
            {
               mp3Queue_t    mp3Out( fdAudio );
               rawKbd_t      kbd ;
               mpegYUV_t     decoder ;
               mpegStream_t  stream ;
               long long     headerTick = 0 ;
               bool          waitVideo = false ;
               unsigned      prevMask = 0 ;

               while( !fIn.endOfFile() )
               {
                  unsigned mask = 0 ;
                  if( ( !mp3Out.started() )     // force read if < low water mark
                      ||
                      ( AUDIOHIGHWATER > mp3Out.bytesQueued() ) )
                      mask |= mpegStream_t::audioFrame_e ;
/*
*/

                  if( ( ( mp3Out.started()
                          &&
                          ( AUDIOLOWWATER < mp3Out.bytesQueued() ) ) // only if audio is okay
                        ||
                        mp3Out.done() )
                      &&
                      ( decoder.needData == decoder.getState() ) )
                     mask |= mpegStream_t::videoFrame_e ;

                  if( mask != prevMask )
                  {
                     printf( "mask 0x%x\n", mask );
                     prevMask = mask ;
                  }

                  if( 0 != mask )
                  {
//                     mask |= mpegStream_t::videoFrame_e ;
                     mask |= mpegStream_t::otherFrame_e ; // always skip other junk
                     mask |= mpegStream_t::audioFrame_e ; // always accept audio data
                     
                     unsigned char *frame = 0 ;
                     mpegStream_t::frameType_e type ;
                     unsigned frameLen ;
                     long long pts ;
                     long long dts ;
                     unsigned char streamId ;

                     if( fIn.getFrame( mask, 
                                       type, frame, frameLen, pts, dts, streamId ) )
                     {
debugPrint( "frame:%u bytes, %llu\n", frameLen, pts );
                        if( mpegStream_t::videoFrame_e == type )
                        {
                           assert( decoder.needData == decoder.getState() );
                           decoder.feed( frame, frameLen, pts );
                        } // video frame
                        else if( mpegStream_t::audioFrame_e == type )
                        {
printf( "audioFrame:%u\n", frameLen );
                           mp3Out.queueData( frame, frameLen, 0 );
                           if( ( AUDIOHIGHWATER <= mp3Out.bytesQueued() )
                               && 
                               !mp3Out.started() )
                              mp3Out.start();
                        }
                        else
                           fprintf( stderr, "other frame type\n" );
                     }
                  }
                  else if( waitVideo )
                  {
                     pollfd filedes[3];
                     filedes[0].fd = yuvDev.getFd() ;
                     filedes[0].events = POLLOUT ;
                     filedes[1].fd = 0 ;
                     filedes[1].events = POLLIN ;
                     filedes[2].fd = fdAudio ;
                     filedes[3].events = POLLOUT ;

                     int pollRes = poll( filedes, 3, 1 );
                     if( 0 < pollRes )
                     {
                        char c ;
                        if( kbd.read( c ) || ( 0 != ( filedes[3].revents & POLLOUT ) ) )
                           break;
                     }
                     else
                     {
                        fprintf( stderr, "stall:%m\n" );
                        return 0 ;
                     }
                  }

                  if( mp3Out.started() )
                     mp3Out.poll();

                  if( ( decoder.needData != decoder.getState() )
                      &&
                      ( !waitVideo ) )
                  {
                     switch( decoder.parse() )
                     {
                        case mpegYUV_t :: header :
                        {
                           if( !yuvDev.haveHeaders() )
                           {
                              yuvDev.initHeader( xPos, yPos, 
                                                 decoder.width(),
                                                 decoder.height(),
                                                 ( 0 != outWidth ) ? outWidth : decoder.width(),
                                                 ( 0 != outHeight ) ? outHeight : decoder.height() );
debugPrint( "haveHeader: %u x %u\n", decoder.width(), decoder.height() );
                              headerTick = tickMs();
                           }
                           else
                              debugPrint( "!!! double headers\n" );
                           break;
                        }
                        case mpegYUV_t :: picture :
                        case mpegYUV_t :: needParse :
                        case mpegYUV_t :: needData :
                        {
                           break;
                        }
                     }
                  } // parse video output

                  if( mp3Out.started() )
                     mp3Out.poll();
                  
                  if( decoder.picture == decoder.getState() )
                  {
                     unsigned index ;
                     void *picMem = yuvDev.getBuf( index );
                     if( picMem )
                     {
debugPrint( "writePicture\n" );
                        unsigned char *yBuf ;
                        unsigned char *uBuf ;
                        unsigned char *vBuf ;

                        decoder.yuvBufs( yBuf, uBuf, vBuf );

                        interleaveYUV( yuvDev.getRowStride(), 
                                       yuvDev.getInHeight(), 
                                       (unsigned char *)picMem, yBuf, uBuf, vBuf );
                        long long whenMs = headerTick + decoder.relativePTS()/90 ;
                        if( yuvDev.write( index, whenMs ) )
                        {
debugPrint( "%u at %lld\n", index, whenMs );
decoder.parse();
                        }
                        else
                        {
                           fprintf( stderr, "Write error: %m" );
                           return 0 ;
                        }
                        waitVideo = false ;
                     }
                     else
                        waitVideo = true ;
                  }

                  char c ;
                  kbd.read( c );
                  if( ( '\x1b' == c ) || ( '\x03' == c ) )
                     break;

                  if( mp3Out.started() )
                     mp3Out.poll();

                  debugPrint( "%lu frames, %lu bytes queued\n", fIn.framesQueued(), fIn.bytesQueued() );
               } // while !eof

               printf( "%lu bytes queued at eof\n", mp3Out.bytesQueued() );
            }
            else
               perror( "/dev/dsp" );
         }
         else
            perror( "/dev/yuv" );
      }
      else
         perror( argv[1] );

/*

      FILE *fIn = fopen(argv[1], "rb" );
      if( fIn )
      {
         yuvDev_t     yuvDev ;
         if( yuvDev.isOpen() )
         {
            int const fdAudio = open( "/dev/dsp", O_WRONLY );
            if( 0 <= fdAudio )
            {
               mp3Queue_t   mp3Out( fdAudio );
               rawKbd_t     kbd ;
               mpegYUV_t    decoder ;
               mpegStream_t stream ;
               unsigned state = NEEDINDATA | FEEDVIDEO | NEEDPICTURE ;
               unsigned char inBuf[4096];
               int      numRead ;
               unsigned inOffs = 0 ;
               mpegStream_t::frameType_e type ;
               unsigned frameOffs ;
               unsigned frameLen ;
               long long pts ;
               long long dts ;
               long long headerTick = 0 ;
               unsigned char streamId ;
   
               while( 0 == ( state & ENDOFFILE ) )
               {
                  if( mp3Out.started() )
                     mp3Out.poll();
                  debugPrint( "state == %08lx\n", state );
   
                  if( NEEDINDATA == ( state & INDATAMASK ) )
                  {
debugPrint( "readInData\n" );
                     if( 0 < ( numRead = fread( inBuf, 1, sizeof(inBuf)/2, fIn ) ) )
                     {
                        state |= PARSEINDATA ;
                        inOffs = 0 ;
                     }
                     else
                        state |= ENDOFFILE ;
                  } // read some more input
   
                  if( (PARSEINDATA|FEEDVIDEO) == ( state & (INDATAMASK|VIDEOMASK) ) )
                  {
debugPrint( "parseInData: %x, %x&%x\n", PARSEINDATA|FEEDVIDEO, state, (INDATAMASK|VIDEOMASK) );
                     if( ( numRead > inOffs )
                         &&
                         stream.getFrame( inBuf+inOffs, 
                                          numRead-inOffs,
                                          type,
                                          frameOffs,
                                          frameLen,
                                          pts, dts,
                                          streamId ) )
                     {
printf( "frameLen:%u\n", frameLen );                        
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
                              }
                              else
                              {
                                 fprintf( stderr, "packet underflow: %d of %u bytes read\n", numRead, left );
                                 state |= ENDOFFILE ;
                              }
                           }
                           else
                           {
                              fprintf( stderr, "packet overflow: %lu bytes needed, %lu bytes avail\n", left, max );
                              return 0 ;
                           }
                        }
   
                        unsigned char const *frameData = inBuf+inOffs+frameOffs ;
   
                        if( mpegStream_t::videoFrame_e == type )
                        {
debugPrint( "videoFrame\n" );
                           decoder.feed( frameData, frameLen, pts );
                           state &= ~VIDEOMASK ;
                           state |= PARSEVIDEO ;
                        } // video frame
                        else if( mpegStream_t::audioFrame_e == type )
                        {
                           mp3Out.queueData( frameData, frameLen, 0 );
                           if( mp3Out.queueIsFull() )
                              mp3Out.start();
                        }
                        
                        inOffs += frameOffs+frameLen ;
                     }
                     else
                     {
                        state &= ~INDATAMASK ;
                        state |= NEEDINDATA ;
                     }
                  } // have data and somewhere to put it
   
                  if( PARSEVIDEO == ( state & VIDEOMASK ) )
                  {
debugPrint( "parseVideo\n" );
                     switch( decoder.parse() )
                     {
                        case mpegYUV_t::header :
                        {
                           if( !yuvDev.haveHeaders() )
                           {
                              yuvDev.initHeader( xPos, yPos, 
                                                 decoder.width(),
                                                 decoder.height(),
                                                 ( 0 != outWidth ) ? outWidth : decoder.width(),
                                                 ( 0 != outHeight ) ? outHeight : decoder.height() );
debugPrint( "haveHeader: %u x %u\n", decoder.width(), decoder.height() );
                              headerTick = tickMs();
                           }
                           else
                              debugPrint( "!!! double headers\n" );
   
                           break;
                        }
   
                        case mpegYUV_t::picture :
                        {
                           state &= ~PARSEVIDEO ;
                           state |= EATVIDEO ;
                           state |= SENDPICTURE ;
                           break;
                        }
   
                        case mpegYUV_t::needData :
                        case mpegYUV_t::needParse :
                        default:
                           state &= ~PARSEVIDEO ; 
                           state |= FEEDVIDEO ;
                           break;
                     }
                  } // something to decode
   
                  char c = '\x00' ;
                  if( SENDPICTURE == ( state & PICTUREMASK ) )
                  {
debugPrint( "sendPicture\n" );
                     unsigned index ;
                     void *picMem ; 
                     while( 0 == ( picMem = yuvDev.getBuf( index ) ) )
                     {
                        pollfd filedes[3];
                        filedes[0].fd = yuvDev.getFd() ;
                        filedes[0].events = POLLOUT ;
                        filedes[1].fd = 0 ;
                        filedes[1].events = POLLIN ;
                        filedes[2].fd = fdAudio ;
                        filedes[3].events = POLLOUT ;
   
                        int pollRes = poll( filedes, 3, 1 );
                        if( 0 < pollRes )
                        {
                           if( kbd.read( c ) || ( 0 != ( filedes[3].revents & POLLOUT ) ) )
                              break;
                        }
                        else
                        {
                           fprintf( stderr, "stall:%m\n" );
                           return 0 ;
                        }
                     }
                     
                     if( 0 != picMem )
                     {
debugPrint( "writePicture\n" );
                        unsigned char *yBuf ;
                        unsigned char *uBuf ;
                        unsigned char *vBuf ;
   
                        decoder.yuvBufs( yBuf, uBuf, vBuf );
   
                        interleaveYUV( yuvDev.getRowStride(), 
                                       yuvDev.getInHeight(), 
                                       (unsigned char *)picMem, yBuf, uBuf, vBuf );
                        long long whenMs = headerTick + decoder.relativePTS()/90 ;
                        if( yuvDev.write( index, whenMs ) )
                        {
debugPrint( "%u at %lld\n", index, whenMs );
                           state |= PARSEVIDEO ;
                           state &= ~(EATVIDEO|SENDPICTURE);
                        }
                        else
                        {
                           fprintf( stderr, "Write error: %m" );
                           return 0 ;
                        }
   
                     }
                     else
                        perror( "GETBUF" );
                  } // need to send a picture to the output device
               
                  kbd.read( c );
                  if( ( '\x1b' == c ) || ( '\x03' == c ) )
                     break;

                  if( mp3Out.started() )
                     mp3Out.poll();
               } // while !eof

               close( fdAudio );
            }
            else
               perror( "/dev/dsp" );
         }
         else
            perror( "/dev/yuv" );
         fclose( fIn );
      }
      else
         perror( argv[1] );
*/
   }
   else
      fprintf( stderr, "Usage: %s fileName\n", argv[0] );

   return 0 ;
}

