#ifndef __MPEGPS_H__
#define __MPEGPS_H__ "$Id: mpegPS.h,v 1.1 2006-08-24 23:59:43 ericn Exp $"

/*
 * mpegPS.h
 *
 * This header file declares the 
 *
 *
 * Change History : 
 *
 * $Log: mpegPS.h,v $
 * Revision 1.1  2006-08-24 23:59:43  ericn
 * -expose mpeg parsing function from mpegDecode
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

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

//
// Parses a transport header.
//    returns true and the packet details if found before end-of-data
//    returns false if not found
//
bool mpegps_read_packet
   ( unsigned char const      *&inData,         // input/output
     unsigned                  &bytesLeft,      // input/output
     unsigned                  &packetLen,      // output
     long long                 &when,           // output
     CodecType                 &codecType,      // output
     CodecID                   &codecId,        // output
     unsigned char             &streamId );     // output

#endif

