/*
 * Module dnsPoll.cpp
 *
 * This module defines the methods of the dnsPoll_t class
 * as declared in dnsPoll.h
 *
 *
 * Change History : 
 *
 * $Log: dnsPoll.cpp,v $
 * Revision 1.1  2004-12-13 05:14:12  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "dnsPoll.h"
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
// #define DEBUGPRINT
#include "debugPrint.h"
#include "hexDump.h"

typedef struct dnsMsg_t {
   unsigned short id_ ;
   unsigned short flags_ ;
   unsigned short numQ_ ;
   unsigned short numAnsRR_ ;
   unsigned short numAuthRR_ ;
   unsigned short numAddRR_ ;
   char questions_[512-(6*sizeof(unsigned short))];
} __attribute__( (packed) );


enum {
   respNotQuery  = 0x8000,
   opcodeMask  = 0x7800,
   authAnswer  = 0x0400,
   truncated   = 0x0200,
   recursDes   = 0x0100,
   recursAvail = 0x0080,
   unused      = 0x0040,
   authentic   = 0x0020,
   disabled    = 0x0010,
   returnMask  = 0x000f,
   returnNoErr = 0
};

static dnsPoll_t *dns_ = 0 ;

dnsPoll_t :: dnsPoll_t
   ( pollHandlerSet_t &set,
     unsigned long     dnServer )
   : udpPoll_t( set, htons(53) )
   , dnServer_( dnServer )
   , nextId_( 0x1234 )
   , requests_()
   , entries_( 0 )
{
   INIT_LIST_HEAD(&requests_);
}

dnsPoll_t &dnsPoll_t::get( void )
{
   return *dns_ ;
}

void dnsPoll_t::dumpRequests( void )
{
   list_head *next = requests_.next ;
   
   printf( "head %p\n", &requests_ );

   while( next != &requests_ )
   {
      printf( "%p/%p\n", next->prev, next->next );
      next = next->next ;
   }
}
   
dnsPoll_t &dnsPoll_t::get( pollHandlerSet_t &set,
                           unsigned long     dnsNetOrder )
{
   if( 0 == dns_ )
      dns_ = new dnsPoll_t( set, dnsNetOrder );
}


dnsPoll_t::request_t *dnsPoll_t::find( unsigned short idx )
{
   struct list_head *next = requests_.next ;
   while( next != &requests_ )
   {
      request_t *req = (request_t *)next ;
      if( req->id_ == idx )
         return req ;
      next = next->next ;
   }

   return 0 ;
}

dnsPoll_t::entry_t *dnsPoll_t::find( char const *hostName )
{
   dnsPoll_t::entry_t *next = entries_ ;
   while( next )
   {
      if( 0 == strcasecmp( hostName, next->name_ ) )
         break ;
      next = next->next_ ;
   }

   return next ;
}


class dnsTimer_t : public pollTimer_t {
public:
   dnsTimer_t( unsigned short idx,
               unsigned long  timeoutMs )
      : pollTimer_t()
      , idx_( idx ){ set( timeoutMs ); }

   virtual void fire( void )
   {
      dnsPoll_t::get().timeout( idx_ );
   }

private:
   unsigned short idx_ ;
};

//
// parse a name
//
static bool parseName( void const  *msg,     // start of packet
                       void const  *eop,     // end of packet
                       char const *&nextIn,
                       char        *nextOut )
{
   unsigned const packetLen = (char const *)eop - (char const *)msg ;

   while( nextIn < eop )
   {
      char const len = *nextIn++ ;
      if( 0 == len )
      {
         nextOut[-1] = '\0' ;
         return true ;
      }
      else if( ( 63 >= len ) && ( nextIn + len < eop ) )
      {
         memcpy( nextOut, nextIn, len );
         nextOut += len ;
         nextIn  += len ;
         *nextOut++ = '.' ;
      }
      else if( ( nextIn < eop ) 
               &&
               ( 0xc0 == ( len & 0xc0 ) ) )
      {
         unsigned offs = len & 0x3f ;
         offs += *nextIn++ ;
         if( offs < packetLen )
         {
            char const *tmpIn = (char const *)msg + offs ;
            return parseName( msg, eop, tmpIn, nextOut );
         }
         else
            break;
      }
      else
         break ;
   }
   return false ;
}

void dnsPoll_t::onMsg
   ( void const        *msg,
     unsigned           msgLen,
     sockaddr_in const &sender )
{
   debugPrint( "received %u bytes from %s/%u\n", msgLen, inet_ntoa( sender.sin_addr ), ntohs( sender.sin_port ) );
   dnsMsg_t const *dns = (dnsMsg_t const *)msg ;
   debugPrint( "id:     %x\n"
               "flags:  %x\n"
               "numQ:   %x\n"
               "numAns: %x\n"
               "numAuth: %x\n"
               "numAddr: %x\n",
               ntohs(dns->id_), 
               ntohs(dns->flags_), 
               ntohs(dns->numQ_), 
               ntohs(dns->numAnsRR_), 
               ntohs(dns->numAuthRR_), 
               ntohs(dns->numAddRR_) );
#ifdef DEBUGPRINT
   hexDumper_t dump( msg, msgLen );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
#endif 

   request_t *req = find( dns->id_ );
   if( req )
   {
      char nameBuf[512];

      unsigned short flags = ntohs( dns->flags_ );
      unsigned long addr = 0 ;
      bool found = false ;
      if( returnNoErr == ( flags & returnMask ) )
      {
         char const *const eop = ((char const *)msg )+msgLen ;
         char const *nextIn = dns->questions_ ;
         if( parseName( msg, eop, nextIn, nameBuf ) 
             &&
             ( 0 == strcasecmp( nameBuf, req->name_ ) ) )
         {
            debugPrint( "name: %s, next %p\n", nameBuf, nextIn );
            unsigned short const shortOne = htons(1);
            if( 0 == memcmp( &shortOne, nextIn, sizeof(shortOne) ) )    // type A
            {
               nextIn += sizeof( shortOne );
               if( 0 == memcmp( &shortOne, nextIn, sizeof(shortOne) ) ) // class IN
               {
                  nextIn += sizeof( shortOne );
                  unsigned responses = ntohs(dns->numAnsRR_) + 
                                       ntohs(dns->numAuthRR_) +
                                       ntohs(dns->numAddRR_);
                  for( unsigned i = 0 ; ( nextIn < eop ) && ( i < responses ); i++ )
                  {
                     if( parseName( msg, eop, nextIn, nameBuf ) )
                     {
                        debugPrint( "name: %s, next %p\n", nameBuf, nextIn );

                        if( 10 <= (eop-nextIn ) )
                        {
                           unsigned short type ;
                           memcpy( &type, nextIn, sizeof( type ) );
                           nextIn += sizeof( type );
                           unsigned short class_ ;
                           memcpy( &class_, nextIn, sizeof( class_ ) );
                           nextIn += sizeof( class_ );
                           unsigned long timeout ;
                           memcpy( &timeout, nextIn, sizeof( timeout ) );
                           nextIn += sizeof( timeout );
                           unsigned short rlen ;
                           memcpy( &rlen, nextIn, sizeof( rlen ) );
                           rlen = ntohs( rlen );
                           nextIn += sizeof( rlen );
                           debugPrint( "type %04x, class %04x, timeout %lu, rlen %u\n", type, class_, ntohl( timeout ), rlen );
                           if( ( type == shortOne )
                               &&
                               ( class_ == shortOne ) 
                               &&
                               ( sizeof( struct in_addr ) == rlen ) )
                           {
                              struct in_addr in ;
                              memcpy( &in, nextIn, sizeof( in ) );
                              debugPrint( "--> found %s\n", inet_ntoa( in ) );
                              addr = in.s_addr ;
                              found = true ;
                              break;
                           } 
                           nextIn += rlen ;
                        }
                     }
                  }
               }
            }
         }
         else
         {
            debugPrint( "resolve error 2\n" );
         }
      }
      else
      {
         debugPrint( "resolve error %d\n", flags & returnMask );
      }

      entry_t *entry = new entry_t ;
      entry->name_ = req->name_ ;
      req->name_ = 0 ;
      entry->resolved_ = false ;
      entry->addr_     = 0 ;
      entry->next_     = entries_ ;
      entries_ = entry ;

      req->callback_( entry->name_, found, addr );

      delete req ;
   }
}

void dnsPoll_t::getHostByName
   ( char const   *hostName,
     dnsCallback_t callback,
     unsigned long msTimeout )
{
   entry_t *entry = find( hostName );
   if( 0 == entry )
   {
      in_addr_t addr = inet_addr( hostName );
      if( INADDR_NONE == addr )
      {
         dnsPoll_t::request_t *req = new dnsPoll_t::request_t( requests_, hostName, callback );
         req->id_    = nextId_++ ;
         
         dnsMsg_t msg ;
         msg.id_        = req->id_ ;
         msg.flags_     = htons( recursDes );
         msg.numQ_      = htons( 1 );
         msg.numAnsRR_  = 0 ;
         msg.numAuthRR_ = 0 ;
         msg.numAddRR_  = 0 ;
         char *nextOut = msg.questions_ ;
         char const *prev = hostName ;
         char const *next ;
         while( 0 != ( next = strchr( prev, '.' ) ) )
         {
            char const len = (char)( next-prev );
            *nextOut++ = len ;
            memcpy( nextOut, prev, len );
            nextOut += len ;
            prev = next + 1 ;
         }
         char const len = (char)strlen( prev );
         *nextOut++ = len ;
         memcpy( nextOut, prev, len );
         nextOut += len ;
         *nextOut++ = 0 ;
         unsigned const shortOne = htons(1);
         memcpy( nextOut, &shortOne, 2 );
         nextOut += 2 ;

         memcpy( nextOut, &shortOne, 2 );
         nextOut += 2 ;

         int const msgLen = nextOut - (char *)&msg ;

         sockaddr_in remote ;
         remote.sin_family       = AF_INET ;
         remote.sin_addr.s_addr  = dnServer_ ;
         remote.sin_port         = htons(53) ;

#ifdef DEBUGPRINT
hexDumper_t dump( &msg, msgLen );
while( dump.nextLine() )
   debugPrint( "%s\n", dump.getLine() );
#endif 
         int const numSent = sendto( getFd(), &msg, msgLen, 0, 
                                     (struct sockaddr *)&remote, sizeof( remote ) );
         if( numSent != msgLen )
            fprintf( stderr, "dnsSend: %m\n" );
   
         req->timer_ = new dnsTimer_t( req->id_, msTimeout );
      }
      else
         callback( hostName, true, addr );
   }
   else
      callback( hostName, entry->resolved_, entry->addr_ );
}


void dnsPoll_t::timeout( unsigned short idx )
{
   debugPrint( "dns Timeout %x\n", idx );
   struct list_head *next = requests_.next ;
   while( next != &requests_ )
   {
      request_t *req = (request_t *)next ;
      next = next->next ;

      if( req->id_ == idx )
      {
         entry_t *entry = new entry_t ;
         entry->name_ = req->name_ ;
         req->name_ = 0 ;
         entry->resolved_ = false ;
         entry->addr_     = 0 ;
         entry->next_     = entries_ ;
         entries_ = entry ;

         req->callback_( entry->name_, false, 0 );
         
         debugPrint( "dns Timeout %s\n", entry->name_ );

         delete req ;
      }
   }
}


dnsPoll_t::request_t::request_t
   ( list_head    &list,
     char const   *name,
     dnsCallback_t callback )
{
   list_add( &list_, &list );
   name_ = strdup( name );
   callback_ = callback ;
   
   //
   // filled in by parent
   //
   id_ = 0 ; 
   timer_ = 0 ;
}

dnsPoll_t::request_t :: ~request_t( void )
{
   if( name_ )
      free( (void *)name_ );
   if( timer_ )
      delete timer_ ;
   list_del( &list_ );
}


#ifdef __MODULETEST__
#include <stdio.h>


static void dnsCallback( char const *hostName, bool resolved, unsigned long addr )
{
   struct in_addr in ;
   in.s_addr = addr ;
   printf( "%s:%s:%s\n", resolved ? "resolved" : "timeout", hostName, inet_ntoa( in ) );
}

int main( int argc, char const *const argv[] )
{
   printf( "Hello, dnsPoll\n" );
   if( 2 <= argc )
   {
      pollHandlerSet_t handlers ;
      getTimerPoll( handlers );
      dnsPoll_t &dns = dnsPoll_t::get( handlers, inet_addr( "192.168.0.1" ) );
   
      for( int arg = 1 ; arg < argc ; arg++ )
         dns.getHostByName( argv[arg], dnsCallback, 2000 );
   
      while( 1 )
      {
         handlers.poll( 1000 );
      }
   }

   return 0 ;
}

#endif 
