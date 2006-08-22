#ifndef __MPEGDECODE_H__
#define __MPEGDECODE_H__ "$Id: mpegDecode.h,v 1.6 2006-08-22 15:49:40 ericn Exp $"

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
 * Revision 1.6  2006-08-22 15:49:40  ericn
 * -remove GOP, separate mask from picType
 *
 * Revision 1.5  2006/08/16 02:29:19  ericn
 * -add temp_ref, timestamp to feed/pull interface
 *
 * Revision 1.4  2006/07/30 21:36:17  ericn
 * -show GOPs
 *
 * Revision 1.3  2004/10/30 18:55:31  ericn
 * -Neon YUV support
 *
 * Revision 1.2  2003/07/20 18:35:35  ericn
 * -added picType, decode timing
 *
 * Revision 1.1  2003/07/20 15:43:35  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include <sys/time.h>
#include <list>

class mpegDecoder_t {
public:
   mpegDecoder_t( void );
   ~mpegDecoder_t( void );

   enum picType_e {
      ptI_e       = 0,    // I (Independent) video frame (see http://www.disctronics.co.uk/technology/video/video_mpeg.htm)
      ptP_e       = 1,    // P (Predicted) video frame
      ptB_e       = 2,    // B (Bidirectional) video frame
      ptD_e       = 3,    // D (???) video frame 
      ptUnknown_e = 4
   };

   enum picMask_e {
      ptOnlyI_e   = 1,
      ptNoB_e     = 3,
      ptAll_e     = 15
   };
   
   static char getPicType( picType_e );

   void feed( void const   *inData, 
              unsigned long inBytes,
              long long     pts );

   bool getPicture( void const *&picture,
                    picType_e   &type,
                    unsigned    &temp_ref,
                    long long   &when,
                    unsigned     ptMask = ptAll_e );

   inline bool haveHeader( void ) const { return haveVideoHeader_ ; } 
   inline unsigned short width( void ) const { return mpegWidth_ ; }
   inline unsigned short stride( void ) const { return mpegStride_ ; }
   inline unsigned short height( void ) const { return mpegHeight_ ; }
   inline unsigned numSkipped( void ) const { return numSkipped_ ; }
   inline unsigned numParsed( void ) const { return numParsed_ ; }
   inline unsigned numDrawn( void ) const { return numDraw_ ; }
   inline timeval const &usDecodeTime( void ) const { return usDecodeTime_ ; }
   inline unsigned char frameRate( void ) const { return mpegFrameRate_ ; }

   void const *gop(void) const ;

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
   unsigned short       mpegStride_ ;
   unsigned short       mpegHeight_ ;
   unsigned char        mpegFrameType_ ; // 
   unsigned char        mpegFrameRate_ ;
   picType_e            lastPicType_ ;
   unsigned             tempRef_ ;
   timeval              usStartDecode_ ;
   timeval              usDecodeTime_ ;
   bool                 skippedP_ ;
   unsigned             numSkipped_ ;
   unsigned             numParsed_ ;
   unsigned             numDraw_ ;
   long long            msPerPic_ ;
   long long            ptsIn_ ;
   long long            ptsOut_ ;

#ifdef NEON
   //
   // Get a picture buffer. 
   //
   // height, width, and stride must have been previously established
   //
   void *getPictureBuf( void );
   
   std::list<void *>    freeBufs_ ;
   std::list<void *>    allocated_ ;
#endif 
};

#endif
