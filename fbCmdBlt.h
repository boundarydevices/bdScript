#ifndef __FBCMDBLT_H__
#define __FBCMDBLT_H__ "$Id: fbCmdBlt.h,v 1.5 2002-12-15 05:38:51 ericn Exp $"

/*
 * fbCmdBlt.h
 *
 * This header file declares the fbBlt_t class, which
 * represents a bitblt command to the drawing engine.
 *
 * It also supports a 'skip' function for use when the 
 * screen already displays the desired information.
 * This is performed by changing the command list data
 * to a 
 *
 * It is up to the caller to ensure that the drawing 
 * engine is not processing this command when the skip(),
 * perform(), or set() routines are called.
 *
 * Change History : 
 *
 * $Log: fbCmdBlt.h,v $
 * Revision 1.5  2002-12-15 05:38:51  ericn
 * -added swapSource() method
 *
 * Revision 1.4  2006/12/13 21:31:30  ericn
 * -allow re-targeting RAM
 *
 * Revision 1.2  2006/10/16 22:37:08  ericn
 * -add membbers getDestX(), setDestX(), getWidth()
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbCmdList.h"
#include "fbImage.h"

class fbBlt_t : public fbCommand_t {
public:
   fbBlt_t( unsigned long    destRamOffs,
            unsigned         destx,
            unsigned         desty,
            unsigned         destw,
            unsigned         desth,
            fbImage_t const &srcImg,
            unsigned         srcx,
            unsigned         srcy,
            unsigned         w,
            unsigned         h );
   virtual ~fbBlt_t( void );

   void set( unsigned long    destRamOffs,
             unsigned         destx,
             unsigned         desty,
             unsigned         destw,
             unsigned         desth,
             fbImage_t const &srcImg,
             unsigned         srcx,
             unsigned         srcy,
             unsigned         w,
             unsigned         h );

   virtual void const *data( void ) const ;
   virtual void retarget( void * );

   void skip( void );
   void perform( void );
   bool isSkipped( void ) const { return skip_ ; }

   unsigned getDestX( void ) const ;
   void setDestX( unsigned destx );

   void moveDestY( int numRows );

   unsigned getWidth( void ) const ;

   void swapSource( fbImage_t const &src, unsigned srcy );

private:
   unsigned long * const data_ ;
   unsigned long * cmdMem_ ;
   unsigned long   skipBuf_[2];
   bool            skip_ ;
};

//
// Procedural interface: blt one image
//    returns zero if successful
int fbBlt( unsigned long    destRamOffs,
           unsigned         destx,
           unsigned         desty,
           unsigned         destw,
           unsigned         desth,
           fbImage_t const &srcImg,
           unsigned         srcx,
           unsigned         srcy,
           unsigned         w,
           unsigned         h );

#endif


