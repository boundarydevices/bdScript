/*
 * Module mpegPS.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: mpegPS.cpp,v $
 * Revision 1.1  2006-08-24 23:59:45  ericn
 * -expose mpeg parsing function from mpegDecode
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "mpegPS.h"
#include <stdio.h>

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
     unsigned              &sizeInOut,    // input/output: numavail/numleft
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

#if 0
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
#endif

inline unsigned short be16( unsigned char const *&inData,
                            unsigned             &sizeInOut )
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
                          unsigned             &sizeInOut,          // 
                          signed char           initialChar = -1 )  // sometimes we don't know until we have it
{
   unsigned char const *bytesIn = (unsigned char const *)inData ;
   unsigned             size = sizeInOut ;

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

bool mpegps_read_packet
   ( unsigned char const      *&inData,         // input/output
     unsigned                  &bytesLeft,      // input/output
     unsigned                  &packetLen,      // output
     long long                 &when,           // output
     CodecType                 &codecType,      // output
     CodecID                   &codecId,        // output
     unsigned char             &streamId )      // output
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
   
       if (state >= 0x1e0 && state <= 0x1ef) {
           codecType = CODEC_TYPE_VIDEO;
           codecId = CODEC_ID_MPEG1VIDEO;
       } else if (state >= 0x1c0 && state <= 0x1df) {
           codecType = CODEC_TYPE_AUDIO;
           codecId = CODEC_ID_MP2;
       } else if (state >= 0x80 && state <= 0x9f) {
           codecType = CODEC_TYPE_AUDIO;
           codecId = CODEC_ID_AC3;
       } else {
          bytesLeft -= len ;
          inData += len ;
          goto redo;
       }
       
      streamId = (unsigned char)state ;
      packetLen  = len ;
      bytesLeft -= len ;
      when = pts ;
      return true ;
    }
    else
       return false ;
}

