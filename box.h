#ifndef __BOX_H__
#define __BOX_H__ "$Id: box.h,v 1.1 2002-11-21 14:09:52 ericn Exp $"

/*
 * box.h
 *
 * This header file declares the box_t class, 
 * and the newBox() routine used to create boxen.
 *
 *
 * Change History : 
 *
 * $Log: box.h,v $
 * Revision 1.1  2002-11-21 14:09:52  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

class transparencyMask_t {
public:
   unsigned short const  width_ ;
   unsigned short const  height_ ;
   unsigned short *const data_ ;
   
   enum {
      transparent_ = 0,
      opaque_      = 0xFFFFFFFF
   };

   inline unsigned char value( unsigned short xOffs, unsigned short yOffs ) const ;
private:
};


class box_t ;

typedef void (*touchHandler_t)( box_t         &, 
                                unsigned short x, 
                                unsigned short y );

class box_t {
public:
   enum {
      maxBoxId_     = 0xfffe,
      invalidBoxId_ = 0xffff,
      maxBoxes_     = invalidBoxId_+1
   };
   enum state_e {
      notPressed_ = 0,
      pressed_    = 1
   };

   typedef unsigned short id_t ;

   id_t            id_ ;

   unsigned short  xLeft_ ;
   unsigned short  xRight_ ;
   unsigned short  yTop_ ;
   unsigned short  yBottom_ ;

   //
   // bookkeeping for these is done by the default handlers
   //
   //    start->last can be used to keep track of motion
   // 
   state_e         state_ ;
   unsigned short  startTouchX_ ;
   unsigned short  startTouchY_ ;
   unsigned short  lastTouchX_ ;
   unsigned short  lastTouchY_ ;

   //
   // handlers should know their box type, and use this to locate object-specific data
   //
   void           *objectData_ ;
   touchHandler_t  onTouch_ ;
   touchHandler_t  onTouchMove_ ;
   touchHandler_t  onRelease_ ;

};

//
// derivations should call the default handlers to update
// the state
//
extern void defaultTouch( box_t         &,
                          unsigned short x,
                          unsigned short y );
extern void defaultTouchMove( box_t         &,
                              unsigned short x,
                              unsigned short y );
extern void defaultRelease( box_t         &,
                            unsigned short x,
                            unsigned short y );

//
// This routine constructs a new box with default (empty)handlers
// 
box_t *newBox( unsigned short xLeft, 
               unsigned short xRight, 
               unsigned short yTop, 
               unsigned short yBottom,
               void          *objectData );

//
// This routine destroys a box and makes its' id available for re-use.
//
void destroyBox( box_t * );


//
// used to get a box from a zOrderMap
// returns 0 if not found
//
box_t *getBoxById( box_t::id_t id );


unsigned char transparencyMask_t :: value
   ( unsigned short xOffs, 
     unsigned short yOffs ) const 
{ 
   if( transparent_ != (unsigned long)data_ )
   {
      if( opaque_ != (unsigned long)data_ )
         return data_[yOffs*width_+xOffs];
      else
         return 0xFF ;
   }
   else
      return 0 ;
}

#endif

