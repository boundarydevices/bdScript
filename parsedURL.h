#ifndef __PARSEDURL_H__
#define __PARSEDURL_H__ "$Id: parsedURL.h,v 1.2 2003-01-31 13:26:28 ericn Exp $"

/*
 * parsedURL.h
 *
 * This header file declares the parsedURL_t class, for use
 * in those routines which need to extract pieces of a URL.
 *
 *
 * Change History : 
 *
 * $Log: parsedURL.h,v $
 * Revision 1.2  2003-01-31 13:26:28  ericn
 * -added startsAtRoot flag
 *
 * Revision 1.1  2002/11/29 18:38:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "stringVector.h"

class parsedURL_t {
public:
   parsedURL_t( void );
   parsedURL_t( std::string const &url );

   std::string const    &getProtocol( void ) const { return protocol_ ; }
   std::string const    &getHost( void ) const { return host_ ; }
   unsigned short        getPort( void ) const { return port_ ; }

   //
   // path is vector of directory names
   //
   stringVector_t const &getPathParts( void ) const { return pathParts_ ; }

   // full path, including file
   std::string           getPath( void ) const ;
   std::string    const &getFile( void ) const { return file_ ; }

   inline bool isRelative( void ) const { return 0 == protocol_.size(); }
   inline bool startsAtRoot( void ) const { return startsAtRoot_ ; }

   void fixup( parsedURL_t const &parent );
   void getAbsolute( std::string & ) const ;

private:
   std::string    protocol_ ;
   std::string    host_ ;
   unsigned short port_ ;
   stringVector_t pathParts_ ;
   std::string    file_ ;
   bool           startsAtRoot_ ;
};

void printURL( parsedURL_t const &url );

#endif

