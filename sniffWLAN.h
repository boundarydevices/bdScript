#ifndef __SNIFFWLAN_H__
#define __SNIFFWLAN_H__ "$Id: sniffWLAN.h,v 1.3 2005-11-06 00:49:53 ericn Exp $"

/*
 * sniffWLAN.h
 *
 * This header file declares the sniffWLAN_t 
 * class, which is used to scan for access points.
 *
 * The primary purpose of this class is to keep track
 * of all of the pieces of data surrounding multiple
 * the sniffing process, and hide the details of
 * packet capture.
 *
 *    - socket handlers
 *    - list of known APs
 *
 * It consists of two sets of interfaces, one for a
 * 'worker' thread, and another for an observer.
 *
 * Change History : 
 *
 * $Log: sniffWLAN.h,v $
 * Revision 1.3  2005-11-06 00:49:53  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.2  2003/08/13 00:49:04  ericn
 * -fixed cancellation process
 *
 * Revision 1.1  2003/08/12 01:20:38  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

class sniffWLAN_t {
public:
   typedef struct ap_t {
      char           ssid_[32];
      unsigned char  apMac_[6];
      unsigned char  bssid_[6];
      unsigned       channel_ ;
      unsigned       signal_ ;
      unsigned       noise_ ;
      bool           requiresWEP_ ;
      ap_t          *next_ ;
   };

   sniffWLAN_t( void );
   virtual ~sniffWLAN_t( void );

   //
   // external interfaces
   //
   inline bool sniffing( void ) const { return 0 <= fd_ ; }
   inline void cancel( void ){ cancel_ = true ; }

   ap_t *getFirstAP( void ) const { return firstAP_ ; }
   void  clearAPs( void );

   // override this to get notified
   virtual void onNew( ap_t &newAP );

   //
   // worker and internal interface
   //
   void sniff( unsigned channel, unsigned seconds );
   void close( void );

protected:
   bool volatile cancel_ ;
   ap_t         *firstAP_ ;
   int  volatile fd_ ;
};

#endif

