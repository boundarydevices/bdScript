#ifndef __BARCODEPOLL_H__
#define __BARCODEPOLL_H__ "$Id: barcodePoll.h,v 1.5 2004-01-01 20:11:42 ericn Exp $"

/*
 * barcodePoll.h
 *
 * This header file declares the barcodePoll_t class,
 * which is a pollHandler version of the barcode reader.
 * 
 *
 *
 * Change History : 
 *
 * $Log: barcodePoll.h,v $
 * Revision 1.5  2004-01-01 20:11:42  ericn
 * -added isOpen() routine, and switched pollHandlers to use close()
 *
 * Revision 1.4  2003/12/28 00:35:57  ericn
 * -removed secondary thread
 *
 * Revision 1.3  2003/12/27 22:58:49  ericn
 * -added terminator, timeout support
 *
 * Revision 1.2  2003/10/31 13:31:18  ericn
 * -added terminator and support for partial reads
 *
 * Revision 1.1  2003/10/05 19:15:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "pollHandler.h"
#include "pollTimer.h"

class bcPollTimer_t ;

class barcodePoll_t : public pollHandler_t {
public:
   barcodePoll_t( pollHandlerSet_t &set,
                  char const       *devName = "/dev/ttyS2",
                  int               baud = 9600,
                  int               databits = 8,
                  char              parity = 'N',
                  int               outDelay = 0,           // inter-character delay on output
                  char              terminator = '\0' );    // end-of-barcode char
   ~barcodePoll_t( void );

   // override this to perform processing of a received barcode
   virtual void onBarcode( void );

   bool         haveBarcode( void ) const { return complete_ ; }
   bool         havePartial( void ) const { return '\0' != barcode_[0]; }
   char const  *getBarcode( void ) const ;

   virtual void onDataAvail( void );
   void         timeout( void );
   bool         haveTerminator( void ) const { return '\0' != terminator_ ; }
   int          write( void const *data, int length ) const ;
   int          outputDelay( void ) const { return outDelay_ ; }
   void         setOutputDelay( int value ){ outDelay_ = value ; }
protected:
   bcPollTimer_t *timer_ ;
   int            outDelay_ ;
   bool           complete_ ;
   char           barcode_[256];
   char           terminator_ ;
};

#endif

