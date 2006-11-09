#ifndef __SLOTWHEEL_H__
#define __SLOTWHEEL_H__ "$Id: slotWheel.h,v 1.1 2006-11-09 17:09:01 ericn Exp $"

/*
 * slotWheel.h
 *
 * This header file declares the slotWheel_t class, which is used 
 * to represent an on-screen slot machine wheel as described
 * in the GnuPlot file slotWheel.gpl. Note that this wheel is for 
 * rigged to provide known outcomes, so it should be used for 
 * demonstration programs only.
 *
 * The configuration information regarding speed and motion are 
 * provided by the jsData_t parameter in an object of the form: 
 *
 * {    startPos: 5,
 *      maxBack: 10,
 *      backSlope: -10,
 *      maxAccel: 10,
 *      fAccel: 8,
 *      fDecel: -9.8,
 *      bounceMag: 30,
 *      bounceDist: 6,
 *      bounceSpeed: 4
 * } 
 *
 * Change History : 
 *
 * $Log: slotWheel.h,v $
 * Revision 1.1  2006-11-09 17:09:01  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbcCircular.h"
#include "jsData.h"

class slotWheel_t {
public:
   slotWheel_t( jsData_t const &motionParams,
                fbCommandList_t &cmdList,
                unsigned long    destRamOffs,    // generally either graphics or alpha RAM
                unsigned         destx,          // screen position
                unsigned         desty,          // screen position
                unsigned         destw,          // screen width
                unsigned         desth,          // screen height
                fbImage_t const &srcImg,         // source image (digit strip)
                unsigned         height );       // visible height
   ~slotWheel_t( void );

   void start( unsigned targetOffset );
   bool isRunning( void ) const { return ( idle != state_ ) && ( done != state_ ); }
   unsigned getOffset( void ) const { return targetOffs_ ; }

   void tick(unsigned long syncCount);

private:
   enum state_e {
      idle,
      backup,
      accel,
      decel,
      bounce,
      finishing,
      done
   };

   bool            worked_ ;
   fbcCircular_t  *circ_ ;
   state_e         state_ ;
   unsigned const  height_ ;
   unsigned const  width_ ;
   unsigned        position_ ;
   unsigned        targetOffs_ ;

   // motion params
   int             backSlope_ ;
   int             bounceDist_ ;
   double          bounceSpeed_ ;
   int             maxAccel_ ;
   int             maxBack_ ;
   int             startPos_ ;
   double          fAccel_ ;
   double          maxDecel_ ;
   double          fDecel_ ;

   // calculated values
   unsigned        maxAccelSquared_ ;
   unsigned        tBackEnd_ ;
   unsigned        tAccelEnd_ ;
   unsigned        tDecelEnd_ ;
   unsigned        tBounceEnd_ ;

   long            predictedDecelEnd_ ;

   // calculated, then saved at run-time
   long            decelEndPos_ ;
   long            bounceMag_ ;
   
   unsigned long   startTick_ ;
   unsigned long   decelSlop_ ;
};

#endif

