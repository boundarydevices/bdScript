/*
 * Module openFds.cpp
 *
 * This module defines the methods of the openFds_t
 * class as declared in openFds.h
 *
 *
 * Change History : 
 *
 * $Log: openFds.cpp,v $
 * Revision 1.1  2003-08-24 15:46:48  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "openFds.h"
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

openFds_t :: openFds_t( void )
   : count_( 0 ),
     fds_( 0 )
{
   char fdDir[256];

   sprintf( fdDir, "/proc/%d/fd", getpid() );
   
   DIR *dir = opendir( fdDir );
   if( dir )
   {
      dirent  *dirEntry ;
      unsigned tempCount = 0 ;
      while( 0 != ( dirEntry = readdir( dir ) ) )
      {
         if( isdigit( dirEntry->d_name[0] ) )
            ++tempCount ;
      }

      if( 0 < tempCount )
      {
         fds_ = new int [ tempCount ];
         rewinddir( dir );
         while( ( 0 != ( dirEntry = readdir( dir ) ) )
                &&
                ( count_ < tempCount ) )
         {
            if( isdigit( dirEntry->d_name[0] ) )
            {
               int match = sscanf( dirEntry->d_name, "%u", &fds_[count_] );
               if( 1 == match )
               {
                  ++count_ ;
               }
               else
                  fprintf( stderr, "Invalid fdName:%s\n", dirEntry->d_name );
            }
         }
      }
      closedir( dir );
   }
}

openFds_t :: ~openFds_t( void )
{
   if( fds_ )
      delete [] fds_ ;
}


#ifdef MODULETEST

int main( void )
{
   openFds_t openFds ;
   printf( "%u fds open\n", openFds.count() );

   for( unsigned i = 0 ; i < openFds.count(); i++ )
      printf( "[%u] == %d\n", i, openFds[i] );

   return 0 ;
}
#endif
