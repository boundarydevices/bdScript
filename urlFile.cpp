/*
 * Module urlFile.cpp
 *
 * This module defines the methods of the urlFile_t 
 * class as declared in urlFile.h
 *
 *
 * Change History : 
 *
 * $Log: urlFile.cpp,v $
 * Revision 1.5  2002-12-02 15:02:03  ericn
 * -added param to callback, name for mutex
 *
 * Revision 1.4  2002/11/30 17:32:17  ericn
 * -modified to allow synchronous callbacks
 *
 * Revision 1.3  2002/11/30 05:29:45  ericn
 * -removed debug msgs
 *
 * Revision 1.2  2002/11/30 00:30:15  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.1.1.1  2002/09/28 16:50:46  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "urlFile.h"
#include "ccActiveURL.h"
#include <string.h>

static mutex_t     mutex_( "urlFileMutex_" );
static condition_t cond_ ;

static void onComplete( void         *opaque,
                        void const   *data,
                        unsigned long numRead,
                        unsigned long handle )
{
   urlFile_t *f = (urlFile_t *)opaque ;
   f->data_   = data ;
   f->size_   = numRead ;
   f->handle_ = handle ;
   if( f->callingThread_ != pthread_self() )
   {
      mutexLock_t lock( mutex_ );
      if( !cond_.signal() )
         fprintf( stderr, "Error %m signalling completion\n" );
   }
}

static void onFailure( void              *opaque,
                       std::string const &errorMsg )
{
   printf( "failed:%s\n", errorMsg.c_str() );
   urlFile_t *f = (urlFile_t *)opaque ;
   if( f->callingThread_ != pthread_self() )
   {
      mutexLock_t lock( mutex_ );
      if( !cond_.signal() )
         fprintf( stderr, "Error %m signalling completion\n" );
   }
   else if( !cond_.signal() )
      fprintf( stderr, "Error %m signalling completion\n" );
}

static void onCancel( void *opaque )
{
   printf( "cancelled\n" );
   urlFile_t *f = (urlFile_t *)opaque ;
   if( f->callingThread_ != pthread_self() )
   {
      mutexLock_t lock( mutex_ );
      if( !cond_.signal() )
         fprintf( stderr, "Error %m signalling completion\n" );
   }
   else if( !cond_.signal() )
      fprintf( stderr, "Error %m signalling completion\n" );
}

static void onSize( void         *opaque,
                    unsigned long size )
{
}

static void onProgress( void         *opaque,
                        unsigned long totalReadSoFar )
{
}

static curlCallbacks_t const callbacks_ = {
   onComplete,
   onFailure,
   onCancel,
   onSize,
   onProgress
};

urlFile_t :: urlFile_t( char const url[] )
   : url_( url ),
     size_( 0 ),
     data_( 0 ),
     handle_( 0xdeadbeef ),
     callingThread_( pthread_self() )
{
   getCurlCache().get( url, this, callbacks_ );
   mutexLock_t lock( mutex_ );
   if( 0 == data_ )
      cond_.wait( lock );
}

urlFile_t :: ~urlFile_t( void )
{
   if( isOpen() )
      getCurlCache().closeHandle( handle_ );
}


#ifdef STANDALONE
#include <stdio.h>
#include "zlib.h"

int main( int argc, char const * const argv[] )
{
   if( 1 < argc ) 
   {
      getCurlCache();
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         urlFile_t f( argv[arg] );
         if( f.isOpen() )
         {
            unsigned a = adler32( 0, (unsigned char *)f.getData(), f.getSize() );
            printf( "%08x - %s\n", a, argv[arg] );
         }
         else
            fprintf( stderr, "Error %m opening %s\n", argv[arg] );
      }
   }
   else
      fprintf( stderr, "Usage : %s url [url...]\n", argv[0] );

   return 0 ;
}

#endif
