#ifndef __MPEGDECODE_H__
#define __MPEGDECODE_H__ "$Id: mpegDecode.h,v 1.1 2003-07-20 15:43:35 ericn Exp $"

/*
 * mpegDecode.h
 *
 * This header file declares the mpegDecoder_t 
 * class, which is an push-driven interface to the
 * mpeg2 decoder library, designed to be used in 
 * conjunction with the mpegDemux_t class as declared
 * in mpDemux.h
 *
 * general usage is :
 *
 *    mpegDecoder_t decoder ;
 *    decoder.feed( data, bytes );
 *    while( decoder.getPicture( picture, mask ) )
 *       do something...
 *
 * Change History : 
 *
 * $Log: mpegDecode.h,v $
 * Revision 1.1  2003-07-20 15:43:35  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


class mpegDecoder_t {
public:
   mpegDecoder_t( void );
   ~mpegDecoder_t( void );

   enum picType_e {
      ptI_e       = 0,    // I (Independent) video frame (see http://www.disctronics.co.uk/technology/video/video_mpeg.htm)
      ptP_e       = 1,    // P (Predicted) video frame
      ptB_e       = 2,    // B (Bidirectional) video frame
      ptD_e       = 3,    // D (???) video frame 
      ptUnknown_e = 4,
      ptNoB_e     = 3,
      ptAll_e     = 15
   };

   void feed( void const   *inData, 
              unsigned long inBytes );

   bool getPicture( void const *&picture,
                    picType_e   &type,
                    unsigned     ptMask = ptAll_e );

   inline bool haveHeader( void ) const { return haveVideoHeader_ ; } 
   inline unsigned short width( void ) const { return mpegWidth_ ; }
   inline unsigned short height( void ) const { return mpegHeight_ ; }
   inline unsigned numSkipped( void ) const { return numSkipped_ ; }
   inline unsigned numParsed( void ) const { return numParsed_ ; }
   inline unsigned numDrawn( void ) const { return numDraw_ ; }

private:
   typedef enum state_e {
      end_e,
      processing_e
   };

   state_e              state_ ;
   int                  mpState_ ;
   void                *mpegDecoder_ ;
   void        const   *mpegInfo_ ;
   bool                 haveVideoHeader_ ;
   unsigned short       mpegWidth_ ;
   unsigned short       mpegHeight_ ;
   unsigned char        mpegFrameType_ ; // 
   bool                 skippedP_ ;
   unsigned             numSkipped_ ;
   unsigned             numParsed_ ;
   unsigned             numDraw_ ;
};


#endif

