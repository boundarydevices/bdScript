#ifndef __MADDECODE_H__
#define __MADDECODE_H__ "$Id: madDecode.h,v 1.1 2002-11-24 19:08:55 ericn Exp $"

/*
 * madDecode.h
 *
 * This header file declares the madDecoder_t class, which 
 * is used to decode an entire MP3 file in one fell swoop.
 *
 * Change History : 
 *
 * $Log: madDecode.h,v $
 * Revision 1.1  2002-11-24 19:08:55  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#ifndef __MADHEADERS_H__
#include "madHeaders.h"
#endif

class madDecoder_t {
public:
   madDecoder_t( void const   *mp3Data,
                 unsigned long numBytes );
   ~madDecoder_t( void ){ if( samples_ ) delete [] samples_ ; }

   bool worked( void ) const { return worked_ ; }

   madHeaders_t const &headers( void ) const { return headers_ ; }

   unsigned short const *getSamples( void ) const { return samples_ ; }
   
   //
   // if stereo, includes sum of right and left, 
   // and they are interleaved in getSamples() return value
   //
   unsigned long         numSamples( void ) const { return numSamples_ ; }

private:
   madHeaders_t          headers_ ;
   bool                  worked_ ;
   unsigned short const *samples_ ;
   unsigned long         numSamples_ ;
};

#endif

