#ifndef __MEMFILE_H__
#define __MEMFILE_H__ "$Id: memFile.h,v 1.1 2002-10-07 04:38:17 ericn Exp $"

/*
 * memFile.h
 *
 * This header file declares the memFile_t class, which is
 * used to mmap a file into memory so that its' content
 * can be dealt with in one piece.
 *
 *
 * Change History : 
 *
 * $Log: memFile.h,v $
 * Revision 1.1  2002-10-07 04:38:17  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

class memFile_t {
public:
   memFile_t( char const *path );
   ~memFile_t( void );

   bool worked( void ) const { return 0 <= fd_ ; }
   
   // call if !worked
   char const *getError( void ) const ;

   void const *getData( void ) const { return data_ ; }
   unsigned long getLength( void ) const { return length_ ; }

private:
   int           fd_ ;
   void const   *data_ ;
   unsigned long length_ ;
   int           errno_ ;
};


#endif

