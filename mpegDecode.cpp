/*
 * Module mpegDecode.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: mpegDecode.cpp,v $
 * Revision 1.10  2006-07-30 21:36:20  ericn
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
   #include "mpeg2dec/convert.h"
#endif

};
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
     lastPicType_( ptUnknown_e ),
     skippedP_( false ),
     numSkipped_( 0 )
   , numParsed_( 0 )
   , numDraw_( 0 )
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
   while( !allocated_.empty() )
   {
      void *buf = allocated_.front();
      allocated_.pop_front();
      free( buf );
   }
}

void mpegDecoder_t :: feed
   ( void const   *inData, 
     unsigned long inBytes )
{
   if( mpegDecoder_ && mpegInfo_ )
   {
      unsigned char *bIn = (unsigned char *)inData ;
      mpeg2_buffer( DECODER, bIn, bIn + inBytes );
   }
}

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

#ifdef NEON
struct yuv_internal_t {
   unsigned char *out_ptr;
   unsigned       width; // in pixels
   unsigned       outPad; // in pixels
   unsigned       in_stridex;
   unsigned       out_stridex;
   unsigned       in_stride_frame;  // in bytes
   unsigned       out_stride_frame; // in bytes
};

void yuv_null_start( void * _id, uint8_t * const * dest, int flags )
{
//fprintf( stderr, "null start\n" );
    yuv_internal_t * id = (yuv_internal_t *) _id;
    id->out_ptr = dest[0];
    switch (flags) {
    case CONVERT_BOTTOM_FIELD:
      id->out_ptr += id->out_stride_frame ;
   /* break thru */
    case CONVERT_TOP_FIELD:
      id->in_stridex  = id->in_stride_frame << 1;
      id->out_stridex = id->out_stride_frame << 1 ;
      break;
    default:
      id->in_stridex = id->in_stride_frame ;
      id->out_stridex = id->out_stride_frame ;
    }
}

static bool first = true ;
static bool second = false ;

void yuv_null_copy( void * _id, uint8_t * const * src, unsigned int v_offset)
{
   yuv_internal_t * id = (yuv_internal_t *) _id;
   unsigned char * dst0 = ( id->out_ptr + (id->out_stridex * v_offset) );
   unsigned char * dst1 = ( dst0+id->out_stridex );

   uint8_t const * pyIn0 = src[0];
   uint8_t const * pyIn1 = pyIn0 + id->in_stridex ;
   uint8_t const * pu    = src[1];
   uint8_t const * pv    = src[2];
   
   unsigned const width4 = id->width/4 ;

   if( first || second )
   {
      fprintf( stderr, "null copy: id = %p\n" 
               "    y0 == %p\n"
               "    y1 == %p\n"
               "     u == %p\n"
               "     v == %p\n"
               "  dst0 == %p\n"
               "  dst1 == %p\n"
               "  vOff == %u\n"
               "width  == %u\n"
               "wid4   == %u\n"
               , id, pyIn0, pyIn1, pu, pv, 
               dst0, dst1, v_offset,
               id->width, width4 );
   }
   //
   // called for each 16 lines of input
   // each U and V apply to 2 lines of output, so we loop 8 times
   //
   //
   int loop = 8 ;
   do {
      // for each block of 2 lines, 2 pixels of width
      for( unsigned i = 0 ; i < id->width ; i += 4 )
      {
         unsigned char u ;
         unsigned char v ;
         unsigned char y0 ;
         unsigned char y1 ;
         
         // first block of 2
         u = *pu++ ;
         v = *pv++ ;
         
         y0 = *pyIn0++ ;
         *dst0++ = y0 ;
         *dst0++ = u ;
         y0 = *pyIn0++ ;
         *dst0++ = y0 ;
         *dst0++ = v ;

         y1 = *pyIn1++ ;
         *dst1++ = y1 ;
         *dst1++ = u ;
         y1 = *pyIn1++ ;
         *dst1++ = y1 ;
         *dst1++ = v ;

         // second block of 2
         u = *pu++ ;
         v = *pv++ ;
         
         y0 = *pyIn0++ ;
         *dst0++ = y0 ;
         *dst0++ = u ;
         y0 = *pyIn0++ ;
         *dst0++ = y0 ;
         *dst0++ = v ;

         y1 = *pyIn1++ ;
         *dst1++ = y1 ;
         *dst1++ = u ;
         y1 = *pyIn1++ ;
         *dst1++ = y1 ;
         *dst1++ = v ;
      }
/*
if( first || second )
   printf( "inner1 %s --> dst0: %p, dst1: %p, in0: %p, in1: %p\n", 
           first ? "first" : "second",
           dst0, dst1, pyIn0, pyIn1 );
*/           
      dst0 += id->outPad ;
      dst1 += id->outPad ;
      dst0 = dst1 ;
      dst1 = dst0 + id->out_stridex ;
/*
if( first || second )
   printf( "inner2 %s --> dst0: %p, dst1: %p, in0: %p, in1: %p\n", 
           first ? "first" : "second",
           dst0, dst1, pyIn0, pyIn1 );
*/
   } while (--loop);

if( first || second )
{
   printf( "outer %s --> dst0: %p, dst1: %p, in0: %p, in1: %p\n", 
           first ? "first" : "second",
           dst0, dst1, pyIn0, pyIn1 );
/*
   if( first )
   {
      first = false ;
      second = true ;
   }
   else
      second = false ;
*/      
}
}

void convert_null_128(int width, int height, uint32_t accel, void * arg, convert_init_t * result)
{
   if( 0 == result->id )
   {
      result->id_size = sizeof( yuv_internal_t );
   } // initial call before alloc
   else
   {
      yuv_internal_t *const id = (yuv_internal_t *)result->id ;
      assert( 0 != id );
      assert( result->id_size == sizeof( yuv_internal_t ) );

      id->width = width ;
      id->in_stridex = 0 ;
      id->in_stride_frame  = width ;
      id->out_stridex = 0 ;
      id->out_stride_frame = (((width+127)/128)*128);
      id->outPad = (id->out_stride_frame - width)*2;
      id->out_stride_frame *= 2 ; // 2 bytes per pixel

printf( "--> width %u, in stride: %u, out: %u\n", 
        width,
        id->in_stride_frame,
        id->out_stride_frame );

      result->buf_size[0] = id->out_stride_frame * height * 2 ;
      result->buf_size[1] = result->buf_size[2] = 0;
      result->start = yuv_null_start;
      result->copy  = yuv_null_copy ;
   } // second call to fill in details
}

#endif

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
     unsigned     ptMask )
{
   type = ptUnknown_e ;
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
#ifdef NEON
               mpegWidth_ = seq.width ;
               mpegStride_ = ((seq.width+15)/16)*16 ;
#else
               mpegWidth_ = seq.width ;
#endif 
               mpegHeight_ = seq.height ;
//printf( "mpegw: %u, stride: %u, mpegh:%u\n", mpegWidth_, mpegStride_, mpegHeight_ );
//printf( "dispw: %u, disph:%u\n", seq.display_width, seq.picture_height );
//printf( "pixw: %u, pixh:%u\n", seq.pixel_width, seq.pixel_height );
            }
//            else 
               if( seq.width != mpegWidth_ )
            {
               printf( "varying width: %u/%u\n", seq.width, mpegWidth_ );
            }

#ifdef NEON
//            mpeg2_convert( DECODER, convert_null_128, NULL );
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
            picture = INFOPTR->gop ;
            type = ptGOP_e ;
            return true ;
         }
         case STATE_PICTURE:
         {
//printf( "picture\n" );
            int picType = ( INFOPTR->current_picture->flags & PIC_MASK_CODING_TYPE ) - 1;
            bool skip ;
            if( picType <= ptD_e )
            {
               lastPicType_ = (picType_e)picType ;
               skip = ( 0 == ( ptMask & ( 1 << picType ) ) );
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

#ifdef NEON
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
                     unsigned ySize = mpegStride_*mpegHeight_ ;
                     unsigned uvSize = ySize / 2 ;
                     unsigned char *yBuf = buf ;
                     unsigned char *uBuf = yBuf+ySize ;
                     unsigned char *vBuf = uBuf+uvSize ;
                     unsigned char *yuv  = vBuf+uvSize ;

                     picture = yuv ;

                     interleaveYUV( mpegStride_, mpegHeight_, yuv, yBuf, uBuf, vBuf );
                     freeBufs_.push_back( buf );
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
         printf( "%lu streams\n", bi->count_ );

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

            mpegDecoder_t decoder ;
            mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[videoIdx];
            mpegDemux_t::frameType_e ft = sAndF.sInfo_.type ;
            mpegDemux_t::frame_t const *next = sAndF.frames_ ;
            int numLeft = sAndF.numFrames_ ;
            unsigned numPictures = 0 ;
            long long startMs = tickMs();
            bool haveHeader = false ;
            unsigned long bytesPerPicture = 0 ;
            int const fdYUV = open( "/dev/yuv", O_WRONLY );
            char keyvalue = '\0' ;
            
            long long prevPTS = -1LL ;
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
                     printf( "%u x %u: %u bytes per picture\n", 
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
                     else
                        printf( "setPlane success\n" );
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
                     if( mpegDecoder_t::ptGOP_e == picType ){
                     mpeg2_gop_t *gop = (mpeg2_gop_t *)picture ;
                     if( gop )
                        printf( "   %02u:%02u:%02u %u 0x%X\n", 
                                gop->hours, gop->minutes, gop->seconds, 
                                gop->pictures, gop->flags );
                        continue ;
                     }
                     if( pts != prevPTS )
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
   }
                        }
                     }
                     else
                        printf( "skip\n" );
                  }
                  else
                     printf( "picture without header\n" );
               }
            } while( 0 < numLeft );

            long long endMs = tickMs();
            
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


#elif defined( __MODULETEST2__ )

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
      FILE *fIn = fopen( argv[1], "rb" );
      if( fIn )
      {
         mpegStream_t stream ;
         mpegDecoder_t decoder ;
         unsigned numPictures = 0 ;
         long long startMs = tickMs();
         bool haveHeader = false ;
         unsigned long bytesPerPicture = 0 ;
         int const fdYUV = open( "/dev/yuv", O_WRONLY );
         long long prevPTS = -1LL ;

         unsigned long globalOffs = 0 ;
         unsigned char inBuf[4096];
         int  numRead ;
         while( 0 < ( numRead = fread( inBuf, 1, sizeof(inBuf)/2, fIn ) ) )
         {
            unsigned                  inOffs = 0 ;
            
            mpegStream_t::frameType_e type ;
            unsigned                  frameOffs ;
            unsigned                  frameLen ;
            long long                 when ;
            unsigned char             streamId ;

            while( ( numRead > inOffs )
                   &&
                   stream.getFrame( inBuf+inOffs, 
                                  numRead-inOffs,
                                  type,
                                  frameOffs,
                                  frameLen,
                                  when,
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
                  
                  if( !haveHeader )
                  {
                     haveHeader = decoder.haveHeader();
                     if( haveHeader )
                     {
                        bytesPerPicture = decoder.width() * decoder.height() * 2;
                        printf( "%u x %u: %u bytes per picture\n", 
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
                     }
                  }
                  void const *picture ;
                  mpegDecoder_t::picType_e picType ;
                  while( decoder.getPicture( picture, picType, picTypeMask ) )
                  {
   printf( "%c - %llu\n", 
           ( mpegDecoder_t::ptD_e >= picType )
           ? cPicTypes[picType]
           : '?',
           when );
                     ++numPictures ;
                     if( haveHeader )
                     {
                        if( when != prevPTS )
                        {
                           prevPTS = when ;
                           if( 0 <= fdYUV )
                           {
if( picTypeMask & ( 1 << picType ) ){
                              int const numWritten = write( fdYUV, picture, bytesPerPicture );
                              if( numWritten != bytesPerPicture )
                              {
                                 printf( "write %d of %u bytes\n", numWritten, bytesPerPicture );
                              }
}
                           }
                        }
                     }
                     else
                        printf( "picture without header\n" );
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

