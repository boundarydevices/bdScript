#ifndef __URLFILE_H__
#define __URLFILE_H__ "$Id: urlFile.h,v 1.4 2004-06-19 18:15:06 ericn Exp $"

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
 * Revision 1.4  2004-06-19 18:15:06  ericn
 * -added failure flag
 *
 * Revision 1.3  2002/11/30 17:32:14  ericn
 * -modified to allow synchronous callbacks
 *
 * Revision 1.2  2002/11/30 00:30:12  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.1.1.1  2002/09/28 16:50:46  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <string>

class urlFile_t {
public:
   urlFile_t( char const url[] );
   ~urlFile_t( void );

   inline bool          isOpen( void ) const { return 0 != data_ ; }
   inline unsigned long getSize( void ) const { return size_ ; }
   inline void const   *getData( void ) const { return data_ ; }

   std::string const url_ ;
   void const       *data_ ;
   unsigned long     size_ ;
   unsigned long     handle_ ;
   unsigned long     callingThread_ ;
   bool              failed_ ;
};


#endif

