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
 * Revision 1.2  2003-07-20 15:42:32  ericn
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

madDecoder_t :: madDecoder_t( unsigned maxSamples )
   : haveHeader_( false ),
     numChannels_( 0 ),
     sampleRate_( 0 ),
     assEndLength_( 0 ),
     samples_( 0 ),
     maxSamples_( maxSamples ),
     numSamples_( 0 )
{
   mad_stream_init(&mp3Stream_);
}

madDecoder_t :: ~madDecoder_t( void )
{
   mad_stream_finish(&mp3Stream_);
   if( samples_ )
      delete [] samples_ ;
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
         fprintf( stderr, "packet too big %lu/%lu\n", assEndLength_, inBytes );
      assEndLength_ = 0 ;
   }
   else
      mad_stream_buffer( &mp3Stream_, (unsigned char const *)inData, inBytes );
}
   
bool madDecoder_t :: getData( unsigned short const *&outData, unsigned &numSamples )
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
         return false ;
   }

   if( haveHeader_ )
   {
      do {
         if( -1 != mad_frame_decode(&mp3Frame_, &mp3Stream_ ) )
         {
            numChannels_ = MAD_NCHANNELS(&mp3Frame_.header);
            sampleRate_ = mp3Frame_.header.samplerate ;
            mad_synth_frame( &mp3Synth_, &mp3Frame_ );

            if( 0 == samples_ )
               samples_ = new unsigned short [maxSamples_];
            
            if( samples_ )
            {
               unsigned short *nextOut = samples_ + numSamples_ ;
               if( 1 == numChannels_ )
               {
                  mad_fixed_t const *left = mp3Synth_.pcm.samples[0];

                  for( unsigned i = 0 ; i < mp3Synth_.pcm.length ; i++ )
                  {
                     assert( numSamples_ < maxSamples_ );
                     unsigned short const sample = scale( *left++ );
                     *nextOut++ = sample ;
                     *nextOut++ = sample ;
                     numSamples_ += 2 ;
                  }
               } // mono
               else
               {
                  mad_fixed_t const *left  = mp3Synth_.pcm.samples[0];
                  mad_fixed_t const *right = mp3Synth_.pcm.samples[1];

                  for( unsigned i = 0 ; i < mp3Synth_.pcm.length ; i++ )
                  {
                     assert( numSamples_ < maxSamples_ );
                     *nextOut++ = scale( *left++ );
                     *nextOut++ = scale( *right++ );
                     numSamples_ += 2 ;
                  }
               } // stereo
            }
            break;
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
                  memcpy( assEnd_, mp3Stream_.next_frame, assEndLength_ );
//                                                   fprintf( stderr, "tail mp3Frame_ %p/%p/%lu, %lu\n", mp3Stream_.next_frame, mp3Stream_.main_data, mp3Stream_.md_len, mp3Stream_.bufend-mp3Stream_.next_frame );
               }
               else
                  assEndLength_ = 0 ;
               break;
            }
         }
      } while( 1 );
   }

   outData = samples_ ;
   numSamples = numSamples_ ;
   numSamples_ = 0 ;
   return ( 0 < numSamples );
}


