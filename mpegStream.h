#ifndef __MPEGSTREAM_H__
#define __MPEGSTREAM_H__ "$Id: mpegStream.h,v 1.3 2006-08-24 23:59:11 ericn Exp $"

/*
 * mpegStream.h
 *
 * This header file declares the mpegStream_t class, which
 * demuxes an mpeg stream a chunk at a time.
 *
 *
 * Change History : 
 *
 * $Log: mpegStream.h,v $
 * Revision 1.3  2006-08-24 23:59:11  ericn
 * -gutted in favor of mpegPS.h/.cpp
 *
 * Revision 1.2  2006/08/16 02:32:50  ericn
 * -add convenience function
 *
 * Revision 1.1  2005/04/24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */

class mpegStream_t {
public:
   mpegStream_t( void );
   ~mpegStream_t( void );

   typedef enum frameType_e {
      videoFrame_e = 1,
      audioFrame_e = 2,
      otherFrame_e = 4,
      needData_e
   };

   struct stream_t {
      frameType_e   type ;
      unsigned long id ;
      unsigned long codec_type ;
      unsigned long codec_id ;
   };

   //
   // Piece-wise demuxing interface. 
   //
   // Note that frameLen may extend beyond the input data. It's up to the app
   // to read more data from the source.
   //
   bool getFrame( unsigned char const *fData,          // input: data to parse
                  unsigned             length,         // input: bytes available
                  frameType_e         &type,           // output: video or audio
                  unsigned            &offset,         // output: offset of start of frame
                  unsigned            &frameLen,       // output: bytes in frame
                  long long           &pts,            // output: when to play, ms relative to start
                  long long           &dts,            // output: when to decode, ms relative to start
                  unsigned char       &streamId );     // output: which stream if video or audio, frame type if other

   stream_t const &getStream( unsigned char streamId ) const { return streams_[streamId]; }

   inline static long long ptsToMs( long long pts ){ return pts/90 ; }

private:
   long long getPTS(unsigned char next,unsigned char pos) const ;

   unsigned      numStreams_ ;
   stream_t      streams_[4];
};


#endif

