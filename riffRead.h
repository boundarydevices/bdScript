#ifndef __RIFFREAD_H__
#define __RIFFREAD_H__ "$Id: riffRead.h,v 1.1 2007-08-14 12:58:41 ericn Exp $"

/*
 * riffRead.h
 *
 * This header file declares the riffRead_t class, which
 * provides a convenient API for traversing RIFF files.
 *
 * Basic usage for a linear scan is as follows:
 *
 *    riffRead_t fIn( "myFile.avi" );
 *    if( fIn.worked() ){
 *       while( fIn.nextChunk() ){
 *          printf( "chunk Id 0x%08lx (%s)\n", fIn.chunkId(), fIn.fourcc() );
 *       }
 *    }
 *
 * Change History : 
 *
 * $Log: riffRead.h,v $
 * Revision 1.1  2007-08-14 12:58:41  ericn
 * -import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#define RIFFLONG(__str) *((unsigned long *)__str)

class riffRead_t {
public:
   riffRead_t( char const *fileName );
   ~riffRead_t( void );

   bool worked( void ) const { return 0 <= fd_ ; }

   unsigned long riffType(void) const { return riffType_ ; }
   char const   *riffStr(void) const { return riffStr_ ; }

   bool nextChunk( void );
   unsigned long chunkId(void) const { return chunkId_ ; }
   char const *fourcc(void) const { return fourcc_ ; }
   unsigned long chunkSize(void) const { return size_ ; }

   int getFd(void) const { return fd_ ; }

private:
   int           fd_ ;
   unsigned long riffType_ ;
   char          riffStr_[8]; 

   unsigned long chunkId_ ;
   unsigned long size_ ;
   char          fourcc_[8];
};


#endif

