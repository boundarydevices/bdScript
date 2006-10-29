/*
 * Module usblpPoll.cpp
 *
 * This module defines the methods of the usblpPoll_t 
 * class as declared in usblpPoll.h
 *
 *
 * Change History : 
 *
 * $Log: usblpPoll.cpp,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "usblpPoll.h"
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

// #define DEBUGPRINT
#include "debugPrint.h"

usblpPoll_t::usblpPoll_t
   ( pollHandlerSet_t &set,
     char const       *devName,
     unsigned          outBuffer,
     unsigned          inBuffer )
   : pollHandler_t( open( devName, O_RDWR ), set )
   , inBufferLength_( inBuffer )
   , inAdd_( 0 )
   , inTake_( 0 )
   , inData_( isOpen() ? new char [inBufferLength_] : 0 )
   , outBufferLength_( outBuffer )
   , outAdd_( 0 )
   , outTake_( 0 )
   , outData_( isOpen() ? new char [outBufferLength_] : 0 )
{
   if( isOpen() )
   {
      fcntl( fd_, F_SETFD, FD_CLOEXEC );
      fcntl( fd_, F_SETFL, O_NONBLOCK );
      setMask( POLLIN );
      set.add( *this );
      debugPrint( "usblp opened\n" );
   }
}

usblpPoll_t::~usblpPoll_t( void )
{
   parent_.removeMe( *this );
   if( inData_ )
      delete [] inData_ ;
   if( outData_ )
      delete [] outData_ ;
}

//
// This class should normally be overridden.
//
void usblpPoll_t::onDataIn( void )
{
   printf( "%s\n", __PRETTY_FUNCTION__ );
   char inBuf[256];
   unsigned len ;
   while( read( inBuf, sizeof(inBuf), len ) ){
      for( unsigned i = 0 ; i < len ; i++ )
      {
         char const c = inBuf[i];
         if( isprint(c) || ( '\n' == c ) || ( '\r' == c ) )
            fwrite( &c, 1, 1, stdout );
         else {
            char hexBuf[6];
            int hexLen = snprintf( hexBuf, sizeof(hexBuf), "<%02x>", (unsigned)c );
            fwrite( hexBuf, hexLen, 1, stdout );
         }
      }
      fflush( stdout );
   }
}
   
bool usblpPoll_t::read( char *inData, unsigned max, unsigned &numRead )
{
   debugPrint( "%s: %u/%u (%u)", __PRETTY_FUNCTION__, inAdd_, inTake_, inBufferLength_ );
   numRead = 0 ;
   unsigned len = inAdd_-inTake_ ;
   if( len > inBufferLength_ )
      len = inBufferLength_ - inTake_ ;
   if( len > max )
      len = max ;

   memcpy( inData, inData_+inTake_, len );
   inTake_ += len ;
   numRead = len ;

   if( inTake_ >= inBufferLength_ ){
      inTake_ = 0 ;
      if( len < max ){
         max -= len ;
         inData += len ;

         len = inAdd_-inTake_ ;
         if( 0 < len ){
debugPrint( "second segment: %u\n", len );
            if( len > max )
               len = max ;
            memcpy( inData, inData_, len );
            numRead += len ;
            inTake_ += len ;
         }
      }
   }

   debugPrint( ": %u\n", numRead );

   return 0 < numRead ;
}

void usblpPoll_t::onDataAvail( void )
{
   debugPrint( "data avail\n" );
   do {
      unsigned spaceAvail = inTake_ > inAdd_ 
                          ? inTake_-inAdd_- 1      // wrapped
                          : inBufferLength_ - inAdd_ ;
      int numRead = ::read( fd_, inData_+inAdd_, spaceAvail );
      if( 0 < numRead )
      {
         inAdd_ += numRead ;
         if( inBufferLength_ == inAdd_ )
            inAdd_ = 0 ;
         
debugPrint( "%d bytes read/%u/%u\n", numRead, inAdd_, inTake_ );

         if( ((unsigned)(inTake_-1))%inBufferLength_ == inAdd_ ){
debugPrint( "inBuffer full: %u/%u\n", inAdd_, inTake_ );
            break ; // full
         }
      }
      else {
         debugPrint( "%d bytes read: %d/%m\n", numRead, errno );
         break ;
      }
   } while( 1 );

   if( inTake_ != inAdd_ )
      onDataIn();
}

unsigned usblpPoll_t::bytesAvail( void ) const 
{
   unsigned len = inAdd_-inTake_ ;
   if( len > inBufferLength_ ){
      len = inBufferLength_ - inTake_ ;
      len += inAdd_ ;
   } // wrapped

   return len ;
}

void usblpPoll_t::onWriteSpace( void )
{
   do {
debugPrint( "writeSpace avail: %u/%u\n", outAdd_, outTake_ );
      unsigned numAvail = outAdd_ >= outTake_
                          ? outAdd_ - outTake_
                          : outBufferLength_ - outTake_ ;
      if( 0 < numAvail ){
         int numWritten = ::write( fd_, outData_+outTake_, numAvail );
         if( 0 < numWritten )
         {
debugPrint( "%d bytes written to device\n", numWritten );
            outTake_ += numWritten ;
            assert( outTake_ <= outBufferLength_ );
            if( outBufferLength_ == outTake_ )
               outTake_ = 0 ;
         }
         else {
debugPrint( "%d bytes written to device: %d: %m\n", numWritten, errno );
            break ;
         }
      }
      else {
debugPrint( "usblp: no more output data\n" );
         break ;
      }
   } while( 1 );

   if( outAdd_ != outTake_ )
      setMask( getMask() | POLLOUT );
   else
      setMask( getMask() & ~POLLOUT );
}

int usblpPoll_t::write( void const *data, int length )
{
   int numWritten = 0 ; 
   while( 0 < length)
   {
      unsigned spaceAvail = outAdd_ < outTake_ 
                          ? outTake_-outAdd_- 1      // wrapped
                          : outBufferLength_ - outAdd_ ;
      if( 0 < spaceAvail ){
         if( spaceAvail > (unsigned)length )
            spaceAvail = length ;
         memcpy( outData_+outAdd_, data, spaceAvail );
         outAdd_ += spaceAvail ;
         if( outBufferLength_ <= outAdd_ )
            outAdd_ = 0 ; // wrap
         length -= spaceAvail ;
         numWritten += spaceAvail ;
debugPrint( "add segment %u, now %u/%u\n", spaceAvail, outAdd_, outTake_ );
      }
      else {
debugPrint( "No space available\n" );
         break ;
      }
   }

   if( outAdd_ != outTake_ ){
      onWriteSpace();
   }

   return numWritten ;
}


#ifdef MODULETEST_USBLP
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "ttyPoll.h"
#include "memFile.h"

static bool doExit_ = false ;

class myTTY_t : public ttyPollHandler_t {
public:
   myTTY_t( pollHandlerSet_t  &set,
            usblpPoll_t       &lp )
      : ttyPollHandler_t( set, 0 ), lp_( lp ){}
   virtual void onLineIn( void );
   virtual void onCtrlC( void );

   usblpPoll_t &lp_ ;
};

void myTTY_t :: onLineIn( void )
{
   printf( "ttyIn: %s\n", getLine() );
   char const *sIn = getLine();
   if( *sIn )
   {
      unsigned const len = strlen( sIn );
      debugPrint( "--> %s..", sIn );
      int numWritten = lp_.write( sIn, len );
      if( numWritten == len ){
         numWritten = lp_.write( "\r\n", 2 );
         if( 2 != numWritten )
            printf( "%d of 2 bytes\n", numWritten, len );
      }
      else {
         printf( "%d of %d bytes written\n", numWritten, len );
      }
   }
}

void myTTY_t :: onCtrlC( void )
{
   printf( "<ctrl-C>\n" );
   doExit_ = true ;
}

int main( int argc, char const * const argv[] )
{
   pollHandlerSet_t handlers ;
   usblpPoll_t lp( handlers, DEFAULT_USBLB_DEV, 16384, 4096 );
   if( lp.isOpen() ){
      myTTY_t tty( handlers, lp );
   
      for( int arg = 1 ; arg < argc ; arg++ ){
         memFile_t fIn( argv[arg] );
         if( fIn.worked() ){
            int written = lp.write( fIn.getData(), fIn.getLength() );
            if( (unsigned)written != fIn.getLength() )
               printf( "%s: %d of %u bytes written\n", argv[arg], written, fIn.getLength() );
            else
               printf( "%s: %d bytes written\n", argv[arg], written );
         }
         else
            perror( argv[arg] );
      }

      while( !doExit_ )
      {
         handlers.poll( -1 );
      }
   
      printf( "completed\n" ); fflush( stdout );
   }
   else
      perror( "usblp0" );

   return 0 ;
}

#endif
