#ifndef __AUDIOQUEUE_H__
#define __AUDIOQUEUE_H__ "$Id: audioQueue.h,v 1.1 2002-11-07 02:16:31 ericn Exp $"

/*
 * audioQueue.h
 *
 * This header file declares the audioQueue_t
 * class, the global accessor, and a shutdown 
 * routine. Note that an audio output thread is
 * created upon first activation. 
 * 
 * Use the shutdown() method to 
 *
 *
 * Change History : 
 *
 * $Log: audioQueue.h,v $
 * Revision 1.1  2002-11-07 02:16:31  ericn
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
   struct item_t {
      JSObject            *obj_ ;
      unsigned char const *data_ ;
      unsigned             length_ ;
      std::string          onComplete_ ;
      std::string          onCancel_ ;
   };

   //
   // queue an item for playback
   //
   bool insert( JSObject            *mp3Obj,
                unsigned char const *data,
                unsigned             length,
                std::string const   &onComplete,
                std::string const   &onCancel );
   
   //
   // flush all outbound audio
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

   //
   // read side interfaces
   //
   
   // returns false if thread should shutdown
   bool pull( item_t & );

   friend void *audioOutputThread( void *arg );

   typedef       mtQueue_t<item_t> queue_t ;
   queue_t       queue_ ;
   void         *threadHandle_ ;
   bool volatile shutdown_ ;
};

#endif

