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
 * Revision 1.1  2002-11-24 19:08:58  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "madDecode.h"
#include <mad.h>

inline unsigned short scale( mad_fixed_t sample )
{
   return (unsigned short)( sample >> (MAD_F_FRACBITS-15) );
}

madDecoder_t :: madDecoder_t
   ( void const   *mp3Data,
     unsigned long numBytes )
   : headers_( mp3Data, numBytes ),
     worked_( headers_.worked() ),
     samples_( 0 ),
     numSamples_( 0 )
{
   if( worked_ )
   {
      //
      // we know :
      //    length in milliseconds
      //    number of channels
      //    samples/second
      //
      // so
      //    #samples = #channels * samples/second * milliseconds/1000
      //
      unsigned long const maxSamples = ( ( headers_.numChannels() 
                                           * headers_.playbackRate() 
                                           * headers_.lengthMilliseconds() )
                                         / 1000 ) + headers_.numChannels();
      unsigned short *sampleBuf = new unsigned short [ maxSamples ];
      samples_ = sampleBuf ;
      unsigned short * const sampleEnd = sampleBuf + maxSamples ;

      struct mad_stream stream;
      struct mad_frame	frame;
      struct mad_synth	synth;
      mad_stream_init(&stream);
      mad_stream_buffer(&stream, (unsigned char const *)mp3Data, numBytes );
      mad_frame_init(&frame);
      mad_synth_init(&synth);

      bool eof = false ;
      do {
         if( -1 != mad_frame_decode(&frame, &stream ) )
         {
            mad_synth_frame( &synth, &frame );
            if( 1 == headers_.numChannels() )
            {
               mad_fixed_t const *left = synth.pcm.samples[0];
               for( unsigned i = 0 ; ( i < synth.pcm.length ) && ( sampleBuf < sampleEnd ) ; i++ )
               {
                  *sampleBuf++ = scale( *left++ );
               }
            } // mono
            else
            {
               mad_fixed_t const *left  = synth.pcm.samples[0];
               mad_fixed_t const *right = synth.pcm.samples[1];
               for( unsigned i = 0 ; ( i < synth.pcm.length ) && ( sampleBuf < sampleEnd ) ; i++ )
               {
                  *sampleBuf++ = scale( *left++ );
                  *sampleBuf++ = scale( *right++ );
               }
            } // stereo
         } // frame decoded
         else
         {
            if( MAD_RECOVERABLE( stream.error ) )
               ;
            else if( MAD_ERROR_BUFLEN == stream.error )
            {
               eof = true ;
            }
            else
               break;
         }
      } while( !eof );

      mad_stream_finish(&stream);
   
      if( eof )
      {
         numSamples_ = sampleBuf - samples_ ;
      }
      else
      {
         worked_ = false ;
         delete [] (unsigned short *)samples_ ;
         samples_ = 0 ;
      }
   } // headers parsed
}

