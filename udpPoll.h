#ifndef __UDPPOLL_H__
#define __UDPPOLL_H__ "$Id: udpPoll.h,v 1.1 2003-12-28 20:56:05 ericn Exp $"

/*
 * udpPoll.h
 *
 * This header file declares the udpPoll_t class, 
 * which is used to signal the arrival of data in 
 * a pollHandler environment
 *
 *
 * Change History : 
 *
 * $Log: udpPoll.h,v $
 * Revision 1.1  2003-12-28 20:56:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif

#include <netinet/in.h>

class udpPoll_t : public pollHandler_t {
public:
   udpPoll_t( pollHandlerSet_t &set,
              unsigned short    port = 0 ); // network byte-order
   ~udpPoll_t( void );

   //
   // override this for socket-specific handling
   //
   virtual void onMsg( void const        *msg,
                       unsigned           msgLen,
                       sockaddr_in const &sender );

   virtual void onDataAvail( void );

protected:
   unsigned short const port_ ;
};



#endif

