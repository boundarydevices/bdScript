#ifndef __MP3QUEUE_H__
#define __MP3QUEUE_H__ "$Id: mp3Queue.h,v 1.1 2005-04-24 18:55:03 ericn Exp $"

/*
 * mp3Queue.h
 *
 * This header file declares the mp3Queue_t class,
 * which is designed for use in video playback applications.
 * It performs buffering and synchronization tasks for
 * audio buffers and makes decisions about when to decode
 * based upon the state of the output.
 *
 * Generally, the order of events is:
 *
 *    app queues data until the queue is full, then 
 *    starts playback. 
 *
 *    The mp3Queue_t object returns a timestamp (tickMs) 
 *    from the start() method that represents when the data 
 *    will be first heard based upon the latency of the audio 
 *    device (generally constant) and the state of the queue.
 *
 *    The application can use this timestamp to synchronize
 *    the video feed.
 *
 *    As the application receives more data, it should queue
 *    it using the same queue() routine. 
 *
 *    While playing video, even if it doesn't have audio buffers
 *    to queue, the application should periodically call poll()
 *    to check for write space and pre-decode data.
 *
 * Change History : 
 *
 * $Log: mp3Queue.h,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */

#ifndef __MADDECODE_H__
#include "madDecode.h"
#endif 

#include <sys/soundcard.h>
#include <stddef.h>
#include <stdlib.h>

class mp3Queue_t {
public:
   mp3Queue_t( int fdAudio );
   ~mp3Queue_t( void );

   //
   // queue interface:
   //
   // application queues data through this method.
   //
   void queueData( void const *mp3Data, 
                   unsigned    numBytes,
                   long long   whenMs );
   bool queueIsFull( void );
   
   inline unsigned long bytesQueued() const { return bytesQueued_ ; }
   inline unsigned long msQueued() const { return (bytesQueued_*1000)
                                                  /(decoder_.numChannels()
                                                    *decoder_.sampleRate()*2); }

   // returns timestamp
   long long start( void );

   inline bool started( void ) const { return started_ ; }

   // pre-decode and check the output state
   void poll( void );

   // tell when we're done w/playback
   bool done( void );

   unsigned numPages( void ) const ;

private:
   struct page_t {
      page_t       *next_ ;
      unsigned      max_ ;       // in bytes
      unsigned      numFilled_ ; // in bytes
      unsigned char data_[1];    // MP3 or PCM, depending on queue
   };
   inline page_t *allocPage( void );
   static void freePages( page_t *head );

   int const      fdAudio_ ;
   madDecoder_t   decoder_ ;
   audio_buf_info ai_ ;
   unsigned       fragSize_ ;
   unsigned       numFrags_ ;
   unsigned long  bytesQueued_ ;
   bool           started_ ;
   page_t        *pcmHead_ ;
   page_t        *pcmTail_ ;
   page_t        *free_ ;
};


#endif

