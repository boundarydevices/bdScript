/*
 * Module mpegDecode.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: mpegDecode.cpp,v $
 * Revision 1.2  2003-07-20 18:35:38  ericn
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
#include <mpeg2dec/mpeg2.h>
#include "mpeg2dec/video_out.h"
#include "mpeg2dec/convert.h"
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
fprintf( stderr, "----> sequence %dx%d\n", INFOPTR->sequence->width, INFOPTR->sequence->height );
            vo_setup_result_t setup_result;

            if( !haveVideoHeader_ )
            {
               haveVideoHeader_ = true ;
               mpegWidth_ = INFOPTR->sequence->width ;
               mpegHeight_ = INFOPTR->sequence->height ;
            }

            mpeg2_convert( DECODER, convert_rgb16, NULL );
            mpeg2_custom_fbuf (DECODER, 0);
            
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

