#ifndef __FBCMDCLEAR_H__
#define __FBCMDCLEAR_H__ "$Id: fbCmdClear.h,v 1.2 2006-11-09 16:34:34 ericn Exp $"

/*
 * fbCmdClear.h
 *
 * This header file declares the fbCmdClear_t class, which represents a 
 * clear (set to zero) command. It is implemented through the rectangle
 * fill drawing engine command.
 *
 * Change History : 
 *
 * $Log: fbCmdClear.h,v $
 * Revision 1.2  2006-11-09 16:34:34  ericn
 * -allow re-targeting Y and destRam
 *
 * Revision 1.1  2006/10/16 22:45:32  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbCmdList.h"

class fbCmdClear_t : public fbCommand_t {
public:
   fbCmdClear_t( unsigned long    destRamOffs,
                 unsigned         destx,
                 unsigned         desty,
                 unsigned         destw,
                 unsigned         desth,
                 unsigned short   rgb16 = 0 );
   virtual ~fbCmdClear_t( void );

   virtual void const *data( void ) const ;
   virtual void retarget( void * );
   
   unsigned getDestX( void ) const ;
   void setDestX( unsigned xPos );

   //
   // Y-position is folded into ram offset (always row 0)
   //
   unsigned getDestRamOffs( void ) const ;
   void setDestRamOffs( unsigned offs );

   void moveDestY( int numRows );

private:
   unsigned long * const data_ ;
   unsigned long * cmdMem_ ; // either local or retargeted
};

#endif

