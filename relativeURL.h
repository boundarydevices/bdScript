#ifndef __RELATIVEURL_H__
#define __RELATIVEURL_H__ "$Id: relativeURL.h,v 1.1 2002-10-25 02:55:01 ericn Exp $"

/*
 * relativeURL.h
 *
 * This header file declares a set of routines
 * which are used to keep track of a stack of
 * urls for use in processing relative URLs per 
 * the HTML spec.
 *
 *
 * Change History : 
 *
 * $Log: relativeURL.h,v $
 * Revision 1.1  2002-10-25 02:55:01  ericn
 * -initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <string>

//
// returns true and the url if established
//
bool currentURL( std::string &url );

//
// returns true and the absolute URL if an absolute URL 
// has been established or is present in relative
//
bool absoluteURL( std::string const &relative,   // input : arbitrary URL
                  std::string       &absolute ); // output : absolute URL

//
// returns true if the specified URL is relative (missing protocol and server)
//
bool isRelativeURL( std::string const &url );

//
// activates a new url (relative or absolute)
//
void pushURL( std::string const &url );

//
// deactivates the previous url
//
void popURL( void );


#endif

