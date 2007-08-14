/*
 * Module aviHeader.cpp
 *
 * This module defines the readAviHeader() routine
 * as declared in aviHeader.h
 *
 *
 * Change History : 
 *
 * $Log: aviHeader.cpp,v $
 * Revision 1.1  2007-08-14 12:58:35  ericn
 * -import
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */


#include "aviHeader.h"
#include <stdio.h>
#include <unistd.h>
#include "riffRead.h"
#include <fcntl.h>

struct aviHdr_t {
   unsigned long dwMicroSecPerFrame;
   unsigned long dwMaxBytesPerSec;
   unsigned long dwReserved1;
   unsigned long dwFlags;
   unsigned long dwTotalFrames;
   unsigned long dwInitialFrames;
   unsigned long dwStreams;
   unsigned long dwSuggestedBufferSize;
   unsigned long dwWidth;
   unsigned long dwHeight;
   unsigned long dwReserved[4];
};

bool readAviHeader( char const    *fileName,
                    unsigned long &numFrames,
                    unsigned long &numStreams,
                    unsigned long &width,
                    unsigned long &height )
{
   int const fd = open( fileName, O_RDONLY );
   if( 0 <= fd ){
      riffRead_t rr(fileName);
      if( rr.worked() ){
         while( rr.nextChunk() ){
            if( ( RIFFLONG("avih") == rr.chunkId() )
                &&
                ( sizeof(struct aviHdr_t) >= rr.chunkSize() ) ){
               struct aviHdr_t header ;
               int numRead = read(rr.getFd(), &header, sizeof(header));
               if( sizeof(header) == numRead ){
                  numFrames = header.dwTotalFrames ;
                  numStreams = header.dwStreams ;
                  width = header.dwWidth ;
                  height = header.dwHeight ;
                  close(fd);
                  return true ;
               }
            }
         }
      }
      close( fd );
   }
   else
      perror( fileName );
   return false ;
}

