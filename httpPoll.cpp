/*
 * Module httpPoll.cpp
 *
 * This module defines the methods of the httpPoll_t class
 * as declared in httpPoll.h
 *
 *
 * Change History : 
 *
 * $Log: httpPoll.cpp,v $
 * Revision 1.3  2005-11-05 23:23:26  ericn
 * -fixed compiler warnings
 *
 * Revision 1.2  2004/12/18 18:30:28  ericn
 * -require tcpHandler, not more generic pollHandler
 *
 * Revision 1.1  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "httpPoll.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dnsPoll.h"
#include "pollTimer.h"

httpPoll_t :: httpPoll_t
   ()
   : h_( 0 )
   , file_( "/" )
   , state_( idle )
{
   addHeader( "Keep-Alive", "300" );
   addHeader( "Connection", "keep-alive" );
}

httpPoll_t :: ~httpPoll_t( void )
{
}

static std::string hostHeader( "Host" );

void httpPoll_t :: addHeader( std::string const &fieldName, std::string const &value )
{
   header_t h ;
   h.fieldName_ = fieldName ;
   h.value_     = value ;
   headers_.push_back( h );
}

void httpPoll_t :: addParam( std::string const &fieldName, std::string const &value )
{
   header_t h ;
   h.fieldName_ = fieldName ;
   h.value_     = value ;
   params_.push_back( h );
}

void httpPoll_t :: addFile( std::string const &fieldName, std::string const &fileName )
{
   header_t h ;
   h.fieldName_ = fieldName ;
   h.value_     = fileName ;
   files_.push_back( h );
}

void httpPoll_t :: setName( char const *path )
{
   file_ = path ;
}

void httpPoll_t :: setHandler( tcpHandler_t *h )
{
   h_ = h ;
   if( h )
      addHeader( hostHeader, h->hostName() );
}

void httpPoll_t :: clearHandler()
{
   h_ = 0 ;
}

static bool onConnect( tcpHandler_t &handler, int event, void *opaque )
{
   return ((httpPoll_t *)opaque )->onConnect( handler );
}

static bool onDataAvail( tcpHandler_t &handler, int event, void *opaque )
{
   return ((httpPoll_t *)opaque )->onDataAvail( handler );
}

static bool onWriteSpace( tcpHandler_t &handler, int event, void *opaque )
{
   return ((httpPoll_t *)opaque )->onWriteSpace( handler );
}

static bool onError( tcpHandler_t &handler, int event, void *opaque )
{
   return ((httpPoll_t *)opaque )->onError( handler );
}

/*
static bool onHUP( tcpHandler_t &handler, int event, void *opaque )
{
   return ((httpPoll_t *)opaque )->onHUP( handler );
}
*/

static bool onDisconnect( tcpHandler_t &handler, int event, void *opaque )
{
   return ((httpPoll_t *)opaque )->onDisconnect( handler );
}

void httpPoll_t :: start( void )
{
   if( ( idle == getState() )
       &&
       ( 0 != h_ ) )
   {
      h_->setCallback( tcpHandler_t::connect_e, ::onConnect, this );
      h_->setCallback( tcpHandler_t::disconnect_e, ::onDisconnect, this );
      h_->setCallback( tcpHandler_t::dataAvail_e, ::onDataAvail, this );
      h_->setCallback( tcpHandler_t::writeSpaceAvail_e, ::onWriteSpace, this );
      h_->setCallback( tcpHandler_t::error_e, ::onError, this );
      if( tcpHandler_t::connected == h_->getState() )
      {
         onConnect( *h_ );
      }
      else
         state_ = waitConnect ;
   }
   else
      fprintf( stderr, "Duplicate or invalid httpPoll_t::start(): %d\n", getState() );
}

bool httpPoll_t :: getAuth( char const *&username, char const *&password )
{
   username = 0 ;
   password = 0 ;
   return false ;
}

void httpPoll_t :: onWriteData( unsigned long writtenSoFar )
{
}

void httpPoll_t :: writeSize( unsigned long numToWrite )
{
}

void httpPoll_t :: readSize( unsigned long numBytes )
{
}

void httpPoll_t :: onReadData( unsigned long readSoFar )
{
}

bool httpPoll_t :: onConnect( tcpHandler_t &h )
{
   if( h_ && ( h_ == &h ) )
   {
      struct in_addr in ;
      in.s_addr = h.getServerIp();
      printf( "Connected to server %s, ip %s, port %u\n", 
              h.hostName(), 
              inet_ntoa( in ), 
              ntohs( h.getServerPort() ) );
      int numWritten = write( h.getFd(), "GET ", 4 );
      if( 4 == numWritten )
      {
         numWritten = write( h.getFd(), file_.c_str(), file_.size() );
         if( file_.size() == (unsigned)numWritten )
         {                              //  12345678901
            numWritten = write( h.getFd(), " HTTP/1.1\r\n", 11 );
            if( 11 == numWritten )
            {
               std::list<header_t>::const_iterator it = headers_.begin();
               for( ; it != headers_.end(); it++ )
               {
                  header_t const &header = (*it);
                  numWritten = write( h.getFd(), header.fieldName_.c_str(), header.fieldName_.size() );
                  if( (unsigned)numWritten == header.fieldName_.size() )
                  {
                     if( 2 == write( h.getFd(), ": ", 2 ) )
                     {
                        numWritten = write( h.getFd(), header.value_.c_str(), header.value_.size() );
                        if( (unsigned)numWritten == header.value_.size() )
                        {
                           if( 2 == write( h.getFd(), "\r\n", 2 ) )
                           {
printf( "sent header %s/%s\n", header.fieldName_.c_str(), header.value_.c_str() );
                           }
                           else
                           {
                              printf( "writeErr4\n" );
                              break;
                           }
                        }
                        else
                        {
                           printf( "writeErr5\n" );
                           break;
                        }
                     }
                     else
                     {
                        printf( "writeErr6\n" );
                        break;
                     }
                  }
                  else
                  {
                     printf( "writeErr7\n" );
                     break;
                  }
               }
               write( h.getFd(), "\r\n", 2 );
            }
            else
            {
               printf( "writeErr3\n" );
               onError( h );
            }
         }
         else
         {
            printf( "writeErr2\n" );
            onError( h );
         }
      }
      else
      {
         printf( "writeErr\n" );
         onError( h );
      }
   }
   else
      printf( "Invalid tcp handle %p/%p\n", h_, &h );
   
   return false ;
}

bool httpPoll_t :: onDisconnect( tcpHandler_t &h )
{
   if( complete != getState() )
      state_ = error ;

   printf( "Disconnect\n" );
   return true ;
}

bool httpPoll_t :: onDataAvail( pollHandler_t &h )
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
      if( sizeof( inData ) < (unsigned)numAvail )
         numAvail = sizeof( inData );
      int const numRead = read( h.getFd(), inData, numAvail );
      if( 0 < numRead )
      {
         write( fileno( stdout ), inData, numRead );
         fflush( stdout );
      }
   }
   else if( 0 == ioctlResult )
   {
printf( "?? closed ??\n" );
      h.onHUP();
   }
   else
      perror( "FIONREAD" );
   return true ;
}

bool httpPoll_t :: onWriteSpace( pollHandler_t &h )
{
   printf( "writeSpace\n" );
   return false ;
}

bool httpPoll_t :: onError( pollHandler_t &h )
{
   printf( "Error\n" );
   onDisconnect( *(tcpHandler_t *)&h );
   return false ;
}

bool httpPoll_t :: onHUP( pollHandler_t &h )
{
   printf( "HUP\n" );
   onDisconnect( *(tcpHandler_t *)&h );
   return false ;
}


#ifdef __MODULETEST__
#include <termios.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "ttyPoll.h"
#include "memFile.h"

#include <signal.h>
#include "hexDump.h"
#include <execinfo.h>
#include "ttyPoll.h"

static bool doExit_ = false ;

class myTTY_t : public ttyPollHandler_t {
public:
   myTTY_t( pollHandlerSet_t  &set,
            tcpHandler_t      &sock,
            httpPoll_t        &http )
      : ttyPollHandler_t( set, 0 ), sock_( sock ), http_( http ){}
   virtual void onLineIn( void );
   virtual void onCtrlC( void );

   tcpHandler_t &sock_ ;
   httpPoll_t   &http_ ;
};

void myTTY_t :: onLineIn( void )
{
   printf( "ttyIn: %s\n", getLine() );
   char *sIn = (char *)getLine();
   if( *sIn )
   {
      char *parts[3]; // header,
      char *tmp ;
      unsigned i = 0 ;
      char *next = strtok_r( sIn, "\t", &tmp );
      while( ( 0 != next ) && ( 3 > i ) )
      {
         parts[i++] = next ;
         next = strtok_r( 0, "\t", &tmp );
      }

      if( 0 == next )
      {
         if( 3 == i )
         {
            if( 0 == strcmp( "header", parts[0] ) ) 
            {
               printf( "header<%s> == %s\n", parts[1], parts[2] );
               http_.addHeader( parts[1], parts[2] );
            }
            else if( 0 == strcmp( "param", parts[0] ) ) 
            {
               printf( "param<%s> == %s\n", parts[1], parts[2] );
               http_.addParam( parts[1], parts[2] );
            }
            else if( 0 == strcmp( "file", parts[0] ) ) 
            {
               printf( "file<%s> == %s\n", parts[1], parts[2] );
               http_.addFile( parts[1], parts[2] );
            }
            else
               printf( "unknown cmd: %s\n"
                       "options are:\n"
                       "   header <tab> name <tab> value    - add custom header\n"
                       "   param <tab> name <tab> value     - add post param\n"
                       "   file <tab> name <tab> value      - add file post param\n",
                       parts[0] );
         }
         else if( 2 == i )
         {
            if( 0 == strcmp( "GET", parts[0] ) )
            {
               printf( "get %s\n", parts[1] );
               http_.setName( parts[1] );
               http_.setHandler( &sock_ );
               http_.start();
            }
            else if( 0 == strcmp( "POST", parts[0] ) )
            {
            }
            else
               printf( "unknown cmd %s\n", parts[0] );
         }
         else
         {
            unsigned const len = strlen( sIn );
            printf( "--> %s..", sIn );
            int numWritten = write( sock_.getFd(), sIn, len )
                           + write( sock_.getFd(), "\r\n", 2 );
            printf( "%d bytes\n", numWritten );
         }
      }
      else
         fprintf( stderr, "too many params\n" );
   }
   else
   {
      printf( "sockState 0x%02x\n", sock_.getState() );
      printf( "httpState 0x%02x\n", http_.getState() );
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
   if( 1 < argc )
   {
      pollHandlerSet_t fds ;
      printf( "client constructed\n" );

      dnsInit( fds );
      
      tcpHandler_t *tcp = 0 ;
      
      unsigned short port = htons( 80 );
      if( 2 < argc )
         port = htons( (unsigned short)strtoul( argv[2], 0, 0 ) );

      printf( "server %s, port %u\n", argv[1], ntohs(port) );

      in_addr server ;
      if( 0 != inet_aton( argv[1], &server ) )
         tcp = new tcpHandler_t( server.s_addr, port, fds );
      else
         tcp = new tcpHandler_t( argv[1], port, fds );
         
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
         
      httpPoll_t http ;
      printf( "http constructed\n" );
      myTTY_t myTTY( fds, *tcp, http );
      printf( "tty constructed\n" );

      while( !doExit_ && ( tcp->closed != tcp->getState() ) )
      {
         fds.poll( 1000 );
      }
   }
   else
      fprintf( stderr, "Usage: tcpPoll server [port=80]\n" );

   return 0 ;
}

#endif
