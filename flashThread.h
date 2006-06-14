#ifndef __FLASHTHREAD_H__
#define __FLASHTHREAD_H__ "$Id: flashThread.h,v 1.4 2006-06-14 13:55:43 ericn Exp $"

/*
 * flashThread.h
 *
 * This header file declares the flashThread_t class,
 * which represents a flash movie object and player thread
 * with interfaces for a controlling thread to:
 *
 *       start
 *       pause
 *       cont        (continue)
 *       stop        (implies rewind and generates 'cancel' event)
 *       goto frame
 *       rewind      (goto frame 0)
 *
 * As well as a file handle for reporting events back to the
 * application:
 *
 *       complete - indicates that the flashMovie played to eof
 *       cancel   - indicates that the flashMovie was cancelled
 *
 * Note that the flash data memory passed in the constructor must
 * remain allocated for the entire scope of the flashThread_t.
 *
 * Also note that the control and feedback are communicated to the
 * thread via pipes to provide ordering and control of concurrency
 * and to allow the use of poll().
 * 
 * Change History : 
 *
 * $Log: flashThread.h,v $
 * Revision 1.4  2006-06-14 13:55:43  ericn
 * -track exec and blt time
 *
 * Revision 1.3  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.2  2003/11/22 19:51:24  ericn
 * -fixed sound support (pause,stop)
 *
 * Revision 1.1  2003/11/22 18:30:04  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include <pthread.h>
#include "flash/flash.h"
#include "flash/swf.h"
#include "flash/movie.h"

class flashThread_t {
public:
   //
   // initialization and destruction
   //
   flashThread_t( FlashHandle hFlash,
                  unsigned    x, 
                  unsigned    y,
                  unsigned    w, 
                  unsigned    h,
                  unsigned    bgColor,
                  bool        loop );
   ~flashThread_t( void );

   bool isAlive( void ) const { return (pthread_t)-1 != threadHandle_ ; }

   //
   // control interface
   //
   inline void start( void ){ sendCtrl( start_e, 0 ); }
   inline void pause( void ){ sendCtrl( pause_e, 0 ); }
   inline void cont( void ){ sendCtrl( cont_e, 0 ); }
   inline void stop( void ){ sendCtrl( stop_e, 0 ); }
   inline void rewind( void ){ sendCtrl( rewind_e, 0 ); }

   //
   // event interface
   //
   inline int eventReadFd( void ) const { return fdReadEvents_ ; }
   enum event_e {
      none_e,
      complete_e,
      cancel_e 
   };
   
   // returns immediately. return value is true if an event (complete, cancel) was read
   bool readEvent( event_e &which ) const ;

private:
   enum controlMsg_e {
      start_e,
      pause_e,
      cont_e,
      stop_e,
      rewind_e
   };

   struct controlMsg_t {
      controlMsg_e   type_ ;
      unsigned       param_ ;
   };

   void sendCtrl( controlMsg_e type, unsigned param );
   FlashHandle const    hFlash_ ;
   unsigned const       x_ ; 
   unsigned const       y_ ;
   unsigned const       w_ ;
   unsigned const       h_ ;
   unsigned const       bgColor_ ;
   bool                 loop_ ;
   FlashDisplay         display_ ;
   unsigned short      *fbMem_ ;
   unsigned             fbStride_ ;
   SoundMixer * const   mixer_ ;
   pthread_t            threadHandle_ ;
   int                  fdReadCtrl_ ;
   int                  fdWriteCtrl_ ;
   int                  fdReadEvents_ ;
   int                  fdWriteEvents_ ;
   unsigned volatile    soundsQueued_ ;
   unsigned volatile    soundsCompleted_ ;
   unsigned long        maxExecTime_ ;
   unsigned long        maxBltTime_ ;
   friend void *flashThread( void *params );
   friend int main( int argc, char const * const argv[] );
   friend void flashSoundComplete( void *param );
   friend class flashSoundMixer ;
};

#endif

