#ifndef __URLFILE_H__
#define __URLFILE_H__ "$Id: urlFile.h,v 1.1 2002-09-28 16:50:46 ericn Exp $"

/*
 * urlFile.h
 *
 * This header file declares the urlFile_t class, which 
 * is used to open either a file over HTTP or a local file
 * depending on usage.
 *
 * If passed a local url:
 *       file://path
 * it will open and mmap() the file directly.
 *
 * If passed a remote url:
 *       http://server/path/fileName
 * it will try to retrieve the file from a web server.
 *
 * Change History : 
 *
 * $Log: urlFile.h,v $
 * Revision 1.1  2002-09-28 16:50:46  ericn
 * Initial revision
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

class urlFile_t {
public:
   urlFile_t( char const url[] );
   ~urlFile_t( void );

   inline bool          isOpen( void ) const { return 0 <= fd_ ; }
   inline unsigned long getSize( void ) const { return size_ ; }
   inline void const   *getData( void ) const { return data_ ; }

private:
   int           fd_ ;
   unsigned long size_ ;
   void const   *data_ ;
   unsigned long mappedSize_ ; // for curlFile_t's
   void const   *mapPtr_ ; // for curlFile_t's
};


#endif

