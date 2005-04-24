/*
 * Module mpegStream.cpp
 *
 * This module defines the methods of the mpegStream_t class
 * as declared in mpegStream.h
 *
 *
 * Change History : 
 *
 * $Log: mpegStream.cpp,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "mpegStream.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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
#define PACK_START_CODE             '\xba'
#define SYSTEM_HEADER_START_CODE    '\xbb'
  
/* mpeg2 */
#define PROGRAM_STREAM_MAP          '\xbc'
#define PRIVATE_STREAM_1            '\xbd'
#define PADDING_STREAM              '\xbe'
#define PRIVATE_STREAM_2            '\xbf'

#define AUDIO_ID                    '\xc0'
#define MAX_AUDIO_ID                '\xdf'
#define VIDEO_ID                    '\xe0'
#define MAX_VIDEO_ID                '\xef'

mpegStream_t :: mpegStream_t( void )
   : state_( startCode_e )
   , headerCode_( 0 )
   , packLength_( 0 )
   , numStreams_( 0 )
{
}

mpegStream_t :: ~mpegStream_t( void )
{
}

mpegStream_t :: frameType_e frameType( unsigned char headerCode )
{
   if( (AUDIO_ID <= headerCode) && (MAX_AUDIO_ID >= headerCode) )
      return mpegStream_t::audioFrame_e ;
   else if( (VIDEO_ID <= headerCode) && (MAX_VIDEO_ID >= headerCode) )
      return mpegStream_t::videoFrame_e ;
   else
      return mpegStream_t::otherFrame_e ;
}

long long mpegStream_t :: getPTS( unsigned char next, unsigned char pos) const 
{
   trace( "getPTS: pos %u, char 0x%02x\n", pos, next );
   switch( pos )
   {
      case 5:     return (long long)((next >> 1) & 0x07) << 30;
      case 4:     return ((long long)next) << 22 ;
      case 3:     return ((long long)(next>>1)) << 15 ;
      case 2:     return ((long long)next) << 7 ;
      case 1:     return ((long long)next) >> 1 ;
      default:    return 0LL ;
   }
}

bool mpegStream_t :: getFrame
   ( unsigned char const *fData,          // input: data to parse
     unsigned             length,         // input: bytes available
     frameType_e         &type,           // output: video or audio
     unsigned            &offset,         // output: offset of start of frame
     unsigned            &frameLen,       // output: bytes in frame
     long long           &pts,            // output: when to play, ms relative to start
     long long           &dts,            // output: when to decode, ms relative to start
     unsigned char       &streamId )      // output: which stream (if multiple)
{
   trace( "getFrame: %p/%u\n", fData, length );

   offset = 0 ;
   while( offset < length )
   {
      unsigned char const next = *fData++ ;
      offset++ ;
      
      trace( "inChar 0x%02x, state %d\n", next, state_ );

      switch( state_ )
      {
         case startCode_e:
         case startCode1_e:
            {
               if( 0 == next )
                  state_ = (state_e)(state_+1) ;
               else
                  state_ = startCode_e ;
               break;
            }
         case startCode2_e :
            {
               if( 1 == next )
                  state_ = (state_e)(state_+1) ;
               else if( 0 != next )
                  state_ = startCode_e ;
               break;
            }
         case startCode3_e :
            {
               headerCode_ = next ;
               if( (headerCode_ >= PACK_START_CODE )
                   &&
                   (headerCode_ <= MAX_VIDEO_ID ) )
               {
                  state_ = packLength_e ;
                  packLength_ = 0 ;
               }
               else
               {
                  trace( "Unsupported headerCode 0x%02x\n", headerCode_ );
                  state_ = startCode_e ;
               }
               break;
            }
         case packLength_e :
            {
               packLength_ = ((unsigned)next) << 8 ;
               state_ = packLength1_e ;
               break;
            }
         case packLength1_e :
            {
               packLength_ += next ;
               
trace( "packLength: 0x%04x, headerCode 0x%02x\n", packLength_, headerCode_ );

               if( ( PROGRAM_STREAM_MAP == headerCode_ )
                   ||
                   ( PADDING_STREAM == headerCode_ )
                   ||
                   ( PRIVATE_STREAM_2 == headerCode_ ) 
                   ||
                   ( SYSTEM_HEADER_START_CODE == headerCode_ ) )
               {
                  state_    = startCode_e ;
                  frameLen  = packLength_ ;
                  pts = dts = 0LL ;
                  streamId  = headerCode_ ;
                  type      = otherFrame_e ;
                  return true ;
               } // other packet
               else if( PACK_START_CODE == headerCode_ )
               {
trace( "pack start code..." );
                  state_ = eatBytes_e ;
                  if( 0x0100 == ( packLength_ & 0x0300 ) )
                  {
trace( "MPEG1\n" );
                     mpeg1_ = true ;
                     packLength_ = 6 ;
                  } // MPEG1
                  else
                  {
trace( "MPEG2\n" );
                     mpeg1_ = false ;
                     packLength_ = 8 ;
                  } // MPEG2 
               }
               else
                  state_ = skipStuff_e ;
               
               break;
            }
         case skipStuff_e :
            {
               if( ( mpeg1_ && ( next & '\x80' ) )
                   ||
                   ( !mpeg1_ && ( next == '\xff' ) ) )
               {
                  // eat
                  if( 0 < packLength_ )
                     --packLength_ ;
               }
               else
               {
                  pesHeaderLen_ = pesHeaderPos_ = 0 ;
                  unsigned char const next4 = ( 0xF0 & next );
                  unsigned char const next2 = ( '\xc0' & next4 );
                  if( '\x40' == next2 )
                  {
                     // buffer scale & size
                     state_ = bufferScale_e ;
                     packLength_ -= 2 ;
                  }
                  else if( ( 0x20 == next4 ) || ( 0x30 == next4 ) )
                  {
                     ptsFlags_ = next4 ;
                     ptsValue_ = 0LL ;
                     tsLeft_   = 5 ;
                     state_    = pts_e ;
                  }
                  else if( 0x80 == next2 )
                  {
                      --packLength_ ;
                      state_ = mpeg2Header_e ;
                      ptsFlags_ = next4 ;
                      if( 0 != ( next4 & 0x30 ) )
                         fprintf( stderr, "Encrypted multiplex not handled\n" );
                      fData++ ;
                      offset++ ;
                  }
                  else
                  {
                     fData++ ;
                     offset++ ;
                     --packLength_ ;
                     state_ = headerTail_e ; 
                  }

                  fData-- ;
                  offset-- ;
               }
               break;
            }
         case bufferScale_e :
            {
               state_ = (state_e)(state_+1);
               break;
            }
         case bufferSize_e :
            {
               state_ = (state_e)(state_+1);
               state_ = skipStuff_e ;
               break;
            }
         case pts_e :
            {
               ptsValue_ |= getPTS( next, tsLeft_-- );
               if( 0 == tsLeft_ )
               {
                  packLength_ -= 5 ;
                  if( 0x30 == ptsFlags_ )
                  {
                     tsLeft_   = 5 ;
                     dtsValue_ = 0LL ;
                     state_    = dts_e ;
                  }
                  else
                     state_ = headerTail_e ; 
               }
               break;
            }
         case dts_e :
            {
               dtsValue_ |= getPTS( next, tsLeft_-- );
               if( 0 == tsLeft_ )
               {
                  packLength_ -= 5 ;
                  state_ = headerTail_e ; 
               }
               break;
            }
         case mpeg2Header_e :
            {
               pesHeaderLen_ = next ;
               state_ = skipStuff_e ;
               break;
            }
         case headerTail_e :
            {
               if( pesHeaderPos_ < pesHeaderLen_ )
               {
                  pesHeaderData_[pesHeaderPos_++] = next ;
                  break;
               }
               else 
               {
                  offset-- ;
                  state_   = startCode_e ;
                  frameLen = packLength_ ;
                  pts      = ptsValue_ ;
                  dts      = dtsValue_ ;
                  streamId = headerCode_ ;
                  type     = frameType( headerCode_ );
                  return true ;
               }
            }
         case eatBytes_e :
            {
               if( 0 == --packLength_ )
                  state_ = startCode_e ;
               break;
            }
         default:
            {
               fprintf( stderr, "Invalid state: %d\n", state_ );
               exit(3);
            }
      }
   }
   return false ;
}


#ifdef __MODULETEST__
#include "hexDump.h"
#include <zlib.h>
#include <string.h>

static char const *cFrameTypes_[] = {
     "video"
   , "audio"
   , "other"
};

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      FILE *fIn = fopen(argv[1], "rb" );
      if( fIn )
      {
         mpegStream_t mpeg ;
         
         unsigned long frameCounts[5] = { 0, 0, 0, 0, 0 };
         unsigned long byteCounts[5] = { 0, 0, 0, 0, 0 };
         unsigned long crc[2] = { 0, 0 };

         unsigned long globalOffs = 0 ;
         unsigned char inBuf[4096];
         int  numRead ;
         while( 0 < ( numRead = fread( inBuf, 1, sizeof(inBuf)/2, fIn ) ) )
         {
            unsigned                  inOffs = 0 ;
            
            mpegStream_t::frameType_e type ;
            unsigned                  frameOffs ;
            unsigned                  frameLen ;
            long long                 pts ;
            long long                 dts ;
            unsigned char             streamId ;

            while( ( numRead > inOffs )
                   &&
                   mpeg.getFrame( inBuf+inOffs, 
                                  numRead-inOffs,
                                  type,
                                  frameOffs,
                                  frameLen,
                                  pts, dts,
                                  streamId ) )
            {
               fprintf( stderr, "%02x:%lu:%u:%u:%llu:%llu %02x %02x %02x\n", streamId, globalOffs, frameOffs, frameLen, pts, dts, inBuf[inOffs+frameOffs], inBuf[inOffs+frameOffs+1], inBuf[inOffs+frameOffs+2] );
               unsigned end = inOffs+frameOffs+frameLen ;
               if( end > numRead )
               {
                  unsigned left = end-numRead ;
                  unsigned max = sizeof(inBuf)-numRead ;
                  if( max > left )
                  {
                     int numRead2 = fread( inBuf+numRead, 1, left, fIn );
                     if( numRead2 == left )
                     {
                        fprintf( stderr, "tail end %u bytes\n", left );
                        globalOffs += numRead2 ;
                     }
                     else
                     {
                        fprintf( stderr, "packet underflow: %d of %u bytes read\n", numRead, left );
                        return 0 ;
                     }
                  }
                  else
                  {
                     fprintf( stderr, "packet overflow: %lu bytes needed, %lu bytes avail\n", left, max );
                     return 0 ;
                  }
               }

               fprintf( stderr, "offs %lx, frame type 0x%02x (%s), %u bytes\n", globalOffs, type, cFrameTypes_[type], frameLen );
               frameCounts[type]++ ;
               byteCounts[type] += frameLen ;
               
               unsigned char const *frameData = inBuf+inOffs+frameOffs ;
               if( mpegStream_t::otherFrame_e == type )
               {
                  hexDumper_t dump( frameData, frameLen );
                  while( dump.nextLine() )
                     fprintf( stderr, "%s\n", dump.getLine() );
               }
               else
               {
                  crc[type] = adler32( crc[type], frameData, frameLen );
               }

               inOffs += frameOffs+frameLen ;
            }
            globalOffs += numRead ;
         }
         for( unsigned i = 0 ; i < 2 ; i++ )
            printf( "%u %s frames, %u bytes, crc 0x%08lx\n", frameCounts[i], cFrameTypes_[i], byteCounts[i], crc[i] );
         fclose( fIn );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s fileName\n", argv[0] );

   return 0 ;
}

#endif 
