#ifndef __FBCMDCLEAR_H__
#define __FBCMDCLEAR_H__ "$Id: fbCmdClear.h,v 1.3 2006-12-13 21:30:40 ericn Exp $"

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
 * Revision 1.3  2006-12-13 21:30:40  ericn
 * -more explicit form of constructor
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
   //
   // This form of clear uses the screen width implicitly
   //
   fbCmdClear_t( unsigned long    destRamOffs,
                 unsigned         destx,
                 unsigned         desty,
                 unsigned         destw,
                 unsigned         desth,
                 unsigned short   rgb16 = 0 );
   //
   // This form has an explicit surface or screen width
   //    (must be a multiple of 4 pixels)
   //
   fbCmdClear_t( unsigned long    destRamOffs,
                 unsigned         destx,
                 unsigned         desty,
                 unsigned         destw,        // width of rectangle
                 unsigned         desth,        // height of rectangle
                 unsigned         surfaceWidth, // width of enclosing surface
                 unsigned short   rgb16 = 0 );
   virtual ~fbCmdClear_t( void );

   virtual void const *data( void ) const ;
   virtual void retarget( void * );
   
   unsigned getDestX( void ) const ;
   void setDestX( unsigned xPos );

private:
   unsigned long * const data_ ;
   unsigned long * cmdMem_ ; // either local or retargeted
};

#endif

