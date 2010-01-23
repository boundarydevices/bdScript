/*
 * Module logTraces.cpp
 *
 * This module defines the methods of the logTraces_t
 * and traceSource_t classes as declared in logTraces.h
 *
 * Change History : 
 *
 * $Log: logTraces.cpp,v $
 * Revision 1.4  2006-12-01 19:38:08  tkisky
 * -include <assert.h>
 *
 * Revision 1.3  2006/10/10 20:40:44  ericn
 * -make dictionary dynamic
 *
 * Revision 1.2  2006/09/17 15:55:23  ericn
 * -no log by default
 *
 * Revision 1.1  2006/09/10 01:18:13  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

// #define LOGTRACES

#define DEBUGPRINT
#include "debugPrint.h"

#include "logTraces.h"
#include "dictionary.h"
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "rtSignal.h"
#include <signal.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdlib.h>

typedef dictionary_t<std::string> stringDictionary_t ;
static stringDictionary_t *srcDict_ ;
static logTraces_t *instance_ = 0 ;
static int connectSig_ = -1 ;

logTraces_t &logTraces_t::get( void )
{
   if( 0 == instance_ ){
      instance_ = new logTraces_t ;
      assert( 0 == srcDict_ );
      srcDict_ = new stringDictionary_t ;
   }
   return *instance_ ;
}

logTraces_t &logTraces_t::get( bool enable )
{
   logTraces_t::get();

   // This should only be called once
   assert( !instance_->enabled() );
   if( enable ){
      connectSig_ = nextRtSignal();
      instance_->enable();
   }
   return *instance_ ;
}

unsigned logTraces_t::newSource( char const *sourceName )
{
   return (*srcDict_) += sourceName ;
}

void logTraces_t::log( unsigned source, unsigned value )
{
   if( connected() ){
      char outLine[80];
      int len = snprintf( outLine, sizeof(outLine), "%lu\t%u\t%u\n", 
                          *(unsigned long const*)oscr_,
                          source, value );
      write( connectFd_, outLine, len );
   }
}

static void connectHandler( int signo, siginfo_t *info, void *context )
{
   sleep(3);
   if( instance_ && !instance_->connected() ){
      instance_->connect();
   }
   else if( instance_ )
      instance_->disconnect();
}

logTraces_t::logTraces_t( void )
   : memFd_( -1 )
   , mem_( MAP_FAILED )
   , oscr_( 0 )
   , listenFd_( -1 )
   , connectFd_( -1 )
{
}

#define MAP_SIZE 4096
#define MAP_MASK ((MAP_SIZE)-1)
#define OSCR 0x40A00010

void logTraces_t::enable( void )
{
   assert( !enabled() );
   
   listenFd_ = socket( AF_INET, SOCK_STREAM, 0 );
   if( 0 <= listenFd_ ){
      memFd_ = open( "/dev/mem", O_RDWR | O_SYNC );
      if( 0 <= memFd_ ){
         mem_ = mmap( 0, MAP_SIZE, 
                      PROT_READ | PROT_WRITE, 
                      MAP_SHARED, 
                      memFd_, 
                      OSCR & ~MAP_MASK );
         if( MAP_FAILED == mem_ ) {
             perror("mmap()");
             exit(1);
         }
         else
            oscr_ = ((char *)mem_)+(OSCR&MAP_MASK);
      }
      else
         perror( "/dev/mem" );

      int doit = 1 ;
      int result = setsockopt( listenFd_, SOL_SOCKET, SO_REUSEADDR, &doit, sizeof( doit ) );
      if( 0 != result )
         fprintf( stderr, "SO_REUSEADDR:%d:%m\n", result );

      sockaddr_in myAddress ;

      memset( &myAddress, 0, sizeof( myAddress ) );

      myAddress.sin_family      = AF_INET;
      myAddress.sin_addr.s_addr = 0 ; // local
      myAddress.sin_port        = 0x2020 ;

      if( 0 == bind( listenFd_, (struct sockaddr *) &myAddress, sizeof( myAddress ) ) )
      {
         if( 0 == listen( listenFd_, 1 ) )
         {
            fcntl( listenFd_, F_SETFD, FD_CLOEXEC );
            fcntl( listenFd_, F_SETOWN, getpid() );
            fcntl( listenFd_, F_SETSIG, connectSig_ );

            struct sigaction sa ;
         
            sa.sa_flags = SA_SIGINFO|SA_RESTART ;
            sa.sa_restorer = 0 ;
            sigemptyset( &sa.sa_mask );
            sa.sa_sigaction = connectHandler ;
            sigaddset( &sa.sa_mask, connectSig_ );
            sigaction(connectSig_, &sa, 0 );

            fcntl(listenFd_, F_SETFL, fcntl(listenFd_, F_GETFL) | O_NONBLOCK | FASYNC );
            return ;
         }
         else
            perror( "listen" );
      }
      else
         perror( "bind" );

      close( listenFd_ );
      listenFd_ = -1 ;
   }
   else
      perror( "socket" );
}

logTraces_t::~logTraces_t( void )
{
   if( connected() )
      close( connectFd_ );
   if( enabled() )
      close( listenFd_ );
   if( MAP_FAILED != mem_ )
      munmap(mem_,MAP_SIZE);
   if( 0 <= memFd_ )
      close( memFd_ );
}

void logTraces_t::connect(void){
   if( 0 <= connectFd_ ){
      close( connectFd_ );
      connectFd_ = -1 ;
   }
   
   struct sockaddr_in clientAddr ;
   socklen_t sockAddrSize = sizeof( clientAddr );

   connectFd_ = accept( listenFd_, (struct sockaddr *)&clientAddr, &sockAddrSize );
   if( connected() ){
      for( unsigned i = 0 ; i < srcDict_->size(); i++ ){
         std::string const &srcName = (*srcDict_)[i];
         write( connectFd_, srcName.c_str(), srcName.size() );
         write( connectFd_, "\n", 1 );
      }
      write( connectFd_, "\n", 1 );

      fcntl( connectFd_, F_SETFL, (fcntl(connectFd_, F_GETFL) | O_NONBLOCK)& ~FASYNC );
   }
   else
      perror( "accept" );
}

void logTraces_t::disconnect(void){
   if( 0 <= connectFd_ ){
      close( connectFd_ );
      connectFd_ = -1 ;
   }
}

traceSource_t::traceSource_t( char const *name )
   : logger_( logTraces_t::get() )
   , sourceId_( logger_.newSource(name) )
   , value_( 0 )
{
}

traceSource_t::~traceSource_t( void )
{
}

void traceSource_t::increment()
{
   logger_.log( sourceId_, ++value_ );
}

void traceSource_t::decrement()
{
   logger_.log( sourceId_, --value_ );
}


#ifdef MODULETEST

static traceSource_t mySource( "mySource" );

int main( void )
{
   logTraces_t::get( true ); // enable

   unsigned iterations = 0 ;
   while( 1 ){
      TRACE_T( mySource, mytrace );
      printf( "\r%u", iterations++ ); fflush( stdout );
   }

   return 0 ;
}

#endif
