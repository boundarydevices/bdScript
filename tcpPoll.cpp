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
 * Revision 1.2  2004-03-17 04:56:19  ericn
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
#include "debugPrint.h"

tcpClient_t :: ~tcpClient_t( void )
{
}

void tcpClient_t :: onConnect( tcpHandler_t & ){ debugPrint( "tcpClient_t::onConnect" ); }
void tcpClient_t :: onDisconnect( tcpHandler_t & ){ debugPrint( "tcpClient_t::onDisconnect" ); }


tcpHandler_t :: tcpHandler_t
   ( unsigned long     serverIP,          // network byte order
     unsigned short    serverPort,        // network byte order
     pollHandlerSet_t &set,
     tcpClient_t      *client )
   : pollHandler_t( socket( AF_INET, SOCK_STREAM, 0 ), set )
   , serverIP_( serverIP )
   , serverPort_( serverPort )
   , state_( closed )
   , clients_( 0 )
{
   if( 0 < getFd() )
   {
      fcntl( getFd(), F_SETFD, FD_CLOEXEC );
      fcntl( getFd(), F_SETFL, O_NONBLOCK );
      
      sockaddr_in serverAddr ;
      memset( &serverAddr, 0, sizeof( serverAddr ) );
      
      serverAddr.sin_family      = AF_INET;
      serverAddr.sin_addr.s_addr = serverIP ;
      serverAddr.sin_port        = serverPort ;
   
      int const connectResult = connect( getFd(), (struct sockaddr *) &serverAddr, sizeof( serverAddr ) );
      if( ( 0 == connectResult )
          ||
          ( ( -1 == connectResult ) && ( EINPROGRESS == errno ) ) )
      {
         int doit = 1 ;
         setsockopt( getFd(), SOL_SOCKET, TCP_NODELAY, &doit, sizeof( doit ) );
         state_ = connecting ;
         setMask( POLLIN|POLLOUT|POLLERR|POLLHUP );
         set.add( *this );
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

   if( client )
      addClient( *client );
}

tcpHandler_t :: ~tcpHandler_t( void )
{
   while( clients_ )
   {
      client_t *old = clients_ ;
      clients_ = old->next_ ;
      delete old ;
   }

   parent_.removeMe( *this );
   close();
}

void tcpHandler_t :: addClient( tcpClient_t &client )
{
   removeClient( client ); // no duplicates!

   client_t *newClient = new client_t ;
   newClient->client_ = &client ;
   newClient->next_   = clients_ ;
   clients_ = newClient ;
   if( connected == getState() )
      client.onConnect( *this );
   else if( closed == getState() )
      client.onDisconnect( *this );
}

void tcpHandler_t :: removeClient( tcpClient_t &client )
{
   client_t *rear = 0 ;
   client_t *front = clients_ ;
   while( front )
   {
      if( front->client_ == &client )
      {
         if( 0 == rear )
            clients_ = front->next_ ;
         else
            rear->next_ = front->next_ ;
         delete front ;
         front = 0 ; // terminate loop
      }
      else
      {
         rear = front ;
         front = rear->next_ ;
      }
   }
}

void tcpHandler_t :: onDataAvail( void )     // POLLIN
{
   checkStatus();
   if( connected == state_ )
   {
      unsigned short const prevMask = getMask();
      unsigned short mask = prevMask & ~POLLIN ;
      client_t *cl = clients_ ;
      while( cl )
      {
         cl->client_->onDataAvail( *this );
         mask |= ( cl->client_->getMask() & POLLIN );
         cl = cl->next_ ;
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
      client_t *cl = clients_ ;
      while( cl )
      {
         cl->client_->onWriteSpace( *this );
         mask |= ( cl->client_->getMask() & POLLOUT );
         cl = cl->next_ ;
      }

      if( mask != prevMask )
         setMask( mask );
   } // defer to clients
}

void tcpHandler_t :: onError( void )         // POLLERR
{
   checkStatus();
   client_t *cl = clients_ ;
   while( cl )
   {
      cl->client_->onError( *this );
      cl = cl->next_ ;
   }
}

void tcpHandler_t :: onHUP( void )           // POLLHUP
{
   client_t *cl = clients_ ;
   while( cl )
   {
      cl->client_->onDisconnect( *this );
      cl = cl->next_ ;
   }
   close();
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
         client_t *cl = clients_ ;
         while( cl )
         {
            cl->client_->onConnect( *this );
            cl = cl->next_ ;
         }
         state_ = connected ;
      }
   }
   else
   {
      printf( "closed:err 0x%x\n", err );
      close();
      state_ = closed ;
      client_t *cl = clients_ ;
      while( cl )
      {
         cl->client_->onDisconnect( *this );
         cl = cl->next_ ;
      }
      setMask( 0 );
   }
}

#ifdef __MODULETEST__
#include <termios.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "ttyPoll.h"
#include "memFile.h"

class ttyClient_t : public tcpClient_t {
public:
   ttyClient_t( char const * const inData,
                unsigned long numBytes );
   virtual ~ttyClient_t( void );
   virtual void onConnect( tcpHandler_t & );
   virtual unsigned short getMask( void );
   virtual void onDataAvail( pollHandler_t & );     // POLLIN
   virtual void onWriteSpace( pollHandler_t & );    // POLLOUT
   virtual void onError( pollHandler_t & );         // POLLERR
   virtual void onHUP( pollHandler_t & );           // POLLHUP
   virtual void onDisconnect( tcpHandler_t & );

   char const *  inData_ ;
   unsigned long numBytes_ ;    
};

ttyClient_t :: ttyClient_t
   ( char const * const inData,
     unsigned long numBytes )
   : inData_( inData )
   , numBytes_( numBytes )
{
}

ttyClient_t :: ~ttyClient_t( void )
{
}

void ttyClient_t :: onConnect( tcpHandler_t & )
{
   printf( "Connected!\n" );
}

unsigned short ttyClient_t :: getMask( void )
{
   unsigned mask = POLLIN | POLLERR | POLLHUP ;
   if( 0 < numBytes_ )
      mask |= POLLOUT ;
   return mask ;
}

void ttyClient_t :: onDataAvail( pollHandler_t &h )     // POLLIN
{
   printf( "dataAvail\n" );
   int numAvail ;
   int const ioctlResult = ioctl( h.getFd(), FIONREAD, &numAvail );
   if( ( 0 == ioctlResult )
       &&
       ( 0 < numAvail ) )
   {
printf( "%u bytes\n", numAvail );

      char inData[2048];
      if( sizeof( inData ) < numAvail )
         numAvail = sizeof( inData );
      int const numRead = read( h.getFd(), inData, numAvail );
      if( 0 < numRead )
      {
         write( fileno( stdout ), inData, numRead );
         fflush( stdout );
      }
      return ;
   }
   else if( 0 == ioctlResult )
   {
printf( "?? closed ??\n" );
      h.onHUP();
   }
   else
      perror( "FIONREAD" );
}

void ttyClient_t :: onWriteSpace( pollHandler_t &h )    // POLLOUT
{
   printf( "writeSpace\n" );
   if( 0 < numBytes_ )
   {
      int const numWritten = write( h.getFd(), inData_, numBytes_ );
      if( 0 < numWritten )
      {
         numBytes_ -= numWritten ;
         inData_   += numWritten ;
      }
      else
         printf( "write error: %d:%m\n", numWritten );
   }
   else
      printf( "nothing to write\n" );
}

void ttyClient_t :: onError( pollHandler_t & )         // POLLERR
{
   printf( "Error\n" );
}

void ttyClient_t :: onHUP( pollHandler_t & )           // POLLHUP
{
   printf( "HUP\n" );
}

void ttyClient_t :: onDisconnect( tcpHandler_t & )
{
   printf( "Disconnected!\n" );
}

#include <signal.h>
#include "hexDump.h"
#include <execinfo.h>

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
      in_addr server ;
      if( 0 != inet_aton( argv[1], &server ) )
      {
         unsigned short port = htons( 80 );
         if( 2 < argc )
            port = htons( (unsigned short)strtoul( argv[2], 0, 0 ) );
         printf( "polling IP adress %s:%lx, port 0x%04x\n", argv[1], server.s_addr, port );
         
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
         ttyClient_t tty( inData, numBytes );
         printf( "tty constructed\n" );
         tcpHandler_t tcp( server.s_addr, port, fds, &tty );
         printf( "tcp constructed\n" );
         while( tcp.closed != tcp.getState() )
         {
            fds.poll( 1000 );
            printf( "state 0x%02x\n", tcp.getState() );
         }
         tcp.removeClient( tty );
      }
      else
         fprintf( stderr, "invalid IP address <%s>\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage: tcpPoll server [port=80]\n" );

   return 0 ;
}

#endif
