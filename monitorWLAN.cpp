/*
 * Module monitorWLAN.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: monitorWLAN.cpp,v $
 * Revision 1.1  2003-08-20 02:54:02  ericn
 * -added module jsMonWLAN
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "monitorWLAN.h"
#include <unistd.h>
#include <sys/socket.h>

#include <asm/types.h>
#include <linux/netlink.h>

#include <wlan/wlan_compat.h>
#include <wlan/version.h>
#include <wlan/p80211msg.h>
#include <wlan/p80211ioctl.h>
#include <string.h>
#include <stdio.h>

void *monitorWLAN_thread( void *arg )
{
   monitorWLAN_t &wlan = *( monitorWLAN_t * )arg ;
   do {
      p80211msg_t msg ;
      int recvlen = recv( wlan.fd_, (char *)&msg, sizeof(msg), 0 );
      if( sizeof( msg ) == recvlen )
      {
         wlan.message( (monitorWLAN_t::event_t)msg.msgcode, (char const*)msg.devname );
      }
      else
         break;

   } while( !wlan.cancel_ );

printf( "monitor thread aborting\n" );

   return 0 ;
}

monitorWLAN_t :: monitorWLAN_t( void )
   : fd_( socket( PF_NETLINK, SOCK_RAW, P80211_NL_SOCK_IND ) ),
     cancel_( 0 > fd_ ),
     thread_( (pthread_t)0 )
{
   if( !cancel_ )
   {
      struct sockaddr_nl	nlskaddr;
      memset ( &nlskaddr, 0 , sizeof( nlskaddr ));
      nlskaddr.nl_family = (sa_family_t)PF_NETLINK;
      nlskaddr.nl_pid = (__u32)getpid();
      nlskaddr.nl_groups = P80211_NL_MCAST_GRP_MLME;
      /* bind the netlink socket */
      if( 0 <= bind( fd_, (struct sockaddr*)&nlskaddr, sizeof(nlskaddr))) 
      {
         if( 0 == pthread_create( &thread_, 0, monitorWLAN_thread, this ) )
         {
            return ;
         }
         else
         {
            perror( "wlanThread" );
            thread_ = (pthread_t)0 ; // just in case
         }
      } 
      else
         perror( "wlanBind" );
      
      cancel_ = true ;
      close( fd_ );
      fd_ = -1 ;
   }
}

monitorWLAN_t :: ~monitorWLAN_t( void ) // thread is shut down in destructor
{
   if( 0 <= fd_ )
   {
      cancel_ = true ;
      int const tempFD = fd_ ;
      fd_ = -1 ;
      close( tempFD );
      if( (pthread_t)0 != thread_ )
      {
         pthread_cancel( thread_ );
         void *exitStat ;
         pthread_join( thread_, &exitStat );
         thread_ = (pthread_t)0 ;
      }
   }
}

void monitorWLAN_t :: message( event_t event, char const *device )
{
}

#ifdef MODULETEST
static char const * const eventNames[] = {
   "notconnected",
   "connected",            // note that these echo the constants in 
   "disconnected",         // linux-wlan/src/prism2/include/prism2/hfa384x.h
   "ap_change",
   "ap_outofrange",
   "ap_inrange",
   "assocfail"
};

class myMonitor_t : public monitorWLAN_t {
public:
   virtual void message( event_t     event,
                         char const *deviceName )
   {
      printf( "%s - %s\n", deviceName, eventNames[event] );
   }
};

int main( void )
{
   myMonitor_t monitor ;
   
   char inBuf[80];
   fgets( inBuf, sizeof( inBuf ), stdin );

   return 0 ;
}
#endif
