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
 * Revision 1.1  2003-10-05 19:15:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "pollHandler.h"
#include <string.h>
#include <stdio.h>

pollHandler_t :: pollHandler_t
   ( int               fd,
     pollHandlerSet_t &set )
   : fd_( fd )
   , parent_( set )
   , mask_( 0 )
{
}
   
void pollHandler_t :: onDataAvail( void ){ printf( "pollHandler\n" ); }     // POLLIN
void pollHandler_t :: onWriteSpace( void ){ printf( "pollHandler\n" ); }    // POLLOUT
void pollHandler_t :: onError( void ){ printf( "pollHandler\n" ); }         // POLLERR
void pollHandler_t :: onHUP( void ){ printf( "pollHandler\n" ); }           // POLLHUP

pollHandler_t :: ~pollHandler_t( void )
{
   parent_.remove( *this );
}


void pollHandler_t :: setMask( short events )
{
   mask_ = events ;
   parent_.setMask( *this, events );
}


pollHandlerSet_t :: pollHandlerSet_t( void )
   : numHandlers_( 0 )
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
      ++numHandlers_ ;
   }
}

void pollHandlerSet_t :: remove( pollHandler_t &handler )
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
   } // make sure it's not already in the list
}

//
// returns true if at least one handler wants notification
//
bool pollHandlerSet_t :: poll( int ms )
{
/*
printf( "polling %u handles\n", numHandlers_ );
for( unsigned h = 0 ; h < numHandlers_ ; h++ )
{
   printf( "[%u].fd_ = %d, .mask_ = %x\n", h, fds_[h].fd, fds_[h].events );
}
*/
   int const numReady = ::poll( fds_, numHandlers_, ms );
   if( 0 < numReady )
   {
      for( unsigned i = 0 ; i < numHandlers_ ; i++ )
      {
         pollfd &fd = fds_[i];
         short const signals = fd.events & fd.revents ;
// printf( "[%u].fd_ = %d, .mask_ = %x, .events = %x, signals = %x\n", i, fds_[i].fd, fds_[i].events, fds_[i].revents, signals );
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
   }

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
   
   virtual void onDataAvail( void ){ printf( "POLLIN\n" ); setMask( getMask() & ~POLLIN ); }    // POLLIN
   virtual void onWriteSpace( void ){ printf( "POLLOUT\n" ); setMask( getMask() & ~POLLOUT ); } // POLLOUT
   virtual void onError( void ){ printf( "POLLERR\n" ); setMask( getMask() & ~POLLERR ); }      // POLLERR
   virtual void onHUP( void ){ printf( "POLLHUP\n" ); setMask( getMask() & ~POLLHUP ); }        // POLLHUP
   
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
            printf( "opened %s: fd %d\n", argv[arg], fd );
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
            printf( "poll %d\n", ++iterations );
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
