#ifndef __MADHEADERS_H__
#define __MADHEADERS_H__ "$Id: madHeaders.h,v 1.1 2002-11-05 05:42:15 ericn Exp $"

/*
 * madHeaders.h
 *
 * This header file declares the madHeaders_t
 * class, which tries to parse an MP3 file, looking
 * for a set of frames and ID3 tags.
 *
 *
 * Change History : 
 *
 * $Log: madHeaders.h,v $
 * Revision 1.1  2002-11-05 05:42:15  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <vector>

typedef std::vector<unsigned long> mp3FrameSet_t ;

class madHeaders_t {
public:
   madHeaders_t( void const *data,
                 unsigned long length );
   ~madHeaders_t( void );

   // does it look like an MP3 file at all?
   bool worked( void ) const { return worked_ ; }

   mp3FrameSet_t const &frames( void ) const { return frames_ ; }

   unsigned long lengthSeconds( void ) const { return numSeconds_ ; }
   unsigned long playbackFrequency( void ) const { return playbackRate_ ; }
   unsigned long numChannels( void ) const { return numChannels_ ; }

   bool          worked_ ;
   mp3FrameSet_t frames_ ;
   unsigned long numSeconds_ ;
   unsigned long playbackRate_ ;
   unsigned long numChannels_ ;
};


#endif

