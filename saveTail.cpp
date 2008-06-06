/*
 * Program saveTail.cpp
 *
 * This program will save the tail end of a child process's
 * stdout and stderr to the a (presumably temporary) file 
 * named by the pid of the child process. The first command-line
 * argument will determine how much output is saved. The file
 * will be re-written each time the limit is reached, saving 
 * half of the output. 
 * 
 * I know this is wasteful in time, but it makes reading the 
 * output easier and is simple to write.
 *
 * Change History : 
 *
 * $Log: saveTail.cpp,v $
 * Revision 1.1  2008-06-06 21:48:10  ericn
 * -import
 *
 * Revision 1.2  2007-08-23 00:31:32  ericn
 * -allow const parameters
 *
 * Revision 1.1  2007/08/14 12:59:26  ericn
 * -import
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include "processWrap.h"
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include "fbDev.h"
#include "pollHandler.h"

static bool die = false ;
static void ctrlcHandler( int signo )
{
   die = true ;
}

class myHandler_t : public pollHandler_t {
public:
   myHandler_t( int fd, char id, pollHandlerSet_t &set, int fdOut, unsigned maxBytes )
      : pollHandler_t( fd, set )
      , id_(id)
      , fdOut_( fdOut )
      , maxBytes_( maxBytes )
   {
      setMask( POLLIN|POLLERR|POLLHUP );
      set.add( *this );
   }
   virtual ~myHandler_t( void ){}

   virtual void onDataAvail( void );     // POLLIN
   virtual void onError( void );     // POLLIN
   virtual void onHUP( void );     // POLLIN
   char const id_ ;
   int const fdOut_ ;
   unsigned const maxBytes_ ;
};

void myHandler_t :: onDataAvail( void )
{
   char inBuf[256];
   int numRead ; 
   while( 0 < (numRead = read( fd_, inBuf, sizeof(inBuf) )) )
   {
      off_t where = lseek( fdOut_, 0, SEEK_CUR );
      if( -1UL != where ){
         unsigned left = maxBytes_ > where ? maxBytes_ - where : 0 ;
         if( left < numRead ){
            char inBuf2[4096];
            unsigned const half = maxBytes_ / 2 ;
            int numRead2 ;
            unsigned offs = 0 ;
            do {
               lseek( fdOut_, half+offs, SEEK_SET );
               numRead2 = read( fdOut_, inBuf2, sizeof(inBuf2));
               if( 0 < numRead2 ){
                  lseek( fdOut_, offs, SEEK_SET );
                  write( fdOut_, inBuf2, numRead2 );
                  offs += numRead2 ;
               }
            } while( 0 < numRead2 );
            ftruncate( fdOut_, offs );
            lseek( fdOut_, offs, SEEK_SET );
            left = maxBytes_ - offs ;
         } // overflow
         write( fdOut_, inBuf, numRead );
      }
      else
         perror( "lseek" );
   }
   if( 0 == numRead )
      onHUP();
}

void myHandler_t :: onError( void )         // POLLERR
{
}

void myHandler_t :: onHUP( void )           // POLLHUP
{
   close();
}

int main( int argc, char **argv )
{
   if( 4 <= argc )
   {
      unsigned long maxBytes = strtoul(argv[1], 0, 0);
      if( 0 == maxBytes ){
         fprintf( stderr, "Invalid byte count %s\n", argv[1] );
         return -1 ;
      }
      processWrap_t child( argv[3], argv+3 );
      if( 0 <= child.pid() ){
         char outFileName[512];
         snprintf( outFileName, sizeof(outFileName), "%s.%d", argv[2], child.pid() );
         int fdOut = open( outFileName, O_RDWR|O_CREAT );
         if( 0 <= fdOut ){
            pollHandlerSet_t handlers ;
            myHandler_t childOut( child.child_stdout(), 'o', handlers, fdOut, maxBytes );
            myHandler_t childErr( child.child_stderr(), 'e', handlers, fdOut, maxBytes );
   
            signal( SIGINT, ctrlcHandler );
   
            while( childOut.isOpen() && !die ){
               if( !handlers.poll(500) ){
               }
            }
   
            if( childOut.isOpen() ){
               child.shutdown();
            }
   
         }
         else {
            perror( outFileName );
         }

         child.wait();
      }
   }
   else
      fprintf( stderr, "Usage: %s maxBytes outFile command arguments\n", argv[0] );

   return 0 ;
}
