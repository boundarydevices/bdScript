/*
 * Module ucb1x00_pins.cpp
 *
 * This module defines the ucb1x00_get_pin()
 * and ucb1x00_set_pin() routines as declared 
 * in ucb1x00_pins.h
 *
 *
 * Change History : 
 *
 * $Log: ucb1x00_pins.cpp,v $
 * Revision 1.1  2006-09-30 18:32:00  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "ucb1x00_pins.h"
#include <sys/ioctl.h>
#include "linux/ucb1x00-adc.h"
#include <unistd.h>
#include <stdio.h>

bool ucb1x00_set_pin( int      fdUCB,     // from touch-screen device, normally /dev/touchscreen/ucb1x00
                      unsigned pinNum,
                      bool     setHigh )
{
   bool oldValue ;
   if( ucb1x00_get_pin( fdUCB, pinNum, oldValue ) ){
      if( oldValue != setHigh ){
         unsigned long ioparam = pinMask( pinNum, (unsigned)setHigh );
         if( 0 == ioctl( fdUCB, SET_PIN, ioparam ) ){
            return true ;
         }
         else
            perror( "?? SET_PIN" );
      }
      else
         return true ;
   }
   return false ;
}
                      
bool ucb1x00_get_pin( int      fdUCB,
                      unsigned pinNum,
                      bool    &value )
{
   struct tsRegs tsr ;
   if( 0 == ioctl( fdUCB, GET_REGS, &tsr ) ){
      value = ( 0 != (tsr.gpioDat_ & (1<<pinNum) ) );
      return true ;
   }

   return false ;
}

//
// Return direction and data for each pin
// if bit is set in outMask, pin is an output
// levels return the state of each pin
//
bool ucb1x00_get_pins( int             fdUCB,
                       unsigned short &outMask,
                       unsigned short &levels )
{
   struct tsRegs tsr ;
   if( 0 == ioctl( fdUCB, GET_REGS, &tsr ) ){
      outMask = tsr.gpioDir_ ;
      levels  = tsr.gpioDat_ ;
      return true ;
   }

   return false ;
}

#ifdef MODULETEST
#include <fcntl.h>
#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   int fd = open( "/dev/touchscreen", O_RDWR );
   if( 0 <= fd ){
      unsigned short outMask ;
      unsigned short levels = 0 ;
      if( ucb1x00_get_pins( fd, outMask, levels ) ){
         printf( "pin:  " );
         for( unsigned i = NUMADCPINS ; i < NUMGPIOPINS ; i++ )
            printf( "%d", i );

         printf( "\n"
                 "dir:  " );
         for( unsigned i = NUMADCPINS ; i < NUMGPIOPINS ; i++ ){
            if( outMask & (1<<i) )
               printf( "o" );
            else
               printf( "i" );
         }
         printf( "\n"
                 "dat:  " );
         for( unsigned i = NUMADCPINS ; i < NUMGPIOPINS ; i++ ){
            if( levels & (1<<i) )
               printf( "1" );
            else
               printf( "0" );
         }
         printf( "\n" );

         for( int arg = 1 ; arg <= argc-2 ; arg += 2 ){
            unsigned pin = strtoul(argv[arg],0,0);
            if( ( pin >= NUMADCPINS ) &&
                ( pin < NUMGPIOPINS ) ){
               unsigned value = strtoul(argv[arg+1],0,0);
               if( 2 > value ){
                  if( ucb1x00_set_pin( fd, pin, (1==value) ) ){
                     printf( "set pin %u to %s\n", pin, (0 != value) ? "HIGH" : "low" );
                  }
                  else
                     perror( "xxxSET_PIN" );
               }
               else
                  fprintf( stderr, "Invalid state: range [0..1]\n" );
            }
            else
               fprintf( stderr, "Invalid pin %s, range [%u..%u]\n",
                        NUMADCPINS, NUMGPIOPINS-1 );
         }
      }
      else
         perror( "get_pins" );
      
      close( fd );

   }
   else
      perror( "/dev/touchscreen/ucb1x00" );

   return 0 ;
}

#endif
