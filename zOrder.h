#ifndef __ZORDER_H__
#define __ZORDER_H__ "$Id: zOrder.h,v 1.1 2002-11-21 14:09:52 ericn Exp $"

/*
 * zOrder.h
 *
 * This header file declares the zOrderMap_t and
 * zOrderMapStack_t classes, which are used to keep
 * track of the objects occupying a piece of screen
 * real estate.
 *
 *
 * Change History : 
 *
 * $Log: zOrder.h,v $
 * Revision 1.1  2002-11-21 14:09:52  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#ifndef __BOX_H__
#include "box.h"
#endif

#include <list>
#include <vector>

class zOrderMap_t {
public:
   zOrderMap_t( void );    // starts off empty
   ~zOrderMap_t( void );
   
   box_t *getBox( unsigned short x, unsigned short y ) const { return getBoxById( boxes_[y*width_+x] ); }
   void   addBox( box_t const & );
   void   removeBox( box_t const & );

private:

   unsigned short const width_ ;    // from frame buffer device
   unsigned short const height_ ;   //           "
   box_t::id_t  * const boxes_ ;

   friend void dumpZMap( zOrderMap_t const &map );
};


class zOrderMapStack_t {
public:
   void addBox( box_t const & );
   void removeBox( box_t const & );

   zOrderMap_t &top( void );

   //
   // increase the depth of the stack
   //
   void push(); 

   //
   // decrease the depth of the stack
   //
   void pop();

   //
   // retrieve a set of box_t's occupying a pixel
   // set is returned in fore->back order, so getBoxes(0,0)[0] is 
   // the top-most item occupying pixel 0,0
   //
   std::vector<box_t *> getBoxes( unsigned short xPos, unsigned short yPos ) const ;

private:
   zOrderMapStack_t( void );
   ~zOrderMapStack_t( void );

   typedef std::list<zOrderMap_t *> mapStack_t ;

   mapStack_t  maps_ ;

   friend zOrderMapStack_t &getZMap();
   friend void dumpZMaps( void );
};


#endif

