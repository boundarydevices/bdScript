#ifndef __AUDIOQUEUE_H__
#define __AUDIOQUEUE_H__ "$Id: audioQueue.h,v 1.5 2003-02-01 18:15:27 ericn Exp $"

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
 * Revision 1.5  2003-02-01 18:15:27  ericn
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

#include "js/jsapi.h"

#ifndef __MTQUEUE_H__
#include "mtQueue.h"
#endif

#include <string>

class audioQueue_t {
public:
   enum itemType_e {
      mp3Play_    = 0,
      wavRecord_  = 1,
      wavPlay_    = 2
   };

   struct item_t {
      itemType_e           type_ ;                 
      JSObject            *obj_ ;
      unsigned char const *data_ ;
      unsigned             length_ ;
      jsval                onComplete_ ;
      jsval                onCancel_ ;
      bool                 isComplete_ ;
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
   friend int getDspFd( void );

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
   int           readFd_ ;
   int           writeFd_ ;
};

#endif

