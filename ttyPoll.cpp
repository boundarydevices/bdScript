/*
 * Module ttyPoll.cpp
 *
 * This module defines the methods of the ttyPollHandler_t
 * class as declared in ttyPoll.h
 *
 *
 * Change History : 
 *
 * $Log: ttyPoll.cpp,v $
 * Revision 1.1  2003-12-28 15:25:49  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "ttyPoll.h"
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

ttyPollHandler_t :: ttyPollHandler_t
   ( pollHandlerSet_t &set,
     char const       *devName )
   : pollHandler_t( ( 0 == devName ) 
                       ? 0
                       : open( devName, O_RDWR ),
                    set )
   , numRead_( 0 )
   , terminator_( 0 )
   , ctrlcHit_( 0 )
{
   dataBuf_[0] = '\0' ;
   if( 0 != devName )
      fcntl( getFd(), F_SETFD, FD_CLOEXEC );
//   fcntl( getFd(), F_SETFL, O_NONBLOCK );
   tcgetattr( getFd(), &oldTermState_ );
   struct termios raw = oldTermState_ ;
   raw.c_lflag &=~ (ICANON | ECHO | ISIG );
   tcsetattr( getFd(), TCSANOW, &raw );
   setMask( POLLIN );
   set.add( *this );
}
/*
static int signalled;

static void catcher(const int sig)
{
    signalled = sig;
}

main()
{
    struct termios	cooked, raw;
    unsigned char	c;
    unsigned int	i, timeouts;

    for (i = SIGHUP; i <= SIGPOLL; i++)
	(void) signal(c, catcher);

    // Get the state of the tty 
    (void) tcgetattr(0, &cooked);
    // Make a copy we can mess with
    (void) memcpy(&raw, &cooked, sizeof(struct termios));
    // Turn off echoing, linebuffering, and special-character processing,
    // but not the SIGINT or SIGQUIT keys.
    raw.c_lflag &=~ (ICANON | ECHO);
    // Ship the raw control blts
    (void) tcsetattr(0, TCSANOW, &raw);

*/

ttyPollHandler_t :: ~ttyPollHandler_t( void )
{
   tcsetattr( getFd(), TCSANOW, &oldTermState_ );
}

void ttyPollHandler_t :: onDataAvail( void )
{
   int numAvail ;
   if( 0 == ioctl( getFd(), FIONREAD, &numAvail ) )
   {
      for( int i = 0 ; i < numAvail ; i++ )
      {
         char c ;
         if( 1 == read( getFd(), &c, 1 ) )
         {
            if( ( '\r' == c ) 
                || 
                ( '\n' == c ) )
            {
               terminator_ = c ;
               write( getFd(), "\r\n", 2 );
               onLineIn();
               numRead_ = 0 ;
               dataBuf_[numRead_] = '\0' ;
            }
            else if( '\x1b' == c )
            {
               terminator_ = c ;
               numRead_ = 0 ;
               dataBuf_[numRead_] = '\0' ;
               write( getFd(), "\r\n", 2 );
               onLineIn();
            }
            else if( '\x15' == c )     // ctrl-u
            {
               while( 0 < numRead_ )
               {
                  write( getFd(), "\b \b", 3 );
                  --numRead_ ;
               }
               dataBuf_[numRead_] = '\0' ;
            }
            else if( '\b' == c )
            {
               if( 0 < numRead_ )
               {
                  dataBuf_[--numRead_] = '\0' ;
                  write( getFd(), "\b \b", 3 );
               }
            }
            else if( '\x03' == c )
            {
               terminator_ = c ;
               ctrlcHit_ = true ;
               onCtrlC();
               numRead_ = 0 ;
               dataBuf_[numRead_] = '\0' ;
            } // Ctrl-C
            else if( numRead_ < ( sizeof( dataBuf_ ) - 1 ) )
            {
               dataBuf_[numRead_++] = c ;
               dataBuf_[numRead_] = '\0' ;
               write( getFd(), &c, 1 );
            }
         }
         else
            break;
      }
   }
   else
      perror( "FIONREAD" );
}

void ttyPollHandler_t :: onLineIn( void )
{
   printf( "linein<%s>, length %u, terminator <%02x>\n", getLine(), strlen( getLine() ), getTerm() );
}

void ttyPollHandler_t :: onCtrlC( void )
{
   write( getFd(), "<ctrl-c>\r\n", 10 );
   kill( getpid(), SIGINT);
}


#ifdef STANDALONE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main( void )
{
   pollHandlerSet_t handlers ;
   ttyPollHandler_t tty( handlers );

   while( !tty.ctrlcHit() )
   {
      handlers.poll( -1 );
   }

   printf( "completed\n" ); fflush( stdout );

   return 0 ;
}

#endif
