/*
 * Module mpegDecode.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: mpegDecode.cpp,v $
 * Revision 1.4  2004-05-23 21:25:21  ericn
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

#ifdef LIBMPEG2_OLD 
   #include "mpeg2dec/video_out.h"
   #include "mpeg2dec/convert.h"
   #define mpeg2_sequence_t sequence_t
   #define mpeg2convert_rgb16 convert_rgb16
#else
   #include "mpeg2dec/video_out.h"
   #include "mpeg2dec/mpeg2convert.h"
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

bool mpegDecoder_t :: getPicture
   ( void const *&picture,
     picType_e   &type,
     unsigned     ptMask = ptAll_e )
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
            if( !haveVideoHeader_ )
            {
               haveVideoHeader_ = true ;
               mpegWidth_ = seq.width ;
               mpegHeight_ = seq.height ;
            }

            mpeg2_convert( DECODER, mpeg2convert_rgb16, NULL );
            mpeg2_custom_fbuf (DECODER, 0);
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
            break;
         }
         case STATE_PICTURE:
         {
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
                  picture = INFOPTR->display_fbuf->buf[0];
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
   } while( -1 != mpState_ );
   
   return false ;
}

static char const cPicTypes[] = {
   'I', 'P', 'B', 'D'
};

char mpegDecoder_t :: getPicType( picType_e t )
{
   if( (unsigned)t < sizeof( cPicTypes ) )
      return cPicTypes[t];
   else
      return '?' ;
}

