#ifndef __TTYPOLL_H__
#define __TTYPOLL_H__ "$Id: ttyPoll.h,v 1.3 2004-12-28 03:46:07 ericn Exp $"

/*
 * ttyPoll.h
 *
 * This header file declares the ttyPollHandler_t,
 * which provides an editable command-line interface
 * in a pollHandler environment.
 *
 * It provides a virtual onLineIn() method for handling 
 * input. The input line is available via getLine(), and
 * the terminating character is available through getTerm().
 *
 * Change History : 
 *
 * $Log: ttyPoll.h,v $
 * Revision 1.3  2004-12-28 03:46:07  ericn
 * -never mind
 *
 * Revision 1.2  2004/12/28 03:36:11  ericn
 * -restore BLOCK state
 *
 * Revision 1.1  2003/12/28 15:25:47  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif

#include <termios.h>

class ttyPollHandler_t : public pollHandler_t {
public:
   ttyPollHandler_t( pollHandlerSet_t  &set,
                     char const        *devName = 0 ); // 0 means use stdin/stdout
   ~ttyPollHandler_t( void );

   virtual void onDataAvail( void );     // POLLIN

   virtual void onLineIn( void );
   virtual void onCtrlC( void );

   inline char const *getLine( void ) const { return dataBuf_ ; }
   inline char getTerm( void ) const { return terminator_ ; }

   bool ctrlcHit( void ) const { return ctrlcHit_ ; }
   void resetCtrlC( void ){ ctrlcHit_ = false ; }

protected:
   struct termios oldTermState_ ;
   char           dataBuf_[512];
   unsigned       numRead_ ;
   char           terminator_ ;
   bool           ctrlcHit_ ;
};

#endif

