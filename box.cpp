/*
 * Module box.cpp
 *
 * This header file declares the box_t and transparency_t classes, 
 * and the newBox() routine as declared in box.h
 *
 * Change History : 
 *
 * $Log: box.cpp,v $
 * Revision 1.2  2002-12-26 19:26:59  ericn
 * -added onMoveOff support
 *
 * Revision 1.1  2002/11/21 14:09:52  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "box.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

void defaultTouch( box_t         &box,
                   unsigned short x,
                   unsigned short y )
{
   box.state_ = box.pressed_ ;
   box.startTouchX_ = box.lastTouchX_ = x ;
   box.startTouchY_ = box.lastTouchY_ = y ;
}

void defaultTouchMove( box_t         &box,
                       unsigned short x,
                       unsigned short y )
{
   box.lastTouchX_ = x ;
   box.lastTouchY_ = y ;
}

void defaultTouchMoveOff( box_t         &box,
                          unsigned short x,
                          unsigned short y )
{
   box.state_ = box.notPressed_ ;
}

void defaultRelease( box_t         &box,
                     unsigned short x,
                     unsigned short y )
{
   box.state_ = box.notPressed_ ;
}

static box_t::id_t  nextBoxId_ = box_t::invalidBoxId_ ;
static box_t      **boxesById_ = 0 ;

//
// This routine constructs a new box with default (empty)handlers
// 
box_t *newBox( unsigned short xLeft, 
               unsigned short xRight, 
               unsigned short yTop, 
               unsigned short yBottom,
               void          *objectData )
{
   if( 0 == boxesById_ )
   {
      boxesById_ = new box_t *[ box_t::maxBoxes_ ];
      memset( boxesById_, 0, sizeof( boxesById_[0] ) * box_t::maxBoxes_ );
   }

   for( box_t::id_t i = nextBoxId_ + 1 ; i != nextBoxId_ ; i++ )
   {
      if( box_t::invalidBoxId_ != i )
      {
         if( 0 == boxesById_[i] )
         {
            box_t * const newOne = new box_t ;
            newOne->id_             = i ;
            newOne->xLeft_          = xLeft ;
            newOne->xRight_         = xRight ;
            newOne->yTop_           = yTop ;
            newOne->yBottom_        = yBottom ;
            newOne->state_          = box_t::notPressed_ ;
            newOne->startTouchX_    = 0 ;
            newOne->startTouchY_    = 0 ;
            newOne->lastTouchX_     = 0 ;
            newOne->lastTouchY_     = 0 ;
            newOne->objectData_     = objectData ;
            newOne->onTouch_        = defaultTouch ;
            newOne->onTouchMove_    = defaultTouchMove ;
            newOne->onTouchMoveOff_ = defaultTouchMoveOff ;
            newOne->onRelease_      = defaultRelease ;

            boxesById_[i] = newOne ;
            nextBoxId_ = i ;
   
            return newOne ;
         }
      } // skip invalid box id
   } // find an unused box id

   return 0 ; // out of boxes!
}

//
// This routine destroys a box and makes its' id available for re-use.
//
void destroyBox( box_t *b )
{
   printf( "boxesById_ = %p\n", boxesById_ );
   assert( 0 != boxesById_ );
   printf( "destroying box id %u\n", b->id_ );
   assert( b == boxesById_[b->id_] );
   boxesById_[b->id_] = 0 ;
   delete b ;
}

box_t *getBoxById( box_t::id_t id )
{
   if( boxesById_ && ( box_t::invalidBoxId_ != id ) )
   {
      return boxesById_[id];
   }
   else
      return 0 ;
}

