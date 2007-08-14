/*
 * Module riffRead.cpp
 *
 * This module defines the methods of the riffRead_t
 * class as declared in riffRead.h
 *
 * Change History : 
 *
 * $Log: riffRead.cpp,v $
 * Revision 1.1  2007-08-14 12:58:44  ericn
 * -import
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */


#include "riffRead.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

riffRead_t::riffRead_t( char const *fileName )
   : fd_( open( fileName, O_RDONLY ) )
   , riffType_(0xdeadbeef)
   , chunkId_(0xdeadbeef)
   , size_(0)
{
   memcpy(riffStr_, &riffType_, sizeof(riffType_));
   riffStr_[sizeof(riffType_)] = 0 ;
   memcpy(fourcc_, &chunkId_, sizeof(chunkId_));
   fourcc_[sizeof(chunkId_)] = 0 ;

   if( 0 <= fd_ ){
      unsigned long const headerSize = sizeof(chunkId_)+sizeof(size_);
      int numRead = read( fd_, &chunkId_, headerSize );
      if( (int)headerSize == numRead ){
         if(RIFFLONG("RIFF") == chunkId_){
            numRead = read( fd_, &riffType_, sizeof(riffType_));
            if( sizeof(riffType_) == numRead ){
               memcpy(riffStr_, &riffType_, sizeof(riffType_));
               size_ = 0 ; /* don't skip this one */
               return ;
            }
         }
      }
      close( fd_ );
      fd_ = -1 ;
   }
   else
      perror(fileName);
}

riffRead_t::~riffRead_t( void )
{
   if( 0 <= fd_ ){
      close( fd_ );
      fd_ = -1 ;
   }
}

bool riffRead_t::nextChunk( void )
{
   lseek(fd_, size_, SEEK_CUR);
   unsigned long const headerSize = sizeof(chunkId_)+sizeof(size_);
   int numRead = read(fd_, &chunkId_, headerSize );
   if( (int)headerSize == numRead ){
      if(RIFFLONG("LIST") == chunkId_){
         size_ = 0 ; // don't skip list content
         read(fd_,&chunkId_,sizeof(chunkId_));
         return nextChunk();
         chunkId_ = size_ ;
         numRead = read(fd_,&size_,sizeof(size_));
         if( sizeof(size_) == numRead ){
            memcpy(fourcc_,&chunkId_,sizeof(chunkId_));
            return true ;
         }
      }
      else {
         memcpy(fourcc_,&chunkId_,sizeof(chunkId_));
         return true ;
      }
   }
   return false ;
}


#ifdef STANDALONE

static bool dump = false ;
static void parseArgs( int &argc, char **argv )
{
   for( int i = 1 ; i < argc ; i++ ){
      char *arg = argv[i];
      if( '-' == *arg ){
         switch( arg[1] ){
            case 'd' : 
            case 'D' :
            {
               dump = true ;
               memcpy( argv+i, argv+i+1, (argc-i)*sizeof(*argv));
               i-- ;
               argc-- ;
            }
         }
      }
   }
}

#include "hexDump.h"

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

int main( int argc, char **argv )
{
   parseArgs(argc,argv);
   for( int i = 1 ; i < argc ; i++ ){
      char const *fileName = argv[i];
      riffRead_t rr(fileName);
      if( rr.worked() ){
         printf( "RIFF file type 0x%08lx(%s)\n", rr.riffType(), rr.riffStr() );
         while( rr.nextChunk() ){
            printf( "chunk 0x%lx: %s, size %u\n", rr.chunkId(), rr.fourcc(), rr.chunkSize() );
            if( dump ){
               long int oldPos = lseek(rr.getFd(),0,SEEK_CUR);
               unsigned long numLeft = rr.chunkSize();
               unsigned long addr = oldPos ;

               while(0 < numLeft){
                  unsigned char data[256];
                  unsigned count = (numLeft > sizeof(data)) ? sizeof(data) : numLeft ;
                  int numRead = read(rr.getFd(), data, count);
                  if( count == numRead ){
                     hexDumper_t hd(data,numRead, addr);
                     while( hd.nextLine() )
                        printf( "%s\n", hd.getLine() );
                     addr += numRead ;
                     numLeft -= numRead ;
                  }
                  else {
                     perror( fileName );
                     break ;
                  }
               }
               lseek(rr.getFd(),oldPos, SEEK_SET);
            }

            if( ( RIFFLONG("avih") == rr.chunkId() )
                &&
                ( sizeof(struct aviHdr_t) >= rr.chunkSize() ) ){
               long int oldPos = lseek(rr.getFd(),0,SEEK_CUR);
               struct aviHdr_t header ;
               int numRead = read(rr.getFd(), &header, sizeof(header));
               if( sizeof(header) == numRead ){
                  printf( "uSec/frame:\t%lu\n", header.dwMicroSecPerFrame );
                  printf( "maxBPS:\t\t%lu\n", header.dwMaxBytesPerSec );
                  printf( "flags:\t\t0x%x\n", header.dwFlags );
                  printf( "frames:\t\t%lu\n", header.dwTotalFrames );
                  printf( "iframes:\t%lu\n", header.dwInitialFrames );
                  printf( "streams:\t%lu\n", header.dwStreams );
                  printf( "width:\t\t%lu\n", header.dwWidth );
                  printf( "height:\t\t%lu\n", header.dwHeight );
               }
               lseek(rr.getFd(),oldPos, SEEK_SET);
            }
         }
      }
      else
         printf( "%s: Not a RIFF file\n", fileName );
   }
   return 0 ;
}

#endif
