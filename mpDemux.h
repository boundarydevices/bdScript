#ifndef __MPDEMUX_H__
#define __MPDEMUX_H__ "$Id: mpDemux.h,v 1.4 2003-07-24 13:44:09 ericn Exp $"

/*
 * mpDemux.h
 *
 * This header file declares the mpegDemux_t
 * class, which is used to demux MPEG-2 video files 
 * with associated MP3 audio.
 *
 *
 * Change History : 
 *
 * $Log: mpDemux.h,v $
 * Revision 1.4  2003-07-24 13:44:09  ericn
 * -updated to use ptr/length
 *
 * Revision 1.3  2003/07/20 19:03:56  ericn
 * -added comment
 *
 * Revision 1.2  2003/07/20 18:36:05  ericn
 * -added PTS interface
 *
 * Revision 1.1  2003/07/20 15:43:13  ericn
 * -Initial version, using libavformat
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include <stdint.h>
#include <string.h>

class mpegDemux_t {
public:
   mpegDemux_t( void const *data,
                unsigned    dataLen );
   ~mpegDemux_t( void );

   typedef enum frameType_e {
      videoFrame_e,
      audioFrame_e,
      endOfFile_e
   };

   struct stream_t {
      frameType_e   type ;
      unsigned long id ;
      unsigned long codec_type ;
      unsigned long codec_id ;
   };
   
   // piece-wise demuxing interface
   frameType_e getFrame( void const   *&fData,        // audio or video data
                         unsigned long &length,       // #bytes
                         long long     &when_ms,      // when to play, ms relative to start
                         unsigned char &streamId );   // which stream (if multiple)

   // note that you only really know at EOF
   inline unsigned getNumStreams( void ) const { return streams_.numStreams_ ; }
   inline stream_t const &getStream( unsigned char id ) const { return streams_.streams[id]; }

   // bulk demuxing interface
   struct frame_t {
      void const   *data_ ;         // audio or video data
      unsigned long length_ ;       // #bytes
      long long     when_ms_ ;      // when to play, ms relative to start
   };

   struct streamAndFrames_t {
      stream_t sInfo_ ;
      unsigned numFrames_ ;
      frame_t  frames_[1];
   };

   struct bulkInfo_t {
      unsigned           count_ ;
      streamAndFrames_t *streams_[1];

      static void clear( bulkInfo_t const * ); // use in place of delete
   };

   // remember to delete when done.
   bulkInfo_t const *getFrames();

   // reset to allow reading again
   inline void reset( void ){ nextIn_ = startData_ ; bytesLeft_ = fileSize_ ; streams_.numStreams_ = 0 ; }

private:
   struct streamSet_t {
      enum { maxStreams_ = 8 };
   
      unsigned char numStreams_ ;
      stream_t  streams[maxStreams_];
   
      streamSet_t( void ){ memset( this, 0, sizeof( *this ) ); }
   };

   typedef enum state_e {
      end_e,
      processing_e
   };

   state_e             state_ ;

   unsigned char const * const startData_ ;
   unsigned char const        *nextIn_ ;
   unsigned long         const fileSize_ ;
   unsigned long               bytesLeft_ ;
   streamSet_t                 streams_ ;

   friend int main( int argc, char const * const argv[] );
};

#endif

