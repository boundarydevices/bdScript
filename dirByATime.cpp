/*
 * Module dirByATime.cpp
 *
 * This module defines the methods of the dirByATime_t
 * class as declared in dirByATime.h
 *
 *
 * Change History : 
 *
 * $Log: dirByATime.cpp,v $
 * Revision 1.1  2002-09-28 16:50:46  ericn
 * Initial revision
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "dirByATime.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int compareATime( void const *p1, void const *p2 )
{
   dirByATime_t :: details_t const &d1 = *( dirByATime_t :: details_t const * )p1 ;
   dirByATime_t :: details_t const &d2 = *( dirByATime_t :: details_t const * )p2 ;
   int cmp = d1.stat_.st_atime - d2.stat_.st_atime ;
   if( 0 == cmp )
   {
      cmp = d1.stat_.st_size - d2.stat_.st_size ;
      if( 0 == cmp )
         cmp = strcmp( d1.dirent_.d_name, d2.dirent_.d_name );
   }
   return cmp ;
}

dirByATime_t :: dirByATime_t( char const *dirName )
   : dirLen_( strlen( dirName ) ),
     numEntries_( 0 ),
     totalSize_( 0 ),
     entries_( 0 )
{
   strcpy( dir_, dirName );
   if( '/' != dir_[dirLen_-1] )
      dir_[dirLen_++] = '/' ;
   dir_[dirLen_] = '\0' ;

   DIR *dh = opendir( dir_ );
   if( 0 != dh )
   {
      //
      // read once to find out how many
      //
      unsigned count = 0 ;
      struct dirent *entry ;
      while( 0 != ( entry = readdir( dh ) ) )
      {
         if( DT_REG == entry->d_type )
            count++ ;
      }

      unsigned const max = count + 32 ;
      entries_ = new details_t[ max ];
      rewinddir( dh );
      count = 0 ;
      while( ( 0 != ( entry = readdir( dh ) ) ) && ( count < max ) )
      {
         if( DT_REG == entry->d_type )
         {
            details_t dands ;
            dands.dirent_ = *entry ;
            sprintf( dands.dirent_.d_name, "%s%s", dir_, entry->d_name );
            if( 0 == stat( dands.dirent_.d_name, &dands.stat_ ) )
            {
               entries_[count++] = dands ;
               totalSize_ += dands.stat_.st_size ;
            } // have details
         } // regular file
      } // read again for details
      
      numEntries_ = count ;

      qsort( entries_, count, sizeof( entries_[0] ), compareATime );

      closedir( dh );
   }
}

dirByATime_t :: ~dirByATime_t( void )
{
   if( 0 != entries_ )
      delete [] entries_ ;
}

#ifdef STANDALONE

#include <stdio.h>
#include <time.h>

static void processDir( char const *dirName )
{
   dirByATime_t dir( dirName );
   printf( "directories in '%s'\n", dirName );
   for( unsigned i = 0 ; i < dir.numEntries(); i++ )
   {
      dirent const &d = dir.getDirent( i );
      struct stat const &st = dir.getStat( i );
      printf( "%5d - %-32s %10u %s", i, d.d_name, st.st_size, asctime( localtime( &st.st_atime ) ) );
   }
}

int main( int argc, char const * const argv[] )
{
   processDir( "./" );
   for( int arg = 1 ; arg < argc ; arg++ )
      processDir( argv[arg] );
   return 0 ;
}

#endif

