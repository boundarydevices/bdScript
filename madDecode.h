#ifndef __MADDECODE_H__
#define __MADDECODE_H__ "$Id: madDecode.h,v 1.4 2003-08-04 03:13:40 ericn Exp $"

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
 *       while( mp3Decode.getData() )
 *       {
 *          unsigned short  samples[MAXSAMPLES];
 *          unsigned long   numRead ;
 *          while( mp3Decode.getSamples( samples, sizeof(samples[MAXSAMPLES), numRead ) )
 *             ... write data to audio driver ?
 *       }
 * 
 * Change History : 
 *
 * $Log: madDecode.h,v $
 * Revision 1.4  2003-08-04 03:13:40  ericn
 * -fixed comment
 *
 * Revision 1.3  2003/07/27 15:14:24  ericn
 * -modified to keep track of unread samples
 *
 * Revision 1.2  2003/07/20 15:42:29  ericn
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
   madDecoder_t( void );
   ~madDecoder_t( void );

   void feed( void const *inData, unsigned long inBytes );

   //
   // Parse data previously fed.
   // 
   // Returns true if one or more output frames is available.
   //
   // Call readSamples to actually get the data
   //
   bool getData( void );

   //
   // read (and convert) some of the samples.
   //
   // call numChannels to determine if mono or stereo
   //
   bool readSamples( unsigned short sampleBuf[],      // buffer to fill
                     unsigned       maxSamples,       // max #samples
                     unsigned      &numRead );        // number actually read

   inline unsigned numSamples( void ) const { return haveHeader_ ? numSamples_ : 0 ; }
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
   unsigned          numSamples_ ; // number of samples available
   unsigned          sampleStart_ ; // read up to this sample -1 
};

#endif

