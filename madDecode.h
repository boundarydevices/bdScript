#ifndef __MADDECODE_H__
#define __MADDECODE_H__ "$Id: madDecode.h,v 1.2 2003-07-20 15:42:29 ericn Exp $"

/*
 * madDecode.h
 *
 * This header file declares the madDecoder_t class, which 
 * is used to decode an MP3 file in pieces.
 *
 * General usage is something like this:
 *
 *    madDecoder_t mp3Decode ;
 *    while( more data in )
 *       mp3Decode.feed( data, length ) )
 *       unsigned short *samples ;
 *       unsigned long   length ;
 *       while( mp3Decode.getData( samples, length ) )
 *       {
 *          ... write data to audio driver ?
 *       }
 * 
 * Change History : 
 *
 * $Log: madDecode.h,v $
 * Revision 1.2  2003-07-20 15:42:29  ericn
 * -separated feed from read
 *
 * Revision 1.1  2002/11/24 19:08:55  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "mad.h"

class madDecoder_t {
public:
   madDecoder_t( unsigned maxSamples = 16384 );
   ~madDecoder_t( void );

   //
   // returns true if output is available, false otherwise
   //
   void feed( void const *inData, unsigned long inBytes );

   //
   // don't expect data to stay the same between iterations
   //
   bool getData( unsigned short const *&outData, unsigned &numSamples );


   inline bool haveHeader( void ) const { return haveHeader_ ; }
   inline unsigned sampleRate( void ) const { return sampleRate_ ; }
   inline unsigned numChannels( void ) const { return numChannels_ ; }

private:
   struct mad_stream mp3Stream_ ;
   struct mad_frame  mp3Frame_ ;
   struct mad_synth  mp3Synth_ ;
   bool              haveHeader_ ;
   unsigned          numChannels_ ;
   unsigned          sampleRate_ ; // HZ
   unsigned char     assEnd_[4096];
   unsigned short    assEndLength_ ;
   unsigned short   *samples_ ;
   unsigned const    maxSamples_ ;
   unsigned          numSamples_ ;
};

#endif

