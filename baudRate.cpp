/*
 * Module baudRate.cpp
 *
 * This module defines the baudRateToConst() and constToBaudRate()
 * routines as declared in baudRate.h
 *
 *
 * Change History : 
 *
 * $Log: baudRate.cpp,v $
 * Revision 1.1  2004-03-27 20:24:22  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "baudRate.h"
#include <termios.h>

static unsigned const _standardBauds[] = {
   0,
   50,
   75,
   110,
   134,
   150,
   200,
   300,
   600,
   1200,
   1800,
   2400,
   4800,
   9600,
   19200,
   38400
};
static unsigned const numStandardBauds = sizeof( _standardBauds )/sizeof( _standardBauds[0] );

static unsigned const _highSpeedMask = 0010000 ;
static unsigned const _highSpeedBauds[] = {
   0,
   57600,  
   115200, 
   230400, 
   460800, 
   500000, 
   576000, 
   921600, 
   1000000,
   1152000,
   1500000,
   2000000,
   2500000,
   3000000,
   3500000,
   4000000 
};

static unsigned const numHighSpeedBauds = sizeof( _highSpeedBauds )/sizeof( _highSpeedBauds[0] );

bool baudRateToConst( unsigned bps, unsigned &constant )
{
   unsigned baudIdx ;
   bool haveBaud = false ;
   
   unsigned i ;
   for( i = 0 ; i < numStandardBauds ; i++ )
   {
      if( _standardBauds[i] == bps )
      {
         haveBaud = true ;
         baudIdx = i ;
         break;
      }
   }
   
   if( !haveBaud )
   {
      for( i = 0 ; i < numHighSpeedBauds ; i++ )
      {
         if( _highSpeedBauds[i] == bps )
         {
            haveBaud = true ;
            baudIdx = i | _highSpeedMask ;
            break;
         }
      }
   }

   constant = baudIdx ;
   
   return haveBaud ;
}


bool constToBaudRate( unsigned constant, unsigned &bps )
{
   bps = 0 ;
   
   bool haveBaud = false ;

   if( constant < numStandardBauds )
   {
      bps = _standardBauds[constant];
   }
   else if( 0 != ( constant & _highSpeedMask ) )
   {
      constant &= ~_highSpeedMask ;
      if( constant < numHighSpeedBauds )
      {
         bps = _highSpeedBauds[constant];
      }
   }

   return haveBaud ;
}

