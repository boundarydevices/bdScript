/*
 * Module madDecode.cpp
 *
 * This module defines the methods of the madDecoder_t
 * class as declared in madDecode.h
 *
 *
 * Change History : 
 *
 * $Log: madDecode.cpp,v $
 * Revision 1.4  2003-08-02 19:28:57  ericn
 * -remove debug statement
 *
 * Revision 1.3  2003/07/27 15:14:28  ericn
 * -modified to keep track of unread samples
 *
 * Revision 1.2  2003/07/20 15:42:32  ericn
 * -separated feed from read
 *
 * Revision 1.1  2002/11/24 19:08:58  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "madDecode.h"
#include <mad.h>
#include <stdio.h>
#include <assert.h>

inline unsigned short scale( mad_fixed_t sample )
{
   return (unsigned short)( sample >> (MAD_F_FRACBITS-15) );
}

madDecoder_t :: madDecoder_t( void )
   : haveHeader_( false ),
     numChannels_( 0 ),
     sampleRate_( 0 ),
     assEndLength_( 0 ),
     numSamples_( 0 ),
     sampleStart_( 0 )
{
   mad_stream_init(&mp3Stream_);
}

madDecoder_t :: ~madDecoder_t( void )
{
   mad_stream_finish(&mp3Stream_);
}

//
// returns true if output is available, false otherwise
//
void madDecoder_t :: feed( void const *inData, unsigned long inBytes )
{
   if( 0 != assEndLength_ )
   {
      if( sizeof( assEnd_ ) > inBytes + assEndLength_ )
      {
         memcpy( assEnd_+assEndLength_, inData, inBytes );
         assEndLength_ += inBytes ;
         mad_stream_buffer( &mp3Stream_, assEnd_, assEndLength_ );
      }
      else
      {
         fprintf( stderr, "packet too big %lu/%lu\n", assEndLength_, inBytes );
      }
      assEndLength_ = 0 ;
   }
   else
      mad_stream_buffer( &mp3Stream_, (unsigned char const *)inData, inBytes );
}
   
bool madDecoder_t :: getData( void )
{
   if( !haveHeader_ )
   {
      struct mad_header header; 
      if( -1 != mad_header_decode( &header, &mp3Stream_ ) )
      {
         numChannels_ = MAD_NCHANNELS(&header);
         sampleRate_  = header.samplerate;
         haveHeader_ = true ;
         mad_frame_init(&mp3Frame_);
         mad_synth_init(&mp3Synth_);
      }
      else
      {
         assEndLength_ = mp3Stream_.bufend-mp3Stream_.buffer ;
         memcpy( assEnd_, mp3Stream_.buffer, assEndLength_ );
         return false ;
      }
   }

   numSamples_ = 0 ;

   if( haveHeader_ )
   {
      do {
         if( -1 != mad_frame_decode(&mp3Frame_, &mp3Stream_ ) )
         {
            numChannels_ = MAD_NCHANNELS(&mp3Frame_.header);
            sampleRate_ = mp3Frame_.header.samplerate ;
            mad_synth_frame( &mp3Synth_, &mp3Frame_ );

            numSamples_ = mp3Synth_.pcm.length ;
            if( 0 < numSamples_ )
            {
               sampleStart_ = 0 ;
               break;
            }
         } // mp3Frame_ decoded
         else
         {
            if( MAD_RECOVERABLE( mp3Stream_.error ) )
               ;
            else 
            {
               if( MAD_ERROR_BUFLEN != mp3Stream_.error )
                  fprintf( stderr, "Error %d decoding audio mp3Frame_\n", mp3Stream_.error );
               else if( 0 != mp3Stream_.next_frame )
               {
                  assEndLength_ = mp3Stream_.bufend-mp3Stream_.next_frame ;
                  if( 0 < assEndLength_ )
                  {
                     memcpy( assEnd_, mp3Stream_.next_frame, assEndLength_ );
                  }
               }
               else
                  assEndLength_ = 0 ;
               break;
            }
         }
      } while( 1 );
   }

   if( 0 == numSamples_ )
   {
      return false ;
   }
   else
      return true ;
}

bool madDecoder_t :: readSamples( unsigned short samples[],
                                  unsigned       maxSamples,
                                  unsigned      &numRead )
{
   
   numRead = 0 ;

   unsigned short *nextOut = samples ;

   if( 1 == numChannels_ )
   {
      if( maxSamples > numSamples_ )
         maxSamples = numSamples_ ;

      mad_fixed_t const *left = mp3Synth_.pcm.samples[0] + sampleStart_ ;

      for( unsigned i = 0 ; i < maxSamples ; i++ )
         *nextOut++ = scale( *left++ );
      
      numSamples_  -= maxSamples ;
      sampleStart_ += maxSamples ;
   
   } // mono
   else
   {
      maxSamples /= 2 ;

      if( maxSamples > numSamples_ )
         maxSamples = numSamples_ ;

      mad_fixed_t const *left  = mp3Synth_.pcm.samples[0] + sampleStart_ ;
      mad_fixed_t const *right = mp3Synth_.pcm.samples[1] + sampleStart_ ;

      for( unsigned i = 0 ; i < maxSamples ; i++ )
      {
         *nextOut++ = scale( *left++ );
         *nextOut++ = scale( *right++ );
      }

      numSamples_  -= maxSamples ;
      sampleStart_ += maxSamples ;

   } // stereo

   numRead = nextOut - samples ;
   return 0 != numRead ;
}
