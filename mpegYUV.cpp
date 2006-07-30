/*
 * Module mpegYUV.cpp
 *
 * This module defines the methods of the mpegYUV_t class
 * as declared in mpegYUV.h
 *
 *
 * Change History : 
 *
 * $Log: mpegYUV.cpp,v $
 * Revision 1.2  2006-07-30 21:35:48  ericn
 * -compile under latest libmpeg2
 *
 * Revision 1.1  2005/04/24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "mpegYUV.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "debugPrint.h"

mpegYUV_t :: mpegYUV_t( void )
   : state_( needData )
   , firstPts_( 0 )
   , iframePts_( 0 )
   , pts_( 0 )
   , decoder_( mpeg2_init() )
   , mpegInfo_( (mpeg2_info_t *)mpeg2_info( decoder_ ) )
   , stride_( 0 )
   , bufs_( 0 )
{
   memset( &sequence_, 0, sizeof( sequence_ ) );
}


mpegYUV_t :: ~mpegYUV_t( void )
{
   mpeg2_close( decoder_ );
   while( bufs_ )
   {
      buf_t *buf = bufs_ ;
      bufs_ = buf->next_ ;
      free( buf );
   }
}

void mpegYUV_t :: feed
   ( void const *inData, 
     unsigned    numBytes,
     long long   pts )
{
   assert( needData == state_ );
   memcpy( feedBuf_, inData, numBytes );
   mpeg2_buffer( decoder_, (uint8_t *)feedBuf_, (uint8_t *)feedBuf_ + numBytes );
   if( 0 == pts_ )
      firstPts_ = pts ;
   pts_ = pts ;
   state_ = needParse ;
}
   
void *mpegYUV_t :: getPictureBuf
   ( unsigned char **planes,
     unsigned        height,
     unsigned        stride )
{
   buf_t *buf = (buf_t *)malloc( height*stride*2+sizeof(buf_t) );
   buf->next_ = bufs_ ;
   buf->data_ = (char *)buf + sizeof( *buf );
   bufs_ = buf ;

   unsigned ySize = stride*height ; 
   unsigned uvSize = ySize / 2 ;
   planes[0] = (unsigned char *)buf->data_ ;
   planes[1] = (unsigned char *)planes[0]+ySize ;
   planes[2] = (unsigned char *)planes[0]+ySize+uvSize ;

   return buf->data_ ;
}

mpegYUV_t :: state_t mpegYUV_t :: parse( void )
{
   if( needData != state_ )
   {
      int mpState = STATE_BUFFER ;
      do {
         mpState = mpeg2_parse( decoder_ );
         switch( mpState )
         {
            case STATE_SEQUENCE:
            {
               sequence_ = *mpegInfo_->sequence ;
               stride_ = ((sequence_.width+15)/16)*16 ;
               
               ptsPerFrame_ = 0 ;
               switch( sequence_.frame_period )
               {
                  case 1126125 :	// 1 (23.976) - NTSC FILM interlaced
                  case 1125000 :	// 2 (24.000) - FILM
                     frameRate_ = 24 ;
                     break;
                  case 1080000 :	// 3 (25.000) - PAL interlaced
                     frameRate_ = 25 ;
                     break;
                  case  900900 : // 4 (29.970) - NTSC color interlaced
                  case  900000 : // 5 (30.000) - NTSC b&w progressive
                     frameRate_ = 30 ;
                     break;
                  case  540000 : // 6 (50.000) - PAL progressive
                     frameRate_ = 50 ;
                     break;
                  case  450450 : // 7 (59.940) - NTSC color progressive
                  case  450000 : // 8 (60.000) - NTSC b&w progressive
                     frameRate_ = 60 ;
                     break;
                  default:
                     frameRate_ = 0 ;
               }
               if( 0 != frameRate_ )
                  ptsPerFrame_ = 1000*90/frameRate_ ;
               else
                  ptsPerFrame_ = 0 ;
   
               unsigned char *planes[3];
               unsigned char *buf = (unsigned char *)getPictureBuf( planes, sequence_.height, stride_ );
               mpeg2_set_buf(decoder_, planes, buf);
               buf = (unsigned char *)getPictureBuf( planes, sequence_.height, stride_ );
               mpeg2_set_buf(decoder_, planes, buf);
               buf = (unsigned char *)getPictureBuf( planes, sequence_.height, stride_ );
               mpeg2_set_buf(decoder_, planes, buf);
               
               state_ = header ;
               return state_ ;
            }
            
            case STATE_GOP:
            {
               break;
            }
            case STATE_PICTURE:
            {
               mpeg2_picture_t *curpic = (mpeg2_picture_t *)mpegInfo_->current_picture ;
               curpic->tag = pts_ ;
               break;
            }

            case STATE_SLICE:
            case STATE_END:
            {
               if (mpegInfo_->display_fbuf) 
               {
                  mpeg2_picture_t *dpic = (mpeg2_picture_t *)mpegInfo_->display_picture ;
                  if( ( 0 != dpic )
                      && 
                      ( 0 == ( dpic->flags & PIC_FLAG_SKIP ) ) )
                  {
                     int picType = ( dpic->flags & PIC_MASK_CODING_TYPE );
                     if( PIC_FLAG_CODING_TYPE_I == picType )
                     {
                        iframePts_ = dpic->tag ;
                     }
                     else
                     {
                        iframePts_ += ptsPerFrame_ ;
                        dpic->tag = iframePts_ ;
                     }
                     
                     state_ = picture ;
                     return state_ ;
                  }
                  else
                  {
                  } // not a valid picture
               }
               
               break;
            }
         }
      } while( STATE_BUFFER != mpState );
      
      state_ = needData ;
   }
   return state_ ;
}

void mpegYUV_t :: yuvBufs
   ( unsigned char *&yBuf,
     unsigned char *&uBuf,
     unsigned char *&vBuf ) const 
{
   unsigned char *buf = (unsigned char *)getDisplayBuf().id ;
   if( buf )
   {
      unsigned ySize = stride_*height();
      unsigned uvSize = ySize / 2 ;
      yBuf = buf ;
      uBuf = yBuf+ySize ;
      vBuf = uBuf+uvSize ;
   }
   else
   {
      yBuf =
      uBuf =
      vBuf = 0 ;
   }
}



#ifdef MODULETEST
#include <stdio.h>
#include "rawKbd.h"
#include "mpegStream.h"
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <linux/sm501yuv.h>
#include <linux/sm501-int.h>

static void interleaveYUV
   ( int                  width, 
     int                  height, 
     unsigned char       *yuv, 
     unsigned char const *y, 
     unsigned char const *u, 
     unsigned char const *v )
{
   for( int row = 0 ; row < height ; row++ )
   {
      unsigned char const *uRow = u ;
      unsigned char const *vRow = v ;
      for( int col = 0 ; col < width ; col++ )
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
               

// if( 1 == 3&(7)PARSEINDATA|FEEDVIDEO == ( state & (INDATAMASK|VIDEOMASK) ) )

unsigned const NEEDINDATA  = 0 ;
unsigned const PARSEINDATA = 1 ;
unsigned const INDATAMASK = PARSEINDATA ;

unsigned const FEEDVIDEO   = 0 ;
unsigned const PARSEVIDEO  = 2 ;
unsigned const EATVIDEO    = 4 ;
unsigned const VIDEOMASK   = PARSEVIDEO | EATVIDEO ;

unsigned const NEEDPICTURE = 0 ;
unsigned const SENDPICTURE = 8 ;
unsigned const PICTUREMASK = SENDPICTURE ;

unsigned const ENDOFFILE   = 16 ;

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

      FILE *fIn = fopen(argv[1], "rb" );
      if( fIn )
      {
         int fdYUV = open( "/dev/yuv", O_WRONLY );
         if( 0 > fdYUV ){
            perror( "/dev/yuv" );
            return -1 ;
         }

         rawKbd_t     kbd ;
         mpegYUV_t    decoder ;
         mpegStream_t stream ;
         unsigned state = NEEDINDATA | FEEDVIDEO | NEEDPICTURE ;
         unsigned char inBuf[4096];
         int numRead = 0 ;
         int inOffs = 0 ;
         mpegStream_t::frameType_e type ;
         unsigned frameOffs ;
         unsigned frameLen ;
         long long pts ;
         long long dts ;
         unsigned char streamId ;
         unsigned       yuvSize = 0 ;
         unsigned char *yuvBuf = 0 ;

         while( 0 == ( state & ENDOFFILE ) )
         {
            debugPrint( "state == %08x\n", state );

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
                  int end = inOffs+frameOffs+frameLen ;
                  if( end > numRead )
                  {
                     int left = end-numRead ;
                     int max = sizeof(inBuf)-numRead ;
                     if( max > left )
                     {
                        int numRead2 = fread( inBuf+numRead, 1, left, fIn );
                        if( numRead2 == left )
                        {
                           debugPrint( "tail end %u bytes\n", left );
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
debugPrint( "haveHeader: %u x %u\n", decoder.width(), decoder.height() );
                     if( yuvBuf )
                        delete [] yuvBuf ;
                     yuvSize = decoder.width() * decoder.height() * 2;
                     yuvBuf = new unsigned char [yuvSize];

                     sm501yuvPlane_t plane_ ;
                     plane_.xLeft_     = 0 ;
                     plane_.yTop_      = 0 ;
                     plane_.inWidth_   = decoder.width();
                     plane_.inHeight_  = decoder.height();
                     plane_.outWidth_  = decoder.width();
                     plane_.outHeight_ = decoder.height();
                     if( 0 != ioctl( fdYUV, SM501YUV_SETPLANE, &plane_ ) ){
                        perror( "SM501YUV_SETPLANE" );
                        return -1 ;
                     }

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

            if( SENDPICTURE == ( state & PICTUREMASK ) )
            {
debugPrint( "sendPicture\n" );
               state |= PARSEVIDEO ;
               state &= ~(EATVIDEO|SENDPICTURE);
               if( yuvBuf ){
                  unsigned char *yBuf ;
                  unsigned char *uBuf ;
                  unsigned char *vBuf ;

                  decoder.yuvBufs( yBuf, uBuf, vBuf );
                  interleaveYUV( decoder.width(),
                                 decoder.height(), 
                                 yuvBuf, yBuf, uBuf, vBuf );
                  write( fdYUV, yuvBuf, yuvSize );
               }
               else {
                  fprintf( stderr, "picture without header!\n" );
                  return -1 ;
               }

/*
               unsigned index ;
               void *picMem ; 
               while( 0 == ( picMem = yuvDev.getBuf( index ) ) )
               {
                  pollfd filedes[2];
                  filedes[0].fd = yuvDev.getFd() ;
                  filedes[0].events = POLLOUT ;
                  filedes[1].fd = 0 ;
                  filedes[1].events = POLLIN ;

                  int pollRes = poll( filedes, 2, 10000 );
                  if( 0 < pollRes )
                  {
                     char c ;
                     if( kbd.read( c ) )
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
*/                  
            } // need to send a picture to the output device
         }
         fclose( fIn );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s fileName\n", argv[0] );

   return 0 ;
}
#endif 
