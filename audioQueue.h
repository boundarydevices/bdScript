#ifndef __AUDIOQUEUE_H__
#define __AUDIOQUEUE_H__ "$Id: audioQueue.h,v 1.11 2003-08-04 12:37:48 ericn Exp $"

/*
 * audioQueue.h
 *
 * This header file declares the audioQueue_t class, 
 * the global accessor, and a shutdown routine. 
 *
 * Note that an audio input/output thread is created upon 
 * first activation. 
 * 
 * Use the shutdown() method to 
 *
 *
 * Change History : 
 *
 * $Log: audioQueue.h,v $
 * Revision 1.11  2003-08-04 12:37:48  ericn
 * -added raw MP3 (for flash)
 *
 * Revision 1.10  2003/08/02 19:30:00  ericn
 * -modified to allow clipping of video
 *
 * Revision 1.9  2003/07/30 20:26:00  ericn
 * -added MPEG support
 *
 * Revision 1.8  2003/04/24 11:16:44  tkisky
 * -include js/jsapi.h
 *
 * Revision 1.7  2003/02/08 14:56:40  ericn
 * -added mixer fd (to no avail)
 *
 * Revision 1.6  2003/02/02 13:46:17  ericn
 * -added recordBuffer support
 *
 * Revision 1.5  2003/02/01 18:15:27  ericn
 * -preliminary wave file and record support
 *
 * Revision 1.4  2002/12/01 03:13:54  ericn
 * -modified to root objects through audio queue
 *
 * Revision 1.3  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.2  2002/11/14 13:14:03  ericn
 * -modified to expose dsp file descriptor
 *
 * Revision 1.1  2002/11/07 02:16:31  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <js/jsapi.h>

#ifndef __MTQUEUE_H__
#include "mtQueue.h"
#endif

#include <string>

extern unsigned char getVolume( void );
extern void setVolume( unsigned char volume ); // range is 0-100

class audioQueue_t {
public:
   enum itemType_e {
      mp3Play_    = 0,
      mp3Raw_     = 1,
      wavRecord_  = 2,
      wavPlay_    = 3,
      mpegPlay_   = 4
   };

   struct item_t {
      itemType_e           type_ ;                 
      JSObject            *obj_ ;
      unsigned char const *data_ ;
      unsigned             length_ ;
      jsval                onComplete_ ;
      jsval                onCancel_ ;
      bool                 isComplete_ ;
      unsigned             xPos_ ;
      unsigned             yPos_ ;
      unsigned             width_ ;
      unsigned             height_ ;
      void                *callbackParam_ ;
      void               (*callback_)( void *);
   };

   //
   // wave file data is prepended with this
   //
   struct waveHeader_t {
      unsigned char  numChannels_ ;
      unsigned short sampleRate_ ;
      unsigned long  numSamples_ ;
      unsigned short samples_[1];
   };

   //
   // queue an mp3 file for playback
   //
   bool queuePlayback( JSObject            *mp3Obj,
                       unsigned char const *data,
                       unsigned             length,
                       jsval                onComplete = JSVAL_VOID,
                       jsval                onCancel = JSVAL_VOID );

   //
   // queue raw mp3 data for playback
   //
   bool queuePlayback( unsigned char const *data,
                       unsigned             length,
                       void                *cbParam,
                       void (*callback)( void *param ) );

   //
   // queue an MPEG video file for playback
   //
   bool queueMPEG( JSObject            *mpegObj,
                   unsigned char const *data,
                   unsigned             length,
                   jsval                onComplete = JSVAL_VOID,
                   jsval                onCancel = JSVAL_VOID,
                   unsigned             xPos = 0,
                   unsigned             yPos = 0,
                   unsigned             width = 0,    // 0 means use video size
                   unsigned             height = 0 ); // 0 means use video size

   //
   // queue a wave file for playback
   //
   bool queuePlayback( JSObject            *mp3Obj,
                       waveHeader_t const  &data,
                       jsval                onComplete = JSVAL_VOID,
                       jsval                onCancel = JSVAL_VOID );

   //
   // queue a record buffer
   //
   // numChannels_ will alway be 1 (only 1 microphone)
   // numSamples_ should be filled to dimension of samples[] array
   // sampleRate_ should be filled in with the desired sample rate (default 44100)
   //
   bool queueRecord( JSObject            *mp3Obj,
                     waveHeader_t        &data,
                     jsval                onComplete = JSVAL_VOID,
                     jsval                onCancel = JSVAL_VOID );

   //
   // flush all inbound/outbound audio
   //
   bool clear( unsigned &numCancelled );

   //
   //
   //
   bool stopRecording( void );

   //
   // shutdown audio output thread
   //
   static void shutdown( void );

private:
   //
   // private constructor and destructor
   //
   audioQueue_t( void );
   ~audioQueue_t( void );

   friend audioQueue_t &getAudioQueue( void );

   //
   // read side interfaces
   //
   
   // returns false if thread should shutdown
   bool pull( item_t *& );

   friend void *audioThread( void *arg );

   typedef       mtQueue_t<item_t *> queue_t ;
   queue_t       queue_ ;
   void         *threadHandle_ ;
   bool volatile shutdown_ ;
   unsigned      numReadFrags_ ;
   unsigned      readFragSize_ ;
   unsigned      maxReadBytes_ ;
};

#endif

