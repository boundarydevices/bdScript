#ifndef __DNSPOLL_H__
#define __DNSPOLL_H__ "$Id: dnsPoll.h,v 1.2 2004-12-18 18:28:43 ericn Exp $"

/*
 * dnsPoll.h
 *
 * This header file declares the dnsPoll_t class, which 
 * is used to 
 *
 *
 * Change History : 
 *
 * $Log: dnsPoll.h,v $
 * Revision 1.2  2004-12-18 18:28:43  ericn
 * -include opaque ptr in callbacks
 *
 * Revision 1.1  2004/12/13 05:14:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#ifndef __UDPPOLL_H__
#include "udpPoll.h"
#endif 

#ifndef __DLLIST_H__
#include "dlList.h"
#endif 

#ifndef __POLLTIMER_H__
#include "pollTimer.h"
#endif 

typedef void (*dnsCallback_t)( void         *opaque,
                               char const   *hostName,
                               bool          worked, 
                               unsigned long address );

class dnsPoll_t : public udpPoll_t {
public:
   static dnsPoll_t &get( void ); // singleton getter
   static dnsPoll_t &get( pollHandlerSet_t &set );

   //
   // Note that callback will be called immediately if the entry is cached
   // Also note that the host name passed to the callback may be different
   // from the name passed in (usually through the addition of a host name).
   //
   void getHostByName( char const   *hostName,
                       dnsCallback_t callback,
                       void         *opaque,
                       unsigned long msTimeout );

   virtual void onMsg( void const        *msg,
                       unsigned           msgLen,
                       sockaddr_in const &sender );
private:
   dnsPoll_t( pollHandlerSet_t &set );
   friend class dnsTimer_t ;
   
   struct request_t {
      request_t( list_head    &list,
                 char const   *name,
                 dnsCallback_t callback,
                 void         *cbParam );

      ~request_t( void );

      list_head       list_ ;
      char const     *name_ ;
      unsigned short  id_ ;
      pollTimer_t    *timer_ ;
      dnsCallback_t   callback_ ;
      void           *cbParam_ ;
   };

   struct entry_t {
      char const    *name_ ;
      bool           resolved_ ;
      unsigned long  addr_ ;
      entry_t       *next_ ;
   };

   struct server_t {
      unsigned long ipNetOrder_ ;
      server_t     *next_ ;
   };

   entry_t *find( char const *hostName );
   request_t *find( unsigned short idx );
   void     timeout( unsigned short idx );
   void     dumpRequests( void );

   unsigned short    nextId_ ;
   server_t         *servers_ ;
   list_head         requests_ ;
   entry_t          *entries_ ;
};

void dnsInit( pollHandlerSet_t &set );

#endif

