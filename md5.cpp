/*
 * Module md5.cpp
 *
 * This module defines the getMD5() routine as
 * declared in md5.h
 *
 *
 * Change History : 
 *
 * $Log: md5.cpp,v $
 * Revision 1.3  2004-09-27 04:35:33  ericn
 * -allow md5 of file and cramfs
 *
 * Revision 1.2  2003/09/06 19:50:28  ericn
 * -added commentary
 *
 * Revision 1.1  2003/09/06 18:57:43  ericn
 * -initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "md5.h"
#include "openssl/md5.h"
#include <stdio.h>

void getMD5( void const   *data,
             unsigned long bytes,
             md5_t        &result )
{
   MD5_CTX ctx ;
   MD5_Init( &ctx );
   MD5_Update( &ctx, data, bytes );
   MD5_Final( result.md5bytes_, &ctx );
}

bool getMD5( char const *fileName,
             md5_t      &result )
{
   FILE *fIn = fopen( fileName, "rb" );
   if( fIn )
   {
      MD5_CTX ctx ;
      MD5_Init( &ctx );
      unsigned const blockSize = 4096 ;
      char *const inBuf = new char [blockSize];
      unsigned numRead ;
      while( 0 < ( numRead = fread( inBuf, 1, blockSize, fIn ) ) ) 
      {
         MD5_Update( &ctx, inBuf, numRead );
      }
      delete [] inBuf ;
      MD5_Final( result.md5bytes_, &ctx );

      fclose( fIn );
      return true ;
   }
   else
   {
      perror( fileName );
      return false ;
   }
}

struct cramfs_super {
	unsigned long magic;			/* 0x28cd3d45 - random number */
	unsigned long size;			/* length in bytes */
	unsigned long flags;			/* feature flags */
	unsigned long future;			/* reserved for future use */
	unsigned char signature[16];		/* "Compressed ROMFS" */
        
        // ... other stuff goes here
};

static unsigned long const CRAMFS_MAGIC = 0x28cd3d45 ;
static unsigned char const CRAMFS_SIG[] = {
   "Compressed ROMFS"
};

bool md5Cramfs( char const *fileName,
                md5_t      &result )
{
   FILE *fIn = fopen( fileName, "rb" );
   if( fIn )
   {
      bool worked = false ;

      cramfs_super super ;
      unsigned numRead = fread( &super, 1, sizeof( super ), fIn );
      if( sizeof( super ) == numRead )
      {
         if( ( CRAMFS_MAGIC == super.magic )
             &&
             ( 0 == memcmp( super.signature, CRAMFS_SIG, sizeof(super.signature)) ) )
         {
            fseek( fIn, 0, SEEK_SET );
            MD5_CTX ctx ;
            MD5_Init( &ctx );
            unsigned const blockSize = 4096 ;
            unsigned maxRead = blockSize ;
            char *const inBuf = new char [blockSize];
            unsigned totalRead = 0 ;

            while( ( totalRead < super.size ) 
                   &&
                   ( 0 < ( numRead = fread( inBuf, 1, maxRead, fIn ) ) ) ) 
            {
               MD5_Update( &ctx, inBuf, numRead );
               totalRead += numRead ;
               unsigned numLeft = super.size - totalRead ;
               if( numLeft < maxRead )
                  maxRead = numLeft ;
            }

            delete [] inBuf ;
            MD5_Final( result.md5bytes_, &ctx );

            worked = ( totalRead == super.size );
         }
         else
            fprintf( stderr, "%s bad magic : %lx\n", fileName, super.magic );
      }
      else
         fprintf( stderr, "short read : %u\n", numRead );

      fclose( fIn );
      return worked ;
   }
   else
   {
      perror( fileName );
      return false ;
   }
}

#ifdef STANDALONE
#include "memFile.h"

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         char const *fileName = argv[arg];
         if( '@' == fileName[0] )
         {
            fileName++ ;
            memFile_t fIn( fileName );
            if( fIn.worked() )
            {
               md5_t md5 ;
               getMD5( fIn.getData(), fIn.getLength(), md5 );
               for( unsigned dig = 0 ; dig < sizeof( md5.md5bytes_ ); dig++ )
                  printf( "%02x", md5.md5bytes_[ dig ] );
               printf( "\n" );
            }
            else
               perror( fileName );
         } // read entire file
         else if( '+' == fileName[0] )
         {
            fileName++ ;
            md5_t md5 ;
            if( md5Cramfs( fileName, md5 ) )
            {
               printf( "md5cramfs(%s) == ", fileName );
               for( unsigned dig = 0 ; dig < sizeof( md5.md5bytes_ ); dig++ )
                  printf( "%02x", md5.md5bytes_[ dig ] );
               printf( "\n" );
            }
            else
               perror( fileName );
         }
         else
         {
            md5_t md5 ;
            if( getMD5( fileName, md5 ) )
            {
               printf( "md5(%s) == ", fileName );
               for( unsigned dig = 0 ; dig < sizeof( md5.md5bytes_ ); dig++ )
                  printf( "%02x", md5.md5bytes_[ dig ] );
               printf( "\n" );
            }
            else
               perror( fileName );
         }
      }
   }
   else
      fprintf( stderr, "Usage: %s [@]fileName [[+]fileName...]\n"
                       "  '@' indicates read entire file before md5\n"
                       "  '+' indicates read cramfs image (ignoring pad)\n", argv[0] );

   return 0 ;
}

#endif
