/*
 * Module pollHandler.cpp
 *
 * This module defines the methods of the pollHandler_t
 * and pollHandlerSet_t classes as declared in pollHandler.h
 *
 *
 * Change History : 
 *
 * $Log: pollHandler.cpp,v $
 * Revision 1.4  2004-03-27 20:24:14  ericn
 * -DEBUGPRINT
 *
 * Revision 1.3  2004/02/08 10:34:55  ericn
 * -added pollClient_t
 *
 * Revision 1.2  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.1  2003/10/05 19:15:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "pollHandler.h"
#include <string.h>
#include <stdio.h>

// #define DEBUGPRINT
#include "debugPrint.h"

pollHandler_t :: pollHandler_t
   ( int               fd,
     pollHandlerSet_t &set )
   : fd_( fd )
   , parent_( set )
   , mask_( 0 )
{
}
   
void pollHandler_t :: onDataAvail( void ){ debugPrint( "pollHandler\n" ); }     // POLLIN
void pollHandler_t :: onWriteSpace( void ){ debugPrint( "pollHandler\n" ); }    // POLLOUT
void pollHandler_t :: onError( void ){ debugPrint( "pollHandler\n" ); }         // POLLERR
void pollHandler_t :: onHUP( void ){ debugPrint( "pollHandler\n" ); }           // POLLHUP

pollHandler_t :: ~pollHandler_t( void )
{
   parent_.removeMe( *this );
}

pollClient_t :: ~pollClient_t( void )
{
}

unsigned short pollClient_t :: getMask( void )
{
   debugPrint( "pollClient_t::getMask()" );
   return 0 ;
}

void pollClient_t :: onDataAvail( pollHandler_t & ){ debugPrint( "pollClient_t::onDataAvail" ); }
void pollClient_t :: onWriteSpace( pollHandler_t & ){ debugPrint( "pollClient_t::onWriteSpace" ); }
void pollClient_t :: onError( pollHandler_t & ){ debugPrint( "pollClient_t::onError" ); }
void pollClient_t :: onHUP( pollHandler_t & ){ debugPrint( "pollClient_t::onHUP" ); }


void pollHandler_t :: setMask( short events )
{
   mask_ = events ;
   parent_.setMask( *this, events );
}


pollHandlerSet_t :: pollHandlerSet_t( void )
   : inPollLoop_( 0 )
   , numHandlers_( 0 )
{
   memset( handlers_, 0, sizeof( handlers_ ) );
   memset( fds_, 0, sizeof( fds_ ) );
}

pollHandlerSet_t :: ~pollHandlerSet_t( void )
{
}

void pollHandlerSet_t :: add( pollHandler_t &handler )
{
   if( maxHandlers_ > numHandlers_ )
   {
      for( unsigned i = 0 ; i < numHandlers_ ; i++ )
      {
         if( handlers_[i] == &handler )
            return ;
      } // make sure it's not already in the list
      
      handlers_[numHandlers_]   = &handler ;
      fds_[numHandlers_].events = handler.getMask();
      fds_[numHandlers_].fd     = handler.getFd();
      deleted_[numHandlers_]    = false ;
      ++numHandlers_ ;
   }
   else
      fprintf( stderr, "!!!!!!!!!!! max poll handlers exceeded !!!!!!!!!!!\n" );
}

void pollHandlerSet_t :: removeMe( pollHandler_t &handler )
{
   if( !inPollLoop_ )
   {
      for( unsigned i = 0 ; i < numHandlers_ ; i++ )
      {
         if( handlers_[i] == &handler )
         {
            if( i < numHandlers_-1 )
            {
               unsigned numToShift = numHandlers_-i-1 ;
               memcpy( handlers_+i, handlers_+i+1, numToShift*sizeof(handlers_[0]) );
               memcpy( fds_+i, fds_+i+1, numToShift*sizeof(fds_[0]) );
            } // shift everything down
   
            --numHandlers_ ;
            break ;
         }
      } // find in the list
   }
   else
   {
      for( unsigned i = 0 ; i < numHandlers_ ; i++ )
      {
         if( handlers_[i] == &handler )
         {
            deleted_[i] = true ;
            break ;
         }
      } // find in the list
   } // in poll loop, defer removal
}

//
// returns true if at least one handler wants notification
//
bool pollHandlerSet_t :: poll( int ms )
{
debugPrint( "polling %u handles\n", numHandlers_ );
for( unsigned h = 0 ; h < numHandlers_ ; h++ )
{
   debugPrint( "[%u].fd_ = %d, .mask_ = %x\n", h, fds_[h].fd, fds_[h].events );
}

   int const numReady = ::poll( fds_, numHandlers_, ms );
   if( 0 < numReady )
   {
      inPollLoop_ = true ;
      for( unsigned i = 0 ; i < numHandlers_ ; i++ )
      {
         pollfd &fd = fds_[i];
         short const signals = fd.events & fd.revents ;
debugPrint( "[%u].fd_ = %d, .mask_ = %x, .events = %x, signals = %x\n", i, fds_[i].fd, fds_[i].events, fds_[i].revents, signals );
         if( signals )
         {
            pollHandler_t *handler = handlers_[i];

            if( signals & POLLIN )
               handler->onDataAvail();

            if( signals & POLLOUT )
               handler->onWriteSpace();
            
            if( signals & POLLERR )
               handler->onError();
            
            if( signals & POLLHUP )
               handler->onHUP();
         }
      }
      inPollLoop_ = false ;
      
      //
      // check for deferred deletes
      //
      unsigned numDeleted = 0 ;
      for( unsigned i = 0 ; i < numHandlers_ ; i++ )
      {
         if( deleted_[i] )
         {
            if( i < numHandlers_-1 )
            {
               unsigned numToShift = numHandlers_-i-1 ;
               memcpy( handlers_+i, handlers_+i+1, numToShift*sizeof(handlers_[0]) );
               memcpy( fds_+i, fds_+i+1, numToShift*sizeof(fds_[0]) );
            } // shift everything down
   
            ++numDeleted ;
         }
      } // check for deleted items
      numHandlers_ -= numDeleted ;
   }

debugPrint( "done polling %u handles\n", numHandlers_ );
   return 0 != numReady ;
}

void pollHandlerSet_t :: setMask( pollHandler_t &handler, short newMask )
{
   for( unsigned i = 0 ; i < numHandlers_ ; i++ )
   {
      if( handlers_[i] == &handler )
      {
         fds_[i].events = newMask ;
         break;
      }
   }
}

#ifdef STANDALONE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

class readAll_t : public pollHandler_t {
public:
   readAll_t( int               fd,
              pollHandlerSet_t &set )
      : pollHandler_t( fd, set ){}
   
   virtual void onDataAvail( void ){ debugPrint( "POLLIN\n" ); setMask( getMask() & ~POLLIN ); }    // POLLIN
   virtual void onWriteSpace( void ){ debugPrint( "POLLOUT\n" ); setMask( getMask() & ~POLLOUT ); } // POLLOUT
   virtual void onError( void ){ debugPrint( "POLLERR\n" ); setMask( getMask() & ~POLLERR ); }      // POLLERR
   virtual void onHUP( void ){ debugPrint( "POLLHUP\n" ); setMask( getMask() & ~POLLHUP ); }        // POLLHUP
   
};

int main( int argc, char const *const argv[] )
{
   if( 1 < argc )
   {
      pollHandlerSet_t handlers ;
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         int fd = open( argv[arg], O_RDONLY );
         if( 0 <= fd )
         {
            debugPrint( "opened %s: fd %d\n", argv[arg], fd );
            int doit = 1 ;
            int iRes = ioctl( fd, O_NONBLOCK, &doit );
            if( 0 != iRes )
               perror( "O_NONBLOCK" );
            pollHandler_t *handler = new readAll_t( fd, handlers );
            handler->setMask( POLLIN );
            handlers.add( *handler );
         }
         else
            perror( argv[arg] );
      }

      if( 0 < handlers.numHandlers() )
      {
         int iterations = 0 ;
         while( 1 )
         {
            handlers.poll( -1 );
            debugPrint( "poll %d\n", ++iterations );
         }
      }
      else
         fprintf( stderr, "No devices\n" );
   }
   else
      fprintf( stderr, "Usage: pollHandlerTest device [device2...]\n" );

   return 0 ;
}

#endif
