#ifndef __AUDIOOUTPOLL_H__
#define __AUDIOOUTPOLL_H__ "$Id: audioOutPoll.h,v 1.1 2006-05-14 14:31:33 ericn Exp $"

/*
 * audioOutPoll.h
 *
 * This header file declares the audioOutPoll_t
 * class, which is used to output either MP3 or 
 * raw audio.
 *
 *
 * Change History : 
 *
 * $Log: audioOutPoll.h,v $
 * Revision 1.1  2006-05-14 14:31:33  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif

#ifndef __AUDIOQUEUE_H__
#include "audioQueue.h"
#endif

class audioOutPoll_t : public pollHandler_t {
public:
   static audioOutPoll_t &get( pollHandlerSet_t & ); // get singleton (and initialize)
   static audioOutPoll_t *get( void ); // get singleton (return 0 if not initialized)

   void queuePlayback( audioQueue_t::waveHeader_t &wave );

   virtual void onWriteSpace( void );    // POLLOUT

private:
   audioOutPoll_t( audioOutPoll_t const & ); // no copies
   audioOutPoll_t( pollHandlerSet_t &set,
                   char const       *devName = "/dev/dsp" );
   ~audioOutPoll_t( void );
};

#endif

