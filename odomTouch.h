#ifndef __ODOMTOUCH_H__
#define __ODOMTOUCH_H__ "$Id: odomTouch.h,v 1.1 2006-08-28 18:24:43 ericn Exp $"

/*
 * odomTouch.h
 *
 * This header file declares the odometerTouch_t class,
 * which at the moment is a placeholder for application-
 * defined functionality.
 *
 * Change History : 
 *
 * $Log: odomTouch.h,v $
 * Revision 1.1  2006-08-28 18:24:43  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class odomTouch_t {
public:
   static odomTouch_t &get(void); // get singleton

   void onTouch( unsigned x, unsigned y );
   void onRelease( void );
private:
   odomTouch_t( void );
   ~odomTouch_t( void );
};

#endif

