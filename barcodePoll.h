#ifndef __BARCODEPOLL_H__
#define __BARCODEPOLL_H__ "$Id: barcodePoll.h,v 1.1 2003-10-05 19:15:44 ericn Exp $"

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
 * Revision 1.1  2003-10-05 19:15:44  ericn
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
                  int               outDelay = 0 );   // inter-character delay on output
   ~barcodePoll_t( void );

   bool isOpen( void ) const { return 0 <= getFd(); }
   
   // override this to perform processing of a received barcode
   virtual void onBarcode( void );

   bool         haveBarcode( void ){ return '\0' != barcode_[0]; }
   char const  *getBarcode( void ) const ;

   virtual void onDataAvail( void );

protected:
   char barcode_[256];
};

#endif

