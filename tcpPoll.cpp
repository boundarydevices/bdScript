/*
 * Module tcpPoll.cpp
 *
 * This module defines the methods of the tcpPoll_t class
 * as declared in tcpPoll.h
 *
 *
 * Change History : 
 *
 * $Log: tcpPoll.cpp,v $
 * Revision 1.3  2004-12-18 18:29:18  ericn
 * -added dns support
 *
 * Revision 1.2  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.1  2004/02/08 10:34:24  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "tcpPoll.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "dnsPoll.h"
#define DEBUGPRINT
#include "debugPrint.h"
#include <stdlib.h>

tcpHandler_t :: tcpHandler_t
   ( unsigned long     serverIP,          // network byte order
     unsigned short    serverPort,        // network byte order
     pollHandlerSet_t &set )
   : pollHandler_t( socket( AF_INET, SOCK_STREAM, 0 ), set )
   , hostName_( 0 )
   , serverIP_( 0 )
   , serverPort_( serverPort )
   , state_( closed )
{
   memset( callbacks_, 0, sizeof( callbacks_ ) );
   if( 0 < getFd() )
   {
      fcntl( getFd(), F_SETFD, FD_CLOEXEC );
      fcntl( getFd(), F_SETFL, O_NONBLOCK );

      state_ = nameLookup ;
      struct in_addr in ;
      in.s_addr = serverIP ;
      onNameLookup( inet_ntoa( in ), true, serverIP );
   }
}

static void nameLookupShim( void       *cbParam,
                            char const *hostName,
                            bool        resolved,
                            unsigned long address )
{
   ((tcpHandler_t *)cbParam)->onNameLookup( hostName, resolved, address );
}

tcpHandler_t :: tcpHandler_t
   ( char const       *hostName,
     unsigned short    serverPort,        // network byte order
     pollHandlerSet_t &set )
   : pollHandler_t( socket( AF_INET, SOCK_STREAM, 0 ), set )
   , serverIP_( 0 )
   , serverPort_( serverPort )
   , state_( closed )
{
   memset( callbacks_, 0, sizeof( callbacks_ ) );
   if( 0 < getFd() )
   {
      fcntl( getFd(), F_SETFD, FD_CLOEXEC );
      fcntl( getFd(), F_SETFL, O_NONBLOCK );

      state_ = nameLookup ;
      dnsPoll_t::get().getHostByName( hostName, 
                                      nameLookupShim, 
                                      this,
                                      4000 );
   }
}

void tcpHandler_t :: onNameLookup
   ( char const   *hostName, 
     bool          resolved,
     unsigned long address )
{
   if( hostName_ )
      free( (void *)hostName_ );
   if( hostName )
      hostName_ = strdup( hostName );
   else
      hostName_ = 0 ;

   if( resolved )
   {
      serverIP_ = address ;
      sockaddr_in serverAddr ;
      memset( &serverAddr, 0, sizeof( serverAddr ) );
      
      serverAddr.sin_family      = AF_INET;
      serverAddr.sin_addr.s_addr = address ;
      serverAddr.sin_port        = serverPort_ ;
   
      int const connectResult = connect( getFd(), (struct sockaddr *) &serverAddr, sizeof( serverAddr ) );
      if( ( 0 == connectResult )
          ||
          ( ( -1 == connectResult ) && ( EINPROGRESS == errno ) ) )
      {
         int doit = 1 ;
         setsockopt( getFd(), SOL_SOCKET, TCP_NODELAY, &doit, sizeof( doit ) );
         state_ = connecting ;
         setMask( POLLIN|POLLOUT|POLLERR|POLLHUP );
         parent_.add( *this );
         if( 0 == connectResult )
         {
            state_ = connected ;
         }
      }
      else
      {
         perror( "connect" );
         close();
      }
   }
   else
   {
      if( 0 != callbacks_[error_e] )
         callbacks_[dataAvail_e]( *this, error_e, cbParams_[error_e] );
   }
}

tcpHandler_t :: ~tcpHandler_t( void )
{
   if( hostName_ )
      free( (void *)hostName_ );

   parent_.removeMe( *this );
   close();
}

void tcpHandler_t :: setCallback
   ( event_e       event, 
     tcpCallback_t callback,
     void         *opaque )
{
   callbacks_[event] = callback ;
   cbParams_[event]  = opaque ;
}

tcpCallback_t tcpHandler_t :: getCallback( event_e event )
{
   return callbacks_[event];
}

tcpCallback_t tcpHandler_t :: clearCallback( event_e event )
{
   tcpCallback_t old = callbacks_[event];
   callbacks_[event] = 0 ;
   cbParams_[event]  = 0 ;
   return old ;
}

void tcpHandler_t :: onDataAvail( void )     // POLLIN
{
   checkStatus();
   if( connected == state_ )
   {
      unsigned short const prevMask = getMask();
      unsigned short mask = prevMask & ~POLLIN ;
      if( ( 0 != callbacks_[dataAvail_e] )
          &&
          callbacks_[dataAvail_e]( *this, dataAvail_e, cbParams_[dataAvail_e] ) )
      {
         mask |= POLLIN ;
      }

      if( mask != prevMask )
         setMask( mask );
   } // defer to clients
}

void tcpHandler_t :: onWriteSpace( void )    // POLLOUT
{
   checkStatus();
   if( connected == state_ )
   {
      unsigned short const prevMask = getMask();
      unsigned short mask = prevMask & ~POLLOUT ;
      if( ( 0 != callbacks_[writeSpaceAvail_e] )
          &&
          callbacks_[writeSpaceAvail_e]( *this, writeSpaceAvail_e, cbParams_[writeSpaceAvail_e] ) )
      {
         mask |= POLLOUT ;
      }

      if( mask != prevMask )
         setMask( mask );
   } // defer to clients
}

void tcpHandler_t :: onError( void )         // POLLERR
{
   checkStatus();
   if( 0 != callbacks_[error_e] )
      callbacks_[error_e]( *this, error_e, cbParams_[error_e] );
}

void tcpHandler_t :: onHUP( void )           // POLLHUP
{
   checkStatus();
   if( ( connected == state_ )
       &&
       ( 0 != callbacks_[disconnect_e] ) )
      callbacks_[disconnect_e]( *this, disconnect_e, cbParams_[disconnect_e] );
   close();
   setMask(0);
   state_ = closed ;
}

void tcpHandler_t :: checkStatus( void )
{
   int err = 0 ;
   socklen_t errSize ;
   if( ( 0 == getsockopt( getFd(), SOL_SOCKET, SO_ERROR, (void *)&err, &errSize) )
       &&
       ( ( 0 == err ) || ( EISCONN == err ) ) )
   {
      if( connecting == state_ )
      {
         if( 0 != callbacks_[connect_e] )
            callbacks_[connect_e]( *this, connect_e, cbParams_[connect_e] );
         state_ = connected ;
      }
   }
   else
   {
      printf( "closed:err 0x%x\n", err );
      close();
      state_ = closed ;
      if( 0 != callbacks_[disconnect_e] )
         callbacks_[disconnect_e]( *this, disconnect_e, cbParams_[disconnect_e] );
      setMask( 0 );
   }
}

#ifdef __MODULETEST__
#include <termios.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "ttyPoll.h"
#include "memFile.h"

bool onConnect( tcpHandler_t &handler, int event, void *opaque )
{
   printf( "Connected!\n" );
   return true ;
}

bool onDataAvail( tcpHandler_t &handler, int event, void *opaque )
{
   printf( "dataAvail\n" );
   int numAvail ;
   int const ioctlResult = ioctl( handler.getFd(), FIONREAD, &numAvail );
   if( ( 0 == ioctlResult )
       &&
       ( 0 < numAvail ) )
   {
printf( "%u bytes\n", numAvail );

      char inData[2048];
      if( sizeof( inData ) < numAvail )
         numAvail = sizeof( inData );
      int const numRead = read( handler.getFd(), inData, numAvail );
      if( 0 < numRead )
      {
         write( fileno( stdout ), inData, numRead );
         fflush( stdout );
      }
   }
   else if( 0 == ioctlResult )
   {
printf( "?? closed ??\n" );
      handler.onHUP();
   }
   else
      perror( "FIONREAD" );
   return true ;
}

bool onWriteSpace( tcpHandler_t &handler, int event, void *opaque )
{
   printf( "writeSpace\n" );
   return false ;
}

bool onError( tcpHandler_t &handler, int event, void *opaque )
{
   printf( "Error\n" );
   return true ;
}

bool onHUP( tcpHandler_t &handler, int event, void *opaque )
{
   printf( "HUP\n" );
   return true ;
}

bool onDisconnect( tcpHandler_t &handler, int event, void *opaque )
{
   struct in_addr in ;
   in.s_addr = handler.getServerIp();
   printf( "Disconnected from server %s, ip %s, port %u\n", 
           handler.hostName(), 
           inet_ntoa( in ), 
           ntohs( handler.getServerPort() ) );
}

#include <signal.h>
#include "hexDump.h"
#include <execinfo.h>
#include "ttyPoll.h"

static bool doExit_ = false ;

class myTTY_t : public ttyPollHandler_t {
public:
   myTTY_t( pollHandlerSet_t  &set,
            tcpHandler_t      &handler )
      : ttyPollHandler_t( set, 0 ), handler_( handler ){}
   virtual void onLineIn( void );
   virtual void onCtrlC( void );

   tcpHandler_t &handler_ ;
};

void myTTY_t :: onLineIn( void )
{
   printf( "ttyIn: %s\n", getLine() );
   char const *sIn = getLine();
   if( *sIn )
   {
      unsigned const len = strlen( sIn );
      printf( "--> %s..", sIn );
      int numWritten = write( handler_.getFd(), sIn, len )
                     + write( handler_.getFd(), "\r\n", 2 );
      printf( "%d bytes\n", numWritten );
   }
   else
   {
      printf( "state 0x%02x\n", handler_.getState() );
   }
}

void myTTY_t :: onCtrlC( void )
{
   printf( "<ctrl-C>\n" );
   doExit_ = true ;
}

static struct sigaction sa;
static struct sigaction oldint;

void handler(int sig) 
{
   unsigned long addr = (unsigned long)&sig ;
   unsigned long page = addr & ~0xFFF ; // 4K
   unsigned long size = page+0x1000-addr ;
   hexDumper_t dumpStack( &sig, size ); // just dump this page
   while( dumpStack.nextLine() )
      fprintf( stderr, "%s\n", dumpStack.getLine() );

   void *btArray[128];
   int const btSize = backtrace(btArray, sizeof(btArray) / sizeof(void *) );

   fprintf( stderr, "########## Backtrace ##########\n"
                    "Number of elements in backtrace: %u\n", btSize );

   if (btSize > 0)
      backtrace_symbols_fd( btArray, btSize, fileno(stdout) );

   fprintf( stderr, "Handler done.\n" );
   fflush( stderr );
   if( oldint.sa_handler )
      oldint.sa_handler( sig );

   exit( 1 );
}


int main( int argc, char const * const argv[] )
{
   // Initialize the sa structure
   sa.sa_handler = handler;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   
   // Set up the signal handler
   sigaction(SIGSEGV, &sa, NULL);
   printf( "Hello, %s\n", argv[0] );
   if( 1 < argc )
   {
      unsigned short port = htons( 80 );
      if( 2 < argc )
         port = htons( (unsigned short)strtoul( argv[2], 0, 0 ) );
      
      printf( "polling IP adress %s, port 0x%04x\n", argv[1], port );

      memFile_t fIn( "in.dat" );
      char const *inData = 0 ;
      unsigned    numBytes = 0 ;
      if( fIn.worked() )
      {
         inData = (char const *)fIn.getData();
         numBytes = fIn.getLength();
      }
      else
         printf( "no input data\n" );
         
      pollHandlerSet_t fds ;
      getTimerPoll( fds );
      dnsPoll_t &dns = dnsPoll_t::get( fds );

      printf( "client constructed\n" );
      in_addr server ;
      tcpHandler_t *pTCP ;
      if( 0 != inet_aton( argv[1], &server ) )
         pTCP = new tcpHandler_t( server.s_addr, port, fds );
      else
         pTCP = new tcpHandler_t( argv[1], port, fds );

      tcpHandler_t &tcp = *pTCP ;
      printf( "tcp constructed\n" );
      myTTY_t myTTY( fds, tcp );
      printf( "tty constructed\n" );

      tcp.setCallback( tcpHandler_t::connect_e, onConnect, 0 );
      tcp.setCallback( tcpHandler_t::disconnect_e, onDisconnect, 0 );
      tcp.setCallback( tcpHandler_t::dataAvail_e, onDataAvail, 0 );
      tcp.setCallback( tcpHandler_t::writeSpaceAvail_e, onWriteSpace, 0 );
      tcp.setCallback( tcpHandler_t::error_e, onError, 0 );
   
      while( !doExit_ && ( tcp.closed != tcp.getState() ) )
      {
         fds.poll( 1000 );
      }
   }
   else
      fprintf( stderr, "Usage: tcpPoll server [port=80]\n" );

   return 0 ;
}

#endif
