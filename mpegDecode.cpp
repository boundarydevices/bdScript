/*
 * Module mpegDecode.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: mpegDecode.cpp,v $
 * Revision 1.21  2007-08-23 00:25:50  ericn
 * -add CLOEXEC for file handles
 *
 * Revision 1.20  2007/08/08 17:08:21  ericn
 * -[sm501] Use class names from sm501-int.h
 *
 * Revision 1.19  2007/07/30 22:33:24  ericn
 * -Use KERNEL_FB_SM501, not NEON macro
 *
 * Revision 1.18  2007/07/07 19:24:49  ericn
 * -[mpegdecode] allow ACCEL environment variable to turn on acceleration
 *
 * Revision 1.17  2007/05/06 20:39:20  ericn
 * -conditional YUV support
 *
 * Revision 1.16  2006/08/31 15:51:06  ericn
 * -remove unused code
 *
 * Revision 1.15  2006/08/31 03:45:27  ericn
 * -use non-blocking I/O on YUV device
 *
 * Revision 1.14  2006/08/27 19:15:22  ericn
 * -removed mpegDecode2 test program (mpegQueue does the same thing)
 *
 * Revision 1.13  2006/08/22 15:49:42  ericn
 * -remove GOP, separate mask from picType
 *
 * Revision 1.12  2006/08/16 21:22:39  ericn
 * -remove convert_null
 *
 * Revision 1.11  2006/08/16 02:29:12  ericn
 * -add temp_ref, timestamp to feed/pull interface
 *
 * Revision 1.10  2006/07/30 21:36:20  ericn
 * -show GOPs
 *
 * Revision 1.9  2005/11/06 00:49:48  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.8  2005/08/22 13:12:56  ericn
 * -remove redundant default param
 *
 * Revision 1.7  2005/01/09 04:53:08  ericn
 * -added piece-wise processing test via mpegStream.cpp
 *
 * Revision 1.6  2004/10/30 18:55:34  ericn
 * -Neon YUV support
 *
 * Revision 1.5  2004/06/28 02:56:43  ericn
 * -use new tags
 *
 * Revision 1.4  2004/05/23 21:25:21  ericn
 * -updated to work with either new or old mpeg2dec library
 *
 * Revision 1.3  2003/07/28 13:36:08  ericn
 * -removed debug statements
 *
 * Revision 1.2  2003/07/20 18:35:38  ericn
 * -added picType, decode timing
 *
 * Revision 1.1  2003/07/20 15:43:38  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "mpegDecode.h"

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include "config.h"
#include <mpeg2dec/mpeg2.h>
#include <assert.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <linux/sm501-int.h>

#if CONFIG_LIBMPEG2_OLD == 1
   #include "mpeg2dec/video_out.h"
   #include "mpeg2dec/convert.h"
   #define mpeg2_sequence_t sequence_t
   #define mpeg2convert_rgb16 convert_rgb16
#else
   #include "mpeg2dec/video_out.h"
   #include "mpeg2dec/mpeg2convert.h"
#endif
};
#include "yuyv.h"

#include <stdio.h>

#define DECODER ((mpeg2dec_t *)mpegDecoder_ )
#define INFOPTR ((mpeg2_info_t const *)mpegInfo_ )

mpegDecoder_t :: mpegDecoder_t( void )
   : state_( end_e ),
     mpegDecoder_( mpeg2_init() ),
     mpegInfo_( ( 0 != mpegDecoder_ ) ? mpeg2_info( DECODER ) : 0 ),
     haveVideoHeader_( false ),
     mpegWidth_( 0 ),
     mpegStride_( 0 ),
     mpegHeight_( 0 ),
     mpegFrameType_( 0 ),
     mpegFrameRate_( 0 ),
     lastPicType_( ptUnknown_e ),
     tempRef_(UINT_MAX),
     skippedP_( false ),
     numSkipped_( 0 )
   , numParsed_( 0 )
   , numDraw_( 0 )
   , ptsIn_( 0 )
   , ptsOut_( 0 )
{
   if( mpegDecoder_ && mpegInfo_ )
   {
      mpState_ = mpeg2_parse( DECODER );
      state_ = processing_e ;
   }
   else
      fprintf( stderr, "Error initializing mpeg decoder\n" );
}

mpegDecoder_t :: ~mpegDecoder_t( void )
{
   mpeg2_close( DECODER );
#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
   while( !allocated_.empty() )
   {
      void *buf = allocated_.front();
      allocated_.pop_front();
      free( buf );
   }
#endif
}

void mpegDecoder_t :: feed
   ( void const   *inData, 
     unsigned long inBytes,
     long long     pts )
{
   if( mpegDecoder_ && mpegInfo_ )
   {
      if( ( 0 != pts ) && ( 0 == ptsIn_ ) ){
         ptsIn_ = ptsOut_ = pts ;
      }
      unsigned char *bIn = (unsigned char *)inData ;
      mpeg2_buffer( DECODER, bIn, bIn + inBytes );
   }
}

#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
void *mpegDecoder_t :: getPictureBuf( void )
{
   if( !freeBufs_.empty() )
   {
      void *buf = freeBufs_.back();
      freeBufs_.pop_back();
      return buf ;
   }
   else
   {
      void *buf = malloc( mpegHeight_*mpegStride_*4 );
      allocated_.push_back( buf );
      return buf ;
   }
}
#endif

inline void diffTime( timeval       &diff,
                      timeval const &early,
                      timeval const &late )
{
   long secs, usecs;
   secs=late.tv_sec-early.tv_sec;
   usecs=late.tv_usec-early.tv_usec;
   if( usecs<0 ) 
   {
      usecs+=1000000;
      --secs;
   }

   diff.tv_sec = secs ;
   diff.tv_usec = usecs ;
}


void interleaveYUV( int                  width, 
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
      }
      
      if( 1 == ( row & 1 ) )
      {
         u += width/2 ;
         v += width/2 ;
      }
   }
}

bool mpegDecoder_t :: getPicture
   ( void const *&picture,
     picType_e   &type,
     unsigned    &temp_ref,
     long long   &when,
     unsigned     ptMask )
{
   type = ptUnknown_e ;
   temp_ref = tempRef_ ;
   do {
++numParsed_ ;
      mpState_ = mpeg2_parse( DECODER );
      switch( mpState_ )
      {
         case STATE_SEQUENCE:
         {
            mpeg2_sequence_t const &seq = *INFOPTR->sequence ;
//            if( !haveVideoHeader_ )
            {
               haveVideoHeader_ = true ;
#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
               mpegWidth_ = seq.width ;
               mpegStride_ = ((seq.width+15)/16)*16 ;
#else
               mpegWidth_ = seq.width ;
#endif 
               mpegHeight_ = seq.height ;
               switch( seq.frame_period )
               {
                  case 1126125 :	// 1 (23.976) - NTSC FILM interlaced
                  case 1125000 :	// 2 (24.000) - FILM
                     mpegFrameRate_ = 24 ;
                     break;
                  case 1080000 :	// 3 (25.000) - PAL interlaced
                     mpegFrameRate_ = 25 ;
                     break;
                  case  900900 : // 4 (29.970) - NTSC color interlaced
                  case  900000 : // 5 (30.000) - NTSC b&w progressive
                     mpegFrameRate_ = 30 ;
                     break;
                  case  540000 : // 6 (50.000) - PAL progressive
                     mpegFrameRate_ = 50 ;
                     break;
                  case  450450 : // 7 (59.940) - NTSC color progressive
                  case  450000 : // 8 (60.000) - NTSC b&w progressive
                     mpegFrameRate_ = 60 ;
                     break;
                  default:
                     mpegFrameRate_ = 0 ;
               }

               if( 0 != mpegFrameRate_ )
                  msPerPic_ = 1000/mpegFrameRate_ ;
               else
                  msPerPic_ = 0 ;

//printf( "mpegw: %u, stride: %u, mpegh:%u\n", mpegWidth_, mpegStride_, mpegHeight_ );
//printf( "dispw: %u, disph:%u\n", seq.display_width, seq.picture_height );
//printf( "pixw: %u, pixh:%u\n", seq.pixel_width, seq.pixel_height );
            }
//            else 
               if( seq.width != mpegWidth_ )
            {
               printf( "varying width: %u/%u\n", seq.width, mpegWidth_ );
            }

#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
            mpeg2_convert( DECODER, mpeg2convert_yuyv, NULL );
            mpeg2_custom_fbuf (DECODER, 1);
#else
            mpeg2_convert( DECODER, mpeg2convert_rgb16, NULL );
            mpeg2_custom_fbuf (DECODER, 0);
#endif 
/*
            printf( "w: %u, h:%u\n", seq.width, seq.height );
            printf( "chroma_width %u, chroma_height %u\n", seq.chroma_width, seq.chroma_height );
            printf( "byte rate %u\n", seq.byte_rate );
            printf( "vbv_buffer_size %u\n", seq.vbv_buffer_size );
            printf( "flags: %u\n", seq.flags );
            printf( "picw: %u, pich:%u\n", seq.picture_width, seq.picture_height );
            printf( "dispw: %u, disph:%u\n", seq.display_width, seq.picture_height );
            printf( "pixw: %u, pixh:%u\n", seq.pixel_width, seq.pixel_height );
            printf( "period %u\n", seq.frame_period );
            printf( "profile_level_id %u\n", seq.profile_level_id );
            printf( "colour_primaries %u\n", seq.colour_primaries );
            printf( "transfer_characteristics %u\n", seq.transfer_characteristics );
            printf( "matrix_coefficients %u\n", seq.matrix_coefficients );
*/
            break;
         }
         case STATE_GOP:
         {
            break ;
         }
         case STATE_PICTURE:
         {
            int picType = ( INFOPTR->current_picture->flags & PIC_MASK_CODING_TYPE ) - 1;
            bool skip ;
            if( picType <= ptD_e )
            {
               tempRef_ = temp_ref = INFOPTR->current_picture->temporal_reference ;
//printf( "picture (%u)\n", temp_ref );
               lastPicType_ = (picType_e)picType ;
               skip = ( 0 == ( ptMask & ( 1 << picType ) ) );
//               if( PIC_FLAG_CODING_TYPE_I == picType )
//                  ptsOut_ = ptsIn_ ;

               if( PIC_FLAG_CODING_TYPE_I - 1 != picType ) 
               {
                  if( ( PIC_FLAG_CODING_TYPE_I - 1 == picType ) && skip )
                     skippedP_ = true ;

                  if( !skip && skippedP_ )
                     skip = true ;
               }
               else
                  skip = skippedP_ = false ;
            }
            else
            {
               fprintf( stderr, "unknown picType:%d", picType );
               skip = true ;
            }

            numSkipped_ += skip ;
            mpeg2_skip( DECODER, skip );
            if( !skip )
            {
               gettimeofday( &usStartDecode_, 0 );

#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
               unsigned char *buf = (unsigned char *)getPictureBuf();
               unsigned ySize = mpegStride_*mpegHeight_ ; 
               unsigned uvSize = ySize / 2 ;
               unsigned char *planes[3] = { 
                  buf,
                  buf+ySize,
                  buf+ySize+uvSize
               };
               mpeg2_set_buf((mpeg2dec_t *)mpegDecoder_, planes, buf);
#endif 
            }
            break;
         }
         case STATE_SLICE:
//                                          case STATE_END:
         {
            if (INFOPTR->display_fbuf) 
            {
               if( ( 0 != INFOPTR->current_picture )
                   && 
                   ( 0 == ( INFOPTR->current_picture->flags & PIC_FLAG_SKIP ) ) )
               {
                  type = lastPicType_ ;
                  lastPicType_ = ptUnknown_e ;
                  
                  unsigned char *buf = (unsigned char *)INFOPTR->display_fbuf->id ;
                  if( buf )
                  {
picture = buf ;
#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
                     freeBufs_.push_back( buf );
#endif
/*
printf( "buf == %p/%p/%p --> %p\n",
        INFOPTR->display_fbuf->buf[0],
        INFOPTR->display_fbuf->buf[1],
        INFOPTR->display_fbuf->buf[2], 
        INFOPTR->display_fbuf->id
        );
printf( "yuv:%p, y:%p, u:%p, v:%p\n",
        yuv, yBuf, uBuf, vBuf );
*/        
                  }
                  else
                     fprintf( stderr, "Invalid fbuf id!\n" );
                  
                  gettimeofday( &usDecodeTime_, 0 );
                  diffTime( usDecodeTime_, usStartDecode_, usDecodeTime_ );
                  when = ptsOut_ ;
                  ptsOut_ += msPerPic_ ;

                  ++numDraw_ ;
                  return true ;
               }
               else
               {
               } // not a valid picture
            }
            break;
         }
      }
   } while( ( -1 != mpState_ ) && ( STATE_BUFFER != mpState_ ) );
   
   return false ;
}

void const *mpegDecoder_t::gop(void) const 
{
   return INFOPTR->gop ;
}

static char const cPicTypes[] = {
   'I', 'P', 'B', 'D', 'G', '?'
};

char mpegDecoder_t :: getPicType( picType_e t )
{
   if( (unsigned)t < sizeof( cPicTypes ) )
      return cPicTypes[t];
   else
      return '?' ;
}

#ifdef __MODULETEST__
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
#include <openssl/md5.h>

static void setPlane( mpegDecoder_t &decoder,
                      int            fdYUV,
                      unsigned       xPos,
                      unsigned       yPos,
                      unsigned       outWidth,
                      unsigned       outHeight,
                      unsigned      &bytesPerPicture )
{
   bytesPerPicture = decoder.width() * decoder.height() * 2;
   printf( "%u x %u: %u bytes per picture\n"
           "frame rate: %u fps\n", 
           decoder.width(), decoder.height(), bytesPerPicture,
           decoder.frameRate() );
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
   }
   else
      printf( "setPlane success\n" );
}

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
if( 0 != getenv("ACCEL") )
      mpeg2_accel (MPEG2_ACCEL_ARM);

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
         printf( "%lu streams\n", bi->count_ );

         int videoIdx = -1 ;

         MD5_CTX videoMD5 ;
         MD5_Init( &videoMD5 );

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

            mpegDecoder_t decoder ;
            mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[videoIdx];
            mpegDemux_t::frameType_e ft = sAndF.sInfo_.type ;
            mpegDemux_t::frame_t const *next = sAndF.frames_ ;
            int numLeft = sAndF.numFrames_ ;
            unsigned numPictures = 0 ;
            long long startMs = tickMs();
            bool haveHeader = false ;
            unsigned bytesPerPicture = 0 ;
            int const fdYUV = open( "/dev/" SM501YUV_CLASS, O_WRONLY );
            fcntl(fdYUV, F_SETFL, fcntl(fdYUV, F_GETFL) | O_NONBLOCK | FASYNC );
            fcntl( fdYUV, F_SETFD, FD_CLOEXEC );
            char keyvalue = '\0' ;
            
            unsigned long videoBytes = 0 ;

            long long prevPTS = -1LL ;
            do {
               long long pts = next->when_ms_ ;
               decoder.feed( next->data_, next->length_, next->when_ms_ );

               MD5_Update(&videoMD5, next->data_, next->length_ );
               videoBytes += next->length_ ;

               next++ ;
               numLeft-- ;
               void const *picture ;
               mpegDecoder_t::picType_e picType ;
               
               unsigned temp_ref ;
               long long when ;
               while( decoder.getPicture( picture, picType, temp_ref, when ) )
               {
                  if( !haveHeader )
                  {
                     haveHeader = decoder.haveHeader();
                     if( haveHeader )
                     {
                        setPlane( decoder, fdYUV, xPos, yPos, outWidth, outHeight, bytesPerPicture );
                     }
                     else
                        printf( "not yet\n" );
                  }
printf( "%c - %llu (%u)\n", decoder.getPicType(picType), when, temp_ref );

                  ++numPictures ;
                  if( haveHeader )
                  {
//                     if( pts != prevPTS )
                     {
                        prevPTS = pts ;
                        if( 0 <= fdYUV )
                        {
if( picTypeMask & ( 1 << picType ) ){
                           int const numWritten = write( fdYUV, picture, bytesPerPicture );
                           if( numWritten != bytesPerPicture )
                           {
                              printf( "write %d of %u bytes\n", numWritten, bytesPerPicture );
                              numLeft = 0 ;
                              break;
                           }
}
   if( 'g' != keyvalue )
   {
                           while( 0 >= read(0,&keyvalue,1) )
                              ;
   //read(0,&keyvalue,1);
                           keyvalue = tolower( keyvalue );
                           if( ( 'x' == keyvalue ) || ( '\x03' == keyvalue ) )
                           {
                              numLeft = 0 ;
                              break;
                           }
   } else {
      // still watch for <Ctrl-C>
      char c2 ;
      if( 0 >= read(0,&c2,1) ){
         c2 = tolower(c2);
         if( ( '\x03' == c2 ) || ( 'x' == c2 ) ){
            numLeft = 0 ;
            break ;
         }
      }
   }
                        }
                     }
//                     else
//                        printf( "skip\n" );
                  }
                  else
                     printf( "picture without header: %s\n", decoder.haveHeader() ? "true" : "false" );
               }
            } while( 0 < numLeft );

            long long endMs = tickMs();
            
            if( 0 <= fdYUV )
               close( fdYUV );

            unsigned elapsed = (unsigned)( endMs-startMs );
            printf( "%u pictures, %lu bytes, %u ms\n", numPictures, videoBytes, elapsed );
            
            unsigned char md5[MD5_DIGEST_LENGTH];
            MD5_Final( md5, &videoMD5 );

            printf( "   md5: " );
            for( unsigned i = 0 ; i < MD5_DIGEST_LENGTH ; i++ ){
               printf( "%02x ", md5[i] );
            }
            printf( "\n" );

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

