#ifndef __PARSEDFLASH_H__
#define __PARSEDFLASH_H__ "$Id: parsedFlash.h,v 1.1 2003-11-24 19:42:42 ericn Exp $"

/*
 * parsedFlash.h
 *
 * This header file declares the parsedFlash_t
 * class, which is a thin wrapper around the 
 * parsing logic for a flash file. Note that the
 * data memory must remain locked for the entire
 * lifetime of this class.
 *
 *
 * Change History : 
 *
 * $Log: parsedFlash.h,v $
 * Revision 1.1  2003-11-24 19:42:42  ericn
 * -polling touch screen
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "flash/flash.h"

class parsedFlash_t {
public:
   parsedFlash_t( void const *data, unsigned dataBytes );
   ~parsedFlash_t( void );

   bool worked( void ) const { return parsed_ ; }

   operator FlashHandle( void ) const { return hFlash_ ; }

   FlashInfo const &flashInfo( void ) const { return fi_ ; }

private:
   FlashHandle hFlash_ ;
   bool        parsed_ ;
   FlashInfo   fi_ ;
};

#endif

