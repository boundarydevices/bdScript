/*
 * Module popen.cpp
 *
 * This module defines the methods of the popen_t
 * class as declared in popen.h
 *
 *
 * Change History : 
 *
 * $Log: popen.cpp,v $
 * Revision 1.2  2002-12-10 04:10:02  ericn
 * -fixed problems with timeout
 *
 * Revision 1.1  2002/12/09 15:21:11  ericn
 * -added module popen
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "popen.h"
#include <errno.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>

popen_t :: popen_t( char const *cmdLine )
   : fIn_( popen( cmdLine, "r" ) ),
     errorMsg_( "open:" ),
     tempBuf_()
{
   if( 0 == fIn_ )
      errorMsg_ += strerror( errno );
}
   
popen_t :: ~popen_t( void )
{
   if( fIn_ )
      pclose( fIn_ );
}

//
// call this to get the next line of text, or timeout
// returns true if another line of text has become available
//
bool popen_t :: getLine
   ( std::string &line,
     unsigned     milliseconds )
{
   // first check to see if we got multiple lines on a previous
   // call
   for( unsigned i = 0 ; i < tempBuf_.size(); i++ )
   {
      if( '\n' == tempBuf_[i] )
      {
         line = std::string( tempBuf_.c_str(), i );
         tempBuf_ = std::string( tempBuf_.c_str() + i + 1, tempBuf_.size() - i - 1 );
         return true ;
      }
   }

   if( fIn_ )
   {
      int const fd = fileno( fIn_ );

      // set non-blocking mode
      int fcRes = fcntl( fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

      while( 1 )
      {
         pollfd rdPoll ;
         rdPoll.fd = fd ;
         rdPoll.events = POLLIN | POLLERR ;
         int const numEvents = poll( &rdPoll, 1, milliseconds );
         if( 1 == numEvents )
         {
            if( 0 != ( POLLIN & rdPoll.events ) )
            {
               char inBuf[256];
               int numRead = read( fd, inBuf, sizeof( inBuf ) );
               if( 0 < numRead )
               {
                  for( int b = 0 ; b < numRead ; b++ )
                  {
                     if( '\n' == inBuf[b] )
                     {
                        line = tempBuf_ ;
                        line.append( inBuf, b );
                        tempBuf_ = std::string( inBuf+b+1, numRead-b-1 );
                        return true ;
                     } // have end of line, return
                  }
   
                  //
                  // no end-of-line found
                  //
                  tempBuf_.append( inBuf, numRead );
               }
               else
                  break; // eof
            }
            else if( 0 != ( POLLERR & rdPoll.events ) )
            {
               close( fd );
               break;
            }
         }
         else
         {
            close( fd );
            break;
         }
      }

      pclose( fIn_ );
      fIn_ = 0 ;

      if( 0 < tempBuf_.size() )
      {
         line = tempBuf_ ;
         tempBuf_ = "" ;
         return true ;
      } // unterminated line at eof
   }

   line = "" ;
   return false ;
}

#ifdef STANDALONE
#include <stdio.h>

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      popen_t proc( argv[1] );
      if( proc.isOpen() )
      {
         std::string line ;
         int         lineNo = 0 ;
         while( proc.getLine( line, 2000 ) )
         {
            printf( ":%u:%s\n", ++lineNo, line.c_str() );
         }
         printf( "[eof]\n" );
      }
      else
         fprintf( stderr, "Error %s opening process\n", proc.getErrorMsg().c_str() );
   }
   else
      fprintf( stderr, "Usage : popen cmdLine\n" );

   return 0 ;
}

#endif
