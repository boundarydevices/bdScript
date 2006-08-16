#ifndef __FBCMDBLT_H__
#define __FBCMDBLT_H__ "$Id: fbCmdBlt.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * fbCmdBlt.h
 *
 * This header file declares the fbBlt_t class, which
 * represents a bitblt command to the drawing engine
 *
 * Change History : 
 *
 * $Log: fbCmdBlt.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
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
   bool isSkipped( void ) const { return 0 != skip_ ; }

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


