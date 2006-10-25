/*
 * Module setSerial.cpp
 *
 * This module defines the serial port utility routines
 * declared in setSerial.h
 *
 *
 * Change History : 
 *
 * $Log: setSerial.cpp,v $
 * Revision 1.3  2006-10-25 23:26:13  ericn
 * -actuall set the baud rate
 *
 * Revision 1.2  2006/10/10 20:55:12  ericn
 * -use cfmakeraw
 *
 * Revision 1.1  2006/09/27 01:41:46  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "setSerial.h"
#include "baudRate.h"
#include <stdio.h>
#include <termios.h>

int setBaud( int fd, unsigned baud )
{
   struct termios oldState;
   int rval = tcgetattr(fd,&oldState);
   if( 0 == rval ){
      struct termios newState = oldState;
   
      unsigned baudConst ;
      baudRateToConst( baud, baudConst );
      rval = cfsetispeed(&newState, baudConst);
      if( 0 == rval ){
         rval = cfsetospeed(&newState, baudConst);
         if( 0 == rval ){
            rval = tcsetattr( fd, TCSANOW, &newState );
            if( 0 != rval )
               perror( "tcsetattr" );
         }
         else
            perror( "cfsetospeed" );
      }
      else
         perror( "cfsetispeed" );
   }
   else
      perror( "tcgetattr" );

   return rval ;
}

int setParity( int fd, char parity ) // N,E,O
{
   struct termios oldState;
   int rval = tcgetattr(fd,&oldState);
   if( 0 == rval ){
      struct termios newState = oldState;
   
      newState.c_cflag &= ~PARENB ;

      if( 'E' == parity )
      {
         newState.c_cflag |= PARENB ;
         newState.c_cflag &= ~PARODD ;
      }
      else if( 'O' == parity )
      {
         newState.c_cflag |= PARENB | PARODD ;
      }
      else {
      } // no parity... already set

      rval = tcsetattr( fd, TCSANOW, &newState );
      if( 0 != rval )
         perror( "tcsetattr" );
   }
   else
      perror( "tcgetattr" );

   return rval ;
}

int setDataBits( int fd, unsigned bits ) // 7,8
{
   struct termios oldState;
   int rval = tcgetattr(fd,&oldState);
   if( 0 == rval ){
      struct termios newState = oldState;
   
      if( 7 == bits )
         newState.c_cflag &= ~CS8 ;
      else
         newState.c_cflag |= CS8 ;

      rval = tcsetattr( fd, TCSANOW, &newState );
      if( 0 != rval )
         perror( "tcsetattr" );
   }
   else
      perror( "tcgetattr" );

   return rval ;
}

int setStopBits( int fd, unsigned bits ) // 1,2
{
   struct termios oldState;
   int rval = tcgetattr(fd,&oldState);
   if( 0 == rval ){
      struct termios newState = oldState;
   
      if( 2 == bits )
         newState.c_cflag |= CSTOPB ;
      else
         newState.c_cflag &= ~CSTOPB ;

      rval = tcsetattr( fd, TCSANOW, &newState );
      if( 0 != rval )
         perror( "tcsetattr" );
   }
   else
      perror( "tcgetattr" );

   return rval ;
}

int setRaw( int fd ) // no echo or character processing
{
   struct termios oldState;
   int rval = tcgetattr(fd,&oldState);
   if( 0 == rval ){
      struct termios newState = oldState;
   
      cfmakeraw( &newState );

      rval = tcsetattr( fd, TCSANOW, &newState );
      if( 0 != rval )
         perror( "tcsetattr" );
   }
   else
      perror( "tcgetattr" );

   return rval ;
}

