#ifndef __MONITORWLAN_H__
#define __MONITORWLAN_H__ "$Id: monitorWLAN.h,v 1.1 2003-08-20 02:54:02 ericn Exp $"

/*
 * monitorWLAN.h
 *
 * This header file declares the monitorWLAN_t
 * class, which is a thread (started in its' constructor)
 * class which reads WLAN events from a netlink
 * socket connection to the linux-wlan driver.
 *
 * Override the message() routine to trap these
 * events.
 *
 * Change History : 
 *
 * $Log: monitorWLAN.h,v $
 * Revision 1.1  2003-08-20 02:54:02  ericn
 * -added module jsMonWLAN
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include <pthread.h>

class monitorWLAN_t {
public:
   monitorWLAN_t( void );
   virtual ~monitorWLAN_t( void ); // thread is shut down in destructor

   enum event_t {
      notconnected_e,
      connected_e,            // note that these echo the constants in 
      disconnected_e,         // linux-wlan/src/prism2/include/prism2/hfa384x.h
      ap_change_e,
      ap_outofrange_e,
      ap_inrange_e,
      assocfail_e,
      numEvents_e
   };

   virtual void message( event_t     event,
                         char const *deviceName );

   friend void *monitorWLAN_thread( void *arg );
private:
   int           fd_ ;
   bool volatile cancel_ ;
   pthread_t     thread_ ;
};

#endif

