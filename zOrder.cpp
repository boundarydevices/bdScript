/*
 * Module zOrder.cpp
 *
 * This module defines the methods of the zOrderMap_t
 * and zOrderMapStack_t classes as declared in zOrder.h
 *
 *
 * Change History : 
 *
 * $Log: zOrder.cpp,v $
 * Revision 1.1  2002-11-21 14:09:52  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "zOrder.h"
#include "fbDev.h"
#include <assert.h>
#include <stdio.h>

zOrderMap_t :: zOrderMap_t( void )    // starts off empty
   : width_( getFB().getWidth() ),
     height_( getFB().getHeight() ),
     boxes_( new box_t::id_t [ width_*height_ ] )
{
   memset( boxes_, -1, width_*height_*sizeof(boxes_[0]) );
}

zOrderMap_t :: ~zOrderMap_t( void )
{
   delete [] boxes_ ;
}
   
void zOrderMap_t :: addBox( box_t const &b )
{
   printf( "adding box %u, %u/%u, %u/%u\n", b.id_, b.xLeft_, b.xRight_, b.yTop_, b.yBottom_ );
   for( unsigned y = b.yTop_ ; ( y < height_ ) && ( y < b.yBottom_ ); y++ )
   {
      for( unsigned x = b.xLeft_ ; ( x < width_ ) && ( x < b.xRight_ ); x++ )
      {
         boxes_[y*width_+x] = b.id_ ;
      }
   }
}

void zOrderMap_t :: removeBox( box_t const &b )
{
   for( unsigned y = b.yTop_ ; ( y < height_ ) && ( y < b.yBottom_ ); y++ )
   {
      for( unsigned x = b.xLeft_ ; ( x < width_ ) && ( x < b.xRight_ ); x++ )
      {
         unsigned idx = y*width_+x ;
         if( boxes_[idx] == b.id_ )
            boxes_[idx] = box_t::invalidBoxId_ ;
      }
   }
}

void zOrderMapStack_t :: addBox( box_t const &b )
{
   top().addBox( b );
}

void zOrderMapStack_t :: removeBox( box_t const &b )
{
   mapStack_t::reverse_iterator it = maps_.rbegin();
   for( ; it != maps_.rend() ; it++ )
   {
      zOrderMap_t &map = *(*it);
      map.removeBox( b );
   }
}

zOrderMap_t &zOrderMapStack_t :: top( void )
{
   assert( !maps_.empty() );
   return *maps_.back();
}


//
// increase the depth of the stack
//
void zOrderMapStack_t :: push()
{
   maps_.push_back( new zOrderMap_t );
}

//
// decrease the depth of the stack
//
void zOrderMapStack_t :: pop()
{
   if( maps_.front() != maps_.back() )
   {
      zOrderMap_t *top = maps_.back();
      maps_.pop_back();
      delete top ;
   } // more than one
}

//
// retrieve a set of box_t's occupying a pixel
// set is returned in fore->back order, so getBoxes(0,0)[0] is 
// the top-most item occupying pixel 0,0
//
std::vector<box_t *> zOrderMapStack_t :: getBoxes( unsigned short xPos, unsigned short yPos ) const 
{
   std::vector<box_t *> boxes ;
   mapStack_t::const_reverse_iterator it = maps_.rbegin();
   for( ; it != maps_.rend() ; it++ )
   {
      zOrderMap_t const &map = *(*it);
      box_t *box = map.getBox( xPos, yPos );
      if( box )
         boxes.push_back( box );
   }

   return boxes ;
}

zOrderMapStack_t :: zOrderMapStack_t( void )
   : maps_()
{
   push(); // always need one
}

zOrderMapStack_t :: ~zOrderMapStack_t( void )
{
   while( !maps_.empty() )
   {
      delete maps_.back();
      maps_.pop_back();
   }
}

static zOrderMapStack_t *mapStack_ = 0 ;

zOrderMapStack_t &getZMap()
{
   if( 0 == mapStack_ )
      mapStack_ = new zOrderMapStack_t ;
   return *mapStack_ ;
}


void dumpZMap( zOrderMap_t const &map )
{
   printf( "---> map stack\n" );
   for( unsigned y = 0 ; y < map.height_ ; y++ )
   {
      for( unsigned x = 0 ; x < map.width_ ; x++ )
      {
         box_t::id_t const id = map.boxes_[y*map.width_+x];
         if( box_t::invalidBoxId_ != id )
            printf( "%u,%u,%u\n", x, y, id );
      }
   }
}

void dumpZMaps( void )
{
   zOrderMapStack_t &stack = getZMap();
   printf( "------> touch map stack : %u maps\n", stack.maps_.size() );
   
   zOrderMapStack_t::mapStack_t::const_iterator it = stack.maps_.begin();
   for( ; it != stack.maps_.end() ; it++ )
   {
      dumpZMap( *(*it) );
   }
}
 
