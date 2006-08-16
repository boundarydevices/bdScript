#ifndef __ODOMETER_H__
#define __ODOMETER_H__ "$Id: odometer.h,v 1.2 2006-08-16 02:31:56 ericn Exp $"

/*
 * odometer.h
 *
 * This header file declares the odometer_t class,
 * which is used to represent a single on-screen
 * odometer, and the odometerSet_t class, which is 
 * used to represent the complete set of odometers.
 *
 * The singleton of the odometerSet_t class takes care
 * of signal handling and sequencing for all of the odometers.
 *
 * Change History : 
 *
 * $Log: odometer.h,v $
 * Revision 1.2  2006-08-16 02:31:56  ericn
 * -rewrite based on command lists
 *
 * Revision 1.1  2006/06/06 03:04:30  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "odomGraphics.h"
#include "odomValue.h"
#include "odomMode.h"

class odometer_t {
public:
   odometer_t( odomGraphics_t const &graphics,
               unsigned              initValue,
               unsigned              x,
               unsigned              y,
               unsigned              maxVelocity_,
               odometerMode_e        mode );
   ~odometer_t( void );

   void setValue( unsigned newValue );
   void setTarget( unsigned newValue );

   void advance( unsigned numTicks );

   void hide( void );
   void show( void );

   odomValue_t  &value(void){ return value_ ; }
   unsigned      target(void){ return target_ ; }
   unsigned      velocity( void ){ return velocity_ ; }

   unsigned long cmdListOffs(void) const { return cmdListMem_.getOffs(); }

private:
   odometer_t( odometer_t const & );

   fbCommandList_t cmdList_ ;
   fbPtr_t         cmdListMem_ ;
   unsigned long   target_ ;
   odomValue_t     value_ ;
   unsigned        velocity_ ;
   unsigned const  maxVelocity_ ;
};


class odometerSet_t {
public:
   static odometerSet_t &get();

   inline bool isOpen( void ) const { return (0<=fdCmd_) && (0<=fdSync_); }

   void add( char const *name, odometer_t * );
   odometer_t *get( char const *name );

   void setValue( char const *name, unsigned );
   void setTarget( char const *name, unsigned );

   void stop( void );
   void run( void );
   bool isRunning( void ) const { return isRunning_ ; }

   unsigned long bltCount( void ) const { return issueCount_ ; }
   unsigned long syncCount( void ) const ;

   // implementation details
   void sigio(void);
   void sigCmdList(void);
   void sigVsync(void);

   typedef void (*vsyncHandler_t)( void * );

   void setHandler( vsyncHandler_t, void *opaque );

   void dump( void );

private:
   odometerSet_t( void ); // only accessible through singleton
   int const               cmdListSignal_ ;
   int const               vsyncSignal_ ;
   int const               pid_ ;
   int const               fdCmd_ ; 
   int const               fdSync_ ;
   unsigned long          *cmdList_ ;
   unsigned long           cmdListBytes_ ;
   bool                    stopping_ ;
   unsigned long           issueCount_ ;
   unsigned long volatile  completionCount_ ;
   unsigned long           prevSync_ ;
   bool                    isRunning_ ;
   vsyncHandler_t          handler_ ; 
   void                    *handlerParam_ ;
};


#endif

