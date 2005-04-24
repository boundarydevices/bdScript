#ifndef __MPEGFILE_H__
#define __MPEGFILE_H__ "$Id: mpegFile.h,v 1.1 2005-04-24 18:55:03 ericn Exp $"

/*
 * mpegFile.h
 *
 * This header file declares the mpegFile_t class, 
 * which is used to read mpeg data from a file
 *
 *
 * Change History : 
 *
 * $Log: mpegFile.h,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#ifndef __MPEGSTREAM_H__
#include "mpegStream.h"
#endif 

class mpegFile_t {
public:
   mpegFile_t( char const *filename );
   ~mpegFile_t( void );

   inline bool isOpen( void ) const { return 0 <= fd_ ; }
   inline bool endOfFile( void ) const { return !isOpen() && ( 0 == buffered_ ); }

   inline unsigned framesQueued( void ) const { return framesQueued_ ; }
   inline unsigned bytesQueued( void ) const { return bytesQueued_ ; }

   //
   // returns true and frame details if more frames of the specified type are present,
   //
   bool getFrame( unsigned                   whichTypes,    // input: OR of mpegStream_t::frameType_e bits
                  mpegStream_t::frameType_e &type,          // output: video or audio
                  unsigned char            *&frame,         // output: video or audio data
                  unsigned                  &frameLen,      // output: length of frame in bytes
                  long long                 &pts,           // output: when to play, ms relative to start
                  long long                 &dts,           // output: when to decode, ms relative to start
                  unsigned char             &streamId );    // output: which stream if video or audio, frame type if other

private:
   void bufferFrame(                   
            mpegStream_t::frameType_e  type,
            unsigned char             *frame,
            unsigned                   frameLen,
            long long                  pts,
            long long                  dts,
            unsigned char              streamId );

   struct bufEntry_t {
      bufEntry_t                *next_ ;
      long long                  pts_ ;
      long long                  dts_ ;
      mpegStream_t::frameType_e  type_ ;
      unsigned char              streamId_ ;
      unsigned                   length_ ;
      unsigned char              data_[1];
   };

   int           fd_ ;
   unsigned char inBuf_[4096];
   int           numRead_ ;
   unsigned      inOffs_ ;
   unsigned      frameLen_ ;
   unsigned      framesQueued_ ;
   unsigned      bytesQueued_ ;
   bufEntry_t   *buffered_ ;
   bufEntry_t   *bufTail_ ;
   bufEntry_t   *free_ ;
   mpegStream_t *parser_ ;
};


#endif

