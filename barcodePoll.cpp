/*
 * Module barcodePoll.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: barcodePoll.cpp,v $
 * Revision 1.3  2003-12-27 22:58:51  ericn
 * -added terminator, timeout support
 *
 * Revision 1.2  2003/10/31 13:31:21  ericn
 * -added terminator and support for partial reads
 *
 * Revision 1.1  2003/10/05 19:15:44  ericn
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

class bcPollTimer_t : public pollTimer_t {
public:
   bcPollTimer_t( barcodePoll_t &bcp )
      : bcp_( bcp ){}

   virtual void fire( void ){ bcp_.timeout(); }

private:
   barcodePoll_t &bcp_ ;
};

barcodePoll_t :: barcodePoll_t
   ( pollHandlerSet_t &set,
     char const      *devName,          // normally /dev/ttyS2
     int              baud,
     int              databits,
     char             parity,
     int              outDelay,
     char             terminator )      // end-of-barcode char
   : pollHandler_t( open( devName, O_RDWR ), set ),
     timer_( ('\0' == terminator ) ? new bcPollTimer_t( *this ) : 0 ),
     outDelay_( outDelay ),
     complete_( false ),
     terminator_( terminator )
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
      if( terminator_ == barcode_[curLen-1] )
      {
         complete_ = true ;
         onBarcode();
         complete_ = false ;
         barcode_[0] = '\0' ;
      }
      else if( 0 != timer_ )
      {
         timer_->set( 10 );
      }
   }
}

void barcodePoll_t :: timeout( void )
{
   if( '\0' != barcode_[0] )
   {
      complete_ = true ;
      onBarcode();
      complete_ = false ;
      barcode_[0] = '\0' ;
   }
}

int barcodePoll_t :: write( void const *data, int length ) const 
{
   if( 0 == outDelay_ )
      return ::write( fd_, data, length );
   else {
      int numWritten = 0 ;
      while( 0 < length ) {
         if( 0 < numWritten )
            usleep( outDelay_ );
         numWritten += ::write( fd_, data, 1 );
         data = (char *)data + 1 ;
         length--;
      }
      return numWritten ;
   }
}

#ifdef STANDALONE

int main( void )
{
   pollHandlerSet_t handlers ;
   getTimerPoll( handlers );
   barcodePoll_t  bcPoll( handlers, "/dev/ttyS2" );
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
