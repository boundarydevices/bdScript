#ifndef __BARCODEPOLL_H__
#define __BARCODEPOLL_H__ "$Id: barcodePoll.h,v 1.2 2003-10-31 13:31:18 ericn Exp $"

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
 * Revision 1.2  2003-10-31 13:31:18  ericn
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

   bool isOpen( void ) const { return 0 <= getFd(); }
   
   // override this to perform processing of a received barcode
   virtual void onBarcode( void );

   bool         haveBarcode( void ) const { return complete_ ; }
   bool         havePartial( void ) const { return '\0' != barcode_[0]; }
   char const  *getBarcode( void ) const ;

   virtual void onDataAvail( void );
   void         timeout( void );
   bool         haveTerminator( void ) const { return '\0' != terminator_ ; }

protected:
   bool complete_ ;
   char barcode_[256];
   char terminator_ ;
};

#endif

