/*
 * Module serialPoll.cpp
 *
 * This module defines the methods of the serialPoll_t
 * class as declared in serialPoll.h
 *
 *
 * Change History : 
 *
 * $Log: serialPoll.cpp,v $
 * Revision 1.1  2004-03-27 20:24:22  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "serialPoll.h"
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include "baudRate.h"

class serialPollTimer_t : public pollTimer_t {
public:
   serialPollTimer_t( serialPoll_t &bcp )
      : serial_( bcp ){}

   virtual void fire( void ){ serial_.timeout(); }

private:
   serialPoll_t &serial_ ;
};

serialPoll_t :: serialPoll_t
   ( pollHandlerSet_t &set,
     char const       *devName,
     int               baud,
     int               databits,
     char              parity,
     int               outDelay,
     char              terminator,
     int               inputTimeout )
   : pollHandler_t( open( devName, O_RDWR ), set )
   , timer_( ('\0' == terminator ) 
             ? ( ( 0 != inputTimeout )
                 ? new serialPollTimer_t( *this ) 
                 : 0 )
             : 0 )
   , outDelay_( outDelay )
   , inLength_( 0 )
   , inputTimeout_( inputTimeout )
   , terminator_( terminator )
   , lines_( 0 )
   , endLine_( 0 )
{
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
      
      unsigned baudConst ;
      if( baudRateToConst( baud, baudConst ) )
      {
         cfsetispeed(&newTermState, baudConst);
         cfsetospeed(&newTermState, baudConst);
      }
      else
         fprintf( stderr, "Invalid baud %u\n", baud );

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

serialPoll_t :: ~serialPoll_t( void )
{
   if( isOpen() )
      close();
}

// override this to perform processing of a received line
void serialPoll_t :: onLineIn( void )
{
   std::string s ;
   while( readln(s) )
      printf( "%u bytes of input line\n", s.size() );
}

// override this to perform processing of any input characters
// (This routine is just a notification, use read() to extract
// the input)
void serialPoll_t :: onChar( void )
{
   printf( "raw data available:\n" );
   std::string s ;
   if( read( s ) )
      printf( "%u bytes <%s>\n", s.size(), s.c_str() );
}

bool serialPoll_t :: readln( std::string &s )
{
   if( lines_ )
   {
      line_t *const line = lines_ ;
      lines_ = line->next_ ;
      if( 0 == lines_ )
         endLine_ = 0 ;
      s.assign( line->data_, line->length_ );
      free( line );
      return true ;
   }
   else
   {
      s = "" ;
      return false ;
   }
}

//    - read (and dequeue) all available data
//    returns false when nothing is present
bool serialPoll_t :: read( std::string &s )
{
   s.assign( inData_, inLength_ );
   inLength_ = 0 ;
   return ( 0 != s.size() );
}

// general pollHandler input routine
void serialPoll_t :: onDataAvail( void )
{
   unsigned const spaceAvail = sizeof( inData_ )-inLength_-1 ;
   int numRead = ::read( fd_, inData_+inLength_, spaceAvail );
   if( 0 <= numRead )
   {
      inData_[inLength_+numRead] = '\0' ;
      if( 0 != terminator_ )
      {
         char const *start = inData_ ;
         
         for( unsigned next = 0 ; next < numRead ; next++ )
         {
            if( terminator_ == start[inLength_] )
            {
               addLine( start, inLength_ );
               start = start + inLength_ + 1 ;
               inLength_ = 0 ;
            }
            else
               inLength_++ ;
         }

         if( start != inData_ )
         {
            if( inLength_ )
               memcpy( inData_, start, inLength_ ); // input past terminator
         } // read something off of the head

         if( 0 != lines_ )
            onLineIn();
         
      } // terminated
      else if( 0 != timer_ )
      {
         inLength_ += numRead ;
         inData_[inLength_] = '\0' ;
         timer_->set( 10 );
      }
      else
      {
         inLength_ += numRead ;
         inData_[inLength_] = '\0' ;
         onChar();
      }
   }
}

void serialPoll_t :: timeout( void )
{
   if( 0 < inLength_ )
   {   
      addLine( inData_, inLength_ );
      inLength_ = 0 ;
   }
   onLineIn();
}

int serialPoll_t :: write( void const *data, int length ) const 
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

void serialPoll_t :: addLine( char const *data, unsigned len )
{
   line_t * const line = (line_t *)malloc( sizeof( line_t ) + len );
   line->next_   = 0 ;
   line->length_ = len ;
   memcpy( line->data_, data, len );
   line->data_[len] = '\0' ;

   if( 0 == lines_ )
   {
      lines_   = line ;
      endLine_ = line ;
   } // start line
   else
   {
      endLine_->next_ = line ;
      endLine_ = line ;
   } // add to end
}


#ifdef STANDALONE

int main( void )
{
   pollHandlerSet_t handlers ;
   getTimerPoll( handlers );
   serialPoll_t  bcPoll( handlers, "/dev/ttyS1", 115200, 8, 'N', 0, '\xE7', 0 );
   
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
