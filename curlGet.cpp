/*
 * Module curlGet.cpp
 *
 * This module defines the curlGet() routine,
 * as declared in curlGet.h
 *
 *
 * Change History : 
 *
 * $Log: curlGet.cpp,v $
 * Revision 1.2  2002-11-27 18:31:11  ericn
 * -removed use of curlCache module
 *
 * Revision 1.1  2002/10/06 16:52:32  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "curlGet.h"
#include "curlCache.h"
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <string>
#include <vector>
#include <math.h>

static unsigned long totalBytes = 0 ;

static size_t writeData( void *buffer, size_t size, size_t nmemb, void *userp )
{
   unsigned const total = size*nmemb;
   printf( "%p:%lu bytes\n", buffer, total );
   totalBytes += total ;
   std::string *pS = (std::string *)userp ;
   pS->append( (char *)buffer, total );
   return total ;
}

static int progress_callback
   ( void *clientp,
     double dltotal,
     double dlnow,
     double ultotal,
     double ulnow)
{
   printf( "dlt %lu\n"
           "dln %lu\n"
           "ult %lu\n"
           "uln %lu\n", 
           (unsigned long)floor( dltotal ), 
           (unsigned long)floor( dlnow ), 
           (unsigned long)floor( ultotal ), 
           (unsigned long)floor( ulnow ) );
   return 0 ;
}

bool curlGet( char const *url,
              char const *saveAs )
{
   bool worked = false ;


   CURL *cHandle = curl_easy_init();
   if( 0 != cHandle )
   {
      std::string data ;
      CURLcode result = curl_easy_setopt( cHandle, CURLOPT_URL, url );

      if( 0 == result )
      {
         result = curl_easy_setopt( cHandle, CURLOPT_WRITEFUNCTION, writeData );
         if( 0 == result )
         {
            result = curl_easy_setopt( cHandle, CURLOPT_WRITEDATA, &data );
            if( 0 == result )
            {
               result = curl_easy_setopt( cHandle, CURLOPT_FILETIME, (void *)1 );
               if( 0 == result )
               {
                  result = curl_easy_setopt( cHandle, CURLOPT_NOPROGRESS, 0 );
                  if( 0 == result )
                  {
                     result = curl_easy_setopt( cHandle, CURLOPT_PROGRESSFUNCTION, progress_callback );
                     if( 0 == result )
                     {
                        CURLcode result = curl_easy_perform( cHandle );
                        if( 0 == result )
                        {
                           worked = true ;
                        }
                     }
                  }
               }
            }
         }
      }
      curl_easy_cleanup( cHandle );

      if( ( 0 < data.size() ) && ( 0 != saveAs ) )
      {
         FILE *f = fopen( saveAs, "wb" );
         if( f )
         {
            fwrite( data.c_str(), data.size(), 1, f );
            fclose( f );
         }
         else
            perror( saveAs );
      }
   }
   else
      fprintf( stderr, "Error allocating curl handle\n" );
   
   return worked ;
}


#ifdef STANDALONE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int main( int argc, char const * const argv[] )
{
   int returnVal ;

   if( ( 2 == argc ) || ( 3 == argc ) )
   {
      char const *target = ( 3 == argc ) ? argv[2] : 0 ;
      if( curlGet( argv[1], target ) )
      {
         char path[256];
         printf( "%s -> %s\n", argv[1], ( 0 == target ) ? getcwd( path, sizeof( path )) : target );
         printf( "%lu bytes received\n", totalBytes );
         returnVal = 0 ;
      }
      else
      {
         fprintf( stderr, "Error getting %s\n", argv[1] );
         returnVal = ENOENT ;
      }
   }
   else
   {   
      fprintf( stderr, "Usage : curlGet url [saveFileName]\n" );
      returnVal = EINVAL ;
   }
   
   return returnVal ;
}

#endif
