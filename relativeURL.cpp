/*
 * Module relativeURL.cpp
 *
 * This module defines the URL stack routines
 * as declared in relativeURL.h
 *
 *
 * Change History : 
 *
 * $Log: relativeURL.cpp,v $
 * Revision 1.5  2003-03-22 15:30:54  ericn
 * -added printURLStack(), fixed currentURL
 *
 * Revision 1.4  2002/11/30 05:28:37  ericn
 * -removed debug msg
 *
 * Revision 1.3  2002/11/29 18:37:28  ericn
 * -moved URL parsing to parsedURL
 *
 * Revision 1.2  2002/11/11 04:29:00  ericn
 * -moved headers
 *
 * Revision 1.1  2002/10/25 02:55:01  ericn
 * -initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "relativeURL.h"
#include "parsedURL.h"
#include <stdio.h>
#include <ctype.h>

typedef std::vector<parsedURL_t> urlStack_t ;

static urlStack_t urlStack_ ;

//
// returns true and the url if established or if 
// the input url is absolute
//
bool currentURL( std::string &url )
{
   parsedURL_t parsed( urlStack_.back() );

   if( ( 0 < urlStack_.size() ) || !parsed.isRelative() )
   {
      if( parsed.isRelative() )
         parsed.fixup( urlStack_.back() );
      parsed.getAbsolute( url );
      return true ;
   }
   else
   {
      url = "" ;
      return false ;
   }
}


//
// returns true and the absolute URL if an absolute URL 
// has been established
//
bool absoluteURL( std::string const &relative,   //
                  std::string       &absolute )  // output : absolute URL
{
   parsedURL_t parsed( relative );
   if( 0 < urlStack_.size() )
      parsed.fixup( urlStack_.back() );
   if( !parsed.isRelative() )
   {
      parsed.getAbsolute( absolute );
      return true ;
   }
   else
      return false ;
}


//
// returns true if the specified URL is relative (missing protocol and server)
//
bool isRelativeURL( std::string const &url )
{
   parsedURL_t parsed( url );
   return parsed.isRelative();
}

//
// activates a new url (relative or absolute)
//
void pushURL( std::string const &url )
{
   parsedURL_t parsed( url );
   if( 0 < urlStack_.size() )
      parsed.fixup( urlStack_.back() );
   urlStack_.push_back( parsed );
}

//
// deactivates the previous url
//
void popURL( void )
{
   if( 0 < urlStack_.size() )
      urlStack_.pop_back();
}

void printURLStack( void )
{
   for( unsigned i = 0 ; i < urlStack_.size(); i++ )
   {
      printf( "---> url[%u]\n", i );
      printURL( urlStack_[i] );
   }
}
