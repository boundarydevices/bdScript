/*
 * Module barcodePoll.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: barcodePoll.cpp,v $
 * Revision 1.1  2003-10-05 19:15:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "barcodePoll.h"
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>

barcodePoll_t :: barcodePoll_t
   ( pollHandlerSet_t &set,
     char const      *devName,          // normally /dev/ttyS2
     int              baud,
     int              databits,
     char             parity,
     int              outDelay )
   : pollHandler_t( open( devName, O_RDWR ), set )
{
   barcode_[0] = '\0' ;

   if( isOpen() )
   {
      fcntl( fd_, F_SETFD, FD_CLOEXEC );
      fcntl( fd_, F_SETFL, O_NONBLOCK );

      struct termios oldTermState;
      tcgetattr(fd_,&oldTermState);
   
      /* set raw mode for keyboard input */
      struct termios newTermState = oldTermState;
      newTermState.c_cc[VMIN] = 1;
   
      //
      // Note that this doesn't appear to work!
      // Reads always seem to be terminated at 16 chars!
      //
      newTermState.c_cc[VTIME] = 0; // 1/10th's of a second, see http://www.opengroup.org/onlinepubs/007908799/xbd/termios.html
   
      newTermState.c_cflag &= ~(CSTOPB|CRTSCTS);           // Mask character size to 8 bits, no parity, Disable hardware flow control
      newTermState.c_cflag |= (CLOCAL | CREAD);            // Select 8 data bits
      newTermState.c_lflag &= ~(ICANON | ECHO | ISIG);     // set raw mode for input
      newTermState.c_iflag &= ~(IXON | IXOFF | IXANY|INLCR|ICRNL|IUCLC);   //no software flow control
      newTermState.c_oflag &= ~OPOST;                      //raw output
      
      int result = tcsetattr( fd_, TCSANOW, &newTermState );
      if( 0 == result )
      {
         setMask( POLLIN );
         set.add( *this );
      }
      else
         perror( "TCSANOW" );
   }
}
   
barcodePoll_t :: ~barcodePoll_t( void )
{
   if( isOpen() )
   {
      close( fd_ );
      fd_ = -1 ;
   }
}

char const *barcodePoll_t :: getBarcode( void ) const 
{
   return barcode_ ;
}

void barcodePoll_t :: onBarcode( void )
{
   printf( "barcode <%s>\n", barcode_ );
}
   
void barcodePoll_t :: onDataAvail( void )
{
   unsigned curLen = strlen( barcode_ );
   int numRead = read( fd_, barcode_+curLen, sizeof( barcode_ )-curLen-1 );
   if( 0 <= numRead )
   {
      curLen += numRead ;
      barcode_[curLen] = '\0' ;
      onBarcode();
      barcode_[0] = '\0' ;
   }
}



#ifdef STANDALONE

int main( void )
{
   pollHandlerSet_t handlers ;
   barcodePoll_t    bcPoll( handlers, "/dev/ttyS2" );
   if( bcPoll.isOpen() )
   {
      printf( "opened bcPoll: fd %d, mask %x\n", bcPoll.getFd(), bcPoll.getMask() );

      int iterations = 0 ;
      while( 1 )
      {
         handlers.poll( -1 );
         printf( "poll %d\n", ++iterations );
      }
   }
   else
      perror( "/dev/ttyS2" );

   return 0 ;
}

#endif
