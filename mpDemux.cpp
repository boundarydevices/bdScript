/*
 * Module mpDemux.cpp
 *
 * This module defines the methods of the mpegDemuxr_t 
 * class as declared in mpDemux.h
 *
 *
 * Change History : 
 *
 * $Log: mpDemux.cpp,v $
 * Revision 1.6  2003-07-27 15:13:43  ericn
 * -added time summary to bulk info
 *
 * Revision 1.5  2003/07/24 13:44:11  ericn
 * -updated to use ptr/length
 *
 * Revision 1.4  2003/07/20 19:06:12  ericn
 * -fixed small leak
 *
 * Revision 1.3  2003/07/20 18:43:04  ericn
 * -fixed stand-alone program to match module interface
 *
 * Revision 1.2  2003/07/20 18:36:07  ericn
 * -added PTS interface
 *
 * Revision 1.1  2003/07/20 15:43:16  ericn
 * -Initial version, using libavformat
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "mpDemux.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/mman.h>

int doTrace = 0 ;

inline void trace( char const *msg, ... )
{
   if( doTrace )
   {
      va_list ap ;
      va_start( ap, msg );

      vfprintf( stderr, msg, ap );
      va_end( ap );
      fflush( stdout );
   }
}

/*********************************************/
/* demux code */
enum CodecID {
    CODEC_ID_NONE, 
    CODEC_ID_MPEG1VIDEO,
    CODEC_ID_H263,
    CODEC_ID_RV10,
    CODEC_ID_MP2,
    CODEC_ID_MP3LAME,
    CODEC_ID_VORBIS,
    CODEC_ID_AC3,
    CODEC_ID_MJPEG,
    CODEC_ID_MJPEGB,
    CODEC_ID_MPEG4,
    CODEC_ID_RAWVIDEO,
    CODEC_ID_MSMPEG4V1,
    CODEC_ID_MSMPEG4V2,
    CODEC_ID_MSMPEG4V3,
    CODEC_ID_WMV1,
    CODEC_ID_WMV2,
    CODEC_ID_H263P,
    CODEC_ID_H263I,
    CODEC_ID_SVQ1,
    CODEC_ID_DVVIDEO,
    CODEC_ID_DVAUDIO,
    CODEC_ID_WMAV1,
    CODEC_ID_WMAV2,
    CODEC_ID_MACE3,
    CODEC_ID_MACE6,
    CODEC_ID_HUFFYUV,

    /* various pcm "codecs" */
    CODEC_ID_PCM_S16LE,
    CODEC_ID_PCM_S16BE,
    CODEC_ID_PCM_U16LE,
    CODEC_ID_PCM_U16BE,
    CODEC_ID_PCM_S8,
    CODEC_ID_PCM_U8,
    CODEC_ID_PCM_MULAW,
    CODEC_ID_PCM_ALAW,

    /* various adpcm codecs */
    CODEC_ID_ADPCM_IMA_QT,
    CODEC_ID_ADPCM_IMA_WAV,
    CODEC_ID_ADPCM_MS,
};

enum CodecType {
    CODEC_TYPE_UNKNOWN = -1,
    CODEC_TYPE_VIDEO,
    CODEC_TYPE_AUDIO,
};

#define MAX_SYNC_SIZE 100000
#define PACK_START_CODE             ((unsigned int)0x000001ba)
#define SYSTEM_HEADER_START_CODE    ((unsigned int)0x000001bb)
#define SEQUENCE_END_CODE           ((unsigned int)0x000001b7)
#define PACKET_START_CODE_MASK      ((unsigned int)0xffffff00)
#define PACKET_START_CODE_PREFIX    ((unsigned int)0x00000100)
#define ISO_11172_END_CODE          ((unsigned int)0x000001b9)
  
/* mpeg2 */
#define PROGRAM_STREAM_MAP 0x1bc
#define PRIVATE_STREAM_1   0x1bd
#define PADDING_STREAM     0x1be
#define PRIVATE_STREAM_2   0x1bf


#define AUDIO_ID 0xc0
#define VIDEO_ID 0xe0

static bool find_start_code
   ( unsigned char const  *&inBuf,        // input: next byte in
     unsigned long         &sizeInOut,    // input/output: numavail/numleft
     unsigned long         &stateInOut )  // input/output: header state
{
   unsigned long state = stateInOut ;
   unsigned long left = sizeInOut ;
   unsigned char const *buf = inBuf ;
   bool worked = false ;

   while( !worked && ( left > 0 ) )
   {
      unsigned char const c = *buf++ ;
      left-- ;
      worked = ( 1 == state );
      state = ((state << 8) | c) & 0xffffff ;
   }

    
   sizeInOut  = left ;
   stateInOut = state ;
   inBuf      = buf ;
   return worked ;
}

static bool looksLikeMPEG( unsigned char const *inData, unsigned long inSize )
{
   unsigned long state = 0xFF ;
   if( find_start_code( inData, inSize, state ) )
   {
      return (    state == PACK_START_CODE 
               || state == SYSTEM_HEADER_START_CODE 
               || (state >= 0x1e0 && state <= 0x1ef) 
               || (state >= 0x1c0 && state <= 0x1df) 
               || state == PRIVATE_STREAM_2 
               || state == PROGRAM_STREAM_MAP 
               || state == PRIVATE_STREAM_1 
               || state == PADDING_STREAM );
   }
   else
      return false ;
}


inline unsigned short be16( unsigned char const *&inData,
                            unsigned long        &sizeInOut )
{
   if( sizeInOut >= 2 )
   {
      unsigned short const result = ( (unsigned short)inData[0] << 8 ) | inData[1];
      inData += 2 ;
      sizeInOut -= 2 ;
      return result ;
   }
   else
      return 0xFFFF ;
}

static long long get_pts( unsigned char const *&inData,             // i
                          unsigned long        &sizeInOut,          // 
                          signed char           initialChar = -1 )  // sometimes we don't know until we have it
{
   unsigned char const *bytesIn = (unsigned char const *)inData ;
   unsigned long        size = sizeInOut ;

   if( initialChar < 0 )
   {
      initialChar = *bytesIn++ ;
      size-- ;
   }

   long long pts ;
   if( size >= 4 )
   {
      pts = (long long)((initialChar >> 1) & 0x07) << 30;
      unsigned short val = be16( bytesIn, size );
      pts |= (long long)(val >> 1) << 15;
      val = be16( bytesIn, size );
      pts |= (long long)(val >> 1);
   }
   else
      pts = -1LL ;

   inData    = bytesIn ;
   sizeInOut = size ;
   return pts;
}

static bool mpegps_read_packet
   ( unsigned char const      *&inData,         // input/output
     unsigned long             &bytesLeft,      // input/output
     mpegDemux_t::streamSet_t  &streams,        // input/output: streams added here
     unsigned char             &streamIndex,    // output
     unsigned char const      *&packetData,     // output
     unsigned long             &packetLen,      // output
     long long                 &when )
{
    long long pts, dts;
    int flags ;
    unsigned long state ;

redo:
    state = 0xff;
    if( find_start_code( inData, bytesLeft, state ) )
    {
       if (state == PACK_START_CODE)
       {   
           goto redo;
       }
       else if (state == SYSTEM_HEADER_START_CODE)
       {   
           goto redo;
       }
       else if (state == PADDING_STREAM ||
                state == PRIVATE_STREAM_2)
       {
           /* skip them */
           unsigned short len = be16( inData, bytesLeft );
           if( len < bytesLeft )
           {
              inData    += len ;
              bytesLeft -= len ;
              goto redo;
           }
           else
           {
              return false ;
           }
       }
       else if( !((state >= 0x1c0 && state <= 0x1df) ||
                  (state >= 0x1e0 && state <= 0x1ef) ||
                  (state == 0x1bd)))
       {
           goto redo;
       }

       unsigned short len = be16( inData, bytesLeft );
       pts = 0 ;
       dts = 0 ;

       /* stuffing */
       unsigned char nextChar ;
       for(;;) {
           nextChar = *inData++ ;
           bytesLeft-- ;
           len-- ;
           /* XXX: for mpeg1, should test only bit 7 */
           if( nextChar != 0xff ) 
               break;
       }

       if ((nextChar & 0xc0) == 0x40) {
           /* buffer scale & size */
           inData++ ; bytesLeft-- ;
           nextChar = *inData++ ; bytesLeft-- ;
           len -= 2;
       }

       if ((nextChar & 0xf0) == 0x20) {
           pts = get_pts( inData, bytesLeft, nextChar );
           len -= 4;
       } else if ((nextChar & 0xf0) == 0x30) {
           pts = get_pts( inData, bytesLeft, nextChar );
           dts = get_pts( inData, bytesLeft );
           len -= 9;
       } else if ((nextChar & 0xc0) == 0x80) {
           /* mpeg 2 PES */
           if ((nextChar & 0x30) != 0) {
               fprintf(stderr, "Encrypted multiplex not handled\n");
               return false ;
           }
           flags = *inData++ ; bytesLeft-- ;
           unsigned header_len = *inData++ ; bytesLeft-- ;
           len -= 2;
           if (header_len > len)
               goto redo;
           if ((flags & 0xc0) == 0x80) {
               pts = get_pts( inData, bytesLeft );
               header_len -= 5;
               len -= 5;
           } if ((flags & 0xc0) == 0xc0) {
               pts = get_pts( inData, bytesLeft );
               dts = get_pts( inData, bytesLeft );
               header_len -= 10;
               len -= 10;
           }
           len -= header_len;
           inData += header_len ;
           bytesLeft -= header_len ;
       }
       if (state == 0x1bd) {
           state = *inData++ ; bytesLeft-- ;
           len--;
           if (state >= 0x80 && state <= 0xbf) {
               /* audio: skip header */
               inData    += 3 ;
               bytesLeft -= 3 ;
               len       -= 3;
           }
       }
   
       /* now find stream */
       for( unsigned i = 0 ; i < streams.numStreams_ ; i++ ) {
          mpegDemux_t::stream_t &st = streams.streams[i];
          if( st.id == state )
          {
             streamIndex = i ;
             goto found;
          }
       }

       unsigned long type, id ;
       if (state >= 0x1e0 && state <= 0x1ef) {
           type = CODEC_TYPE_VIDEO;
           id = CODEC_ID_MPEG1VIDEO;
       } else if (state >= 0x1c0 && state <= 0x1df) {
           type = CODEC_TYPE_AUDIO;
           id = CODEC_ID_MP2;
       } else if (state >= 0x80 && state <= 0x9f) {
           type = CODEC_TYPE_AUDIO;
           id = CODEC_ID_AC3;
       } else {
          bytesLeft -= len ;
          inData += len ;
          goto redo;
       }
       
       if( streams.numStreams_ < streams.maxStreams_ )
       {
          mpegDemux_t::stream_t &st = streams.streams[streams.numStreams_];

          st.type       = (mpegDemux_t::frameType_e)type ;
          st.id         = state ;
          st.codec_type = type ;
          st.codec_id   = id ;
          streamIndex   = streams.numStreams_++ ;
       }
       else
       {
          bytesLeft -= len ;
          inData += len ;
          goto redo ;
       }

found:
      packetData = inData ;      inData += len ;
      packetLen  = len ;         bytesLeft -= len ;
      when       = pts ;
      return true ;
    }
    else
       return false ;
}


mpegDemux_t :: mpegDemux_t
   ( void const *data,
     unsigned    dataLen )
   : state_( end_e ),
     startData_( (unsigned char const *)data ),
     nextIn_( 0 ),
     fileSize_( dataLen ),
     bytesLeft_( 0 )
{
}


mpegDemux_t :: ~mpegDemux_t( void )
{
}

inline long long pts_to_ms( long long pts )
{
   return ( pts / 90 );
}

mpegDemux_t :: frameType_e 
   mpegDemux_t :: getFrame
      ( void const  *&fData,
        unsigned long &length,
        long long     &when_ms,
        unsigned char &streamId )
{
   fData = 0 ;
   length = 0 ;
   when_ms = -1 ;
   unsigned char const *packetData ;    // output
   unsigned long        packetLen ;      // output
   long long            when_pts ;
   if( mpegps_read_packet( nextIn_, bytesLeft_, streams_, streamId, packetData, packetLen, when_pts ) )
   {
      frameType_e ft = endOfFile_e ;

      switch( streams_.streams[streamId].codec_type )
      {
         case CODEC_TYPE_VIDEO: ft = videoFrame_e ; break ;
         case CODEC_TYPE_AUDIO: ft = audioFrame_e ; break ;
         default: fprintf( stderr, "Unknown type %lu\n", streams_.streams[streamId].codec_type );
      }
      
      if( endOfFile_e != ft )
      {
         fData = packetData ;
         length = packetLen ;
         when_ms = when_pts / 90 ;
      }
      else
         state_ = end_e ;
      return ft ;
   }
   else
   {
      state_ = end_e ;
      return endOfFile_e ;
   }
}

void mpegDemux_t :: bulkInfo_t :: clear( mpegDemux_t :: bulkInfo_t const *bi )
{
   for( unsigned sIdx = 0 ; sIdx < bi->count_ ; sIdx++ )
   {
      streamAndFrames_t * const sAndF = bi->streams_[sIdx];
      free( (void *)bi->streams_[sIdx] );
   }
   free( (void *)bi );
}

mpegDemux_t :: bulkInfo_t const *mpegDemux_t :: getFrames( void )
{
   long long startMs = 0x7fffffffffffffffLL ;
   long long endMs = 0 ;

   if( ( 0 != startData_ ) && ( 0 != fileSize_ ) )
   {
      //
      // reset (in case this object has been used
      //
      reset();

      //
      // read entire file, counting frames by stream
      // (and indirectly counting streams)
      //
      unsigned frameCountByStream[ streamSet_t::maxStreams_ ];
      memset( frameCountByStream, 0, sizeof( frameCountByStream ) );
      
      mpegDemux_t :: frameType_e ft ;
      
      void const   *fData ;        // audio or video data
      unsigned long length ;       // #bytes
      long long     when_ms ;      // when to play, ms relative to start
      unsigned char streamId ;   // which stream (if multiple)

      while( endOfFile_e != ( ft = getFrame( fData, length, when_ms, streamId ) ) )
      {
         ++frameCountByStream[streamId];
      }

      //
      // allocate
      //
      unsigned const numStreams = getNumStreams();
      bulkInfo_t * const bi = (bulkInfo_t *)malloc( sizeof( bulkInfo_t )
                                                    + 
                                                    (numStreams*sizeof(streamAndFrames_t *) ) );
      bi->count_ = numStreams ;
      for( unsigned char sIdx = 0 ; sIdx < numStreams; sIdx++ )
      {
         unsigned streamSize = sizeof(streamAndFrames_t)
                             + frameCountByStream[sIdx]*sizeof(frame_t);
         streamAndFrames_t * const sAndF = (streamAndFrames_t *)malloc( streamSize );
         sAndF->sInfo_     = getStream( sIdx );
         sAndF->numFrames_ = 0 ; // used as counter below

         bi->streams_[sIdx] = sAndF ;
      }

      bi->streams_[numStreams] = 0 ; // just in case

      //
      // now parse again, saving frame data by stream
      // 
      reset();
      while( endOfFile_e != ( ft = getFrame( fData, length, when_ms, streamId ) ) )
      {
         streamAndFrames_t * const sAndF = bi->streams_[streamId];
         frame_t &f = sAndF->frames_[sAndF->numFrames_++];
         f.data_    = fData ;
         f.length_  = length ;
         f.when_ms_ = when_ms ;
         if( when_ms < startMs )
         {
            startMs = when_ms ;
         }
         if( when_ms > endMs )
         {
            endMs = when_ms ;
         }
      }
printf( "start %llx, end %llx\n", startMs, endMs );
      bi->msTotal_ = endMs - startMs ;

      return bi ;
   }
   else
      return 0 ;
}


// #define STANDALONE
#ifdef STANDALONE

static char const * const frameTypes_[] = {
   "video",
   "audio",
   "endOfFile"
};

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>
#include "memFile.h"

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         unsigned long numPackets[2] = { 0, 0 };
         unsigned long numBytes[2] = { 0, 0 };
         unsigned long crc[2] = { 0, 0 };
         
         mpegDemux_t demuxer( fIn.getData(), fIn.getLength() );

         memset( &numPackets, 0, sizeof( numPackets ) );
         memset( &numBytes, 0, sizeof( numBytes ) );
         memset( &crc, 0, sizeof( crc ) );
         printf( "reading in bulk\n" );
         mpegDemux_t::bulkInfo_t const * const bi = demuxer.getFrames();
         for( unsigned char sIdx = 0 ; sIdx < bi->count_ ; sIdx++ )
         {
            mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[sIdx];
            mpegDemux_t::frameType_e ft = sAndF.sInfo_.type ;
            printf( "%s stream %u: %u frames\n", frameTypes_[ft], sIdx, sAndF.numFrames_ );
            numPackets[ft] += sAndF.numFrames_ ;
            for( unsigned f = 0 ; f < sAndF.numFrames_ ; f++ )
            {
               mpegDemux_t::frame_t const &frame = sAndF.frames_[f];
               numBytes[ft] += frame.length_ ;
               crc[ft] = adler32( crc[ft], (unsigned char const *)frame.data_, frame.length_ );
               printf( "%7llu: %s/%lu\n", frame.when_ms_, frameTypes_[ft], frame.length_ );
            }
         }
         
         for( unsigned i = 0 ; i < mpegDemux_t::endOfFile_e ; i++ )
            printf( "%4lu %s packets, %7lu bytes, crc 0x%08lx\n", numPackets[i], frameTypes_[i], numBytes[i], crc[i] );

         printf( "done\ndeleting\n" );
         bi->clear( bi );
         printf( "done\n" );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: mpDemux fileName\n" );

   return 0 ;
}

#endif
