/*
 * Module jsURL.cpp
 *
 * This module defines the initialization routine
 * for Javascript URL routines and the C++ versions
 * of them as declared in jsURL.h
 *
 *
 * Change History : 
 *
 * $Log: jsURL.cpp,v $
 * Revision 1.2  2002-10-24 13:16:01  ericn
 * -added pushURL() and popURL()
 *
 * Revision 1.1  2002/10/20 16:31:23  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsURL.h"
#include <vector>
#include "../boundary1/ultoa.h"
#include <ctype.h>

typedef std::vector<std::string> stringVector_t ;

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
   stringVector_t const &getPath( void ) const { return path_ ; }
   std::string    const &getFile( void ) const { return file_ ; }

   inline bool isRelative( void ) const { return 0 == protocol_.size(); }
   
   void fixup( parsedURL_t const &parent );
   void getAbsolute( std::string & ) const ;

private:
   std::string    protocol_ ;
   std::string    host_ ;
   unsigned short port_ ;
   stringVector_t path_ ;
   std::string    file_ ;
};

parsedURL_t :: parsedURL_t( void )
   : protocol_( "" ),
     host_( "" ),
     port_( 0 ),
     file_( "" )
{
}

parsedURL_t :: parsedURL_t( std::string const &url )
   : protocol_( "" ),
     host_( "" ),
     port_( 0 ),
     file_( "" )
{
   enum {
      stateProto_,
      stateSlash1_,
      stateSlash2_,
      stateHost_,
      statePort_,
      statePath_,
      stateError_
   } state = stateProto_ ;
   
   for( unsigned i = 0 ; ( i < url.size() ) && ( stateError_ > state ) ; i++ )
   {
      char const c = url[i];
      switch( state )
      {
         case stateProto_ :
         {
            if( ':' == c )
            {
               state = stateSlash1_ ;
            } // done with protocol
            else if( '/' == c )
            {
               path_.push_back( protocol_ );
               if( 0 < protocol_.size() )
                  path_.push_back( "" );
               protocol_ = "" ;
               state = statePath_ ;
            } // path name, no protocol or host
            else if( '.' == c )
            {
               protocol_ += c ;

               path_.push_back( protocol_ );
               protocol_ = "" ;

               state = statePath_ ;
            } // part of path name.. no protocol, host, or port
            else
               protocol_ += c ;
            break;
         }
         case stateSlash1_ :
         {
            if( '/' == c )
               state = stateSlash2_ ;
            else
               state = stateError_ ;
            break;
         }
         case stateSlash2_ :
         {
            if( '/' == c )
               state = stateHost_ ;
            else
               state = stateError_ ;
            break;
         }
         case stateHost_ :
         {
            if( '/' == c )
            {
               path_.push_back( "" );
               state = statePath_ ;
            }
            else if( ':' == c )
            {
               state = statePort_ ;
            }
            else
               host_ += c ;
            break;
         }
         case statePort_ :
         {
            if( isdigit( c ) )
            {
               port_ *= 10 ;
               port_ += ( c - '0' );
            }
            else if( '/' == c )
            {
               path_.push_back( "" );
               state = statePath_ ;
            }
            else
               state = stateError_ ;
            break;
         }
         case statePath_ :
         {
            if( '/' == c )
            {
               path_.push_back( "" );
            }
            else
               path_.back() += c ;

            break;
         }
      }
   }
   
   if( ( stateError_ != state ) && ( 0 < path_.size() ) )
   {
      file_ = path_.back();
      path_.pop_back();
   }
}

void parsedURL_t :: fixup( parsedURL_t const &parent )
{
   if( 0 == protocol_.size() )
   {
      protocol_ = parent.getProtocol();
      if( 0 == host_.size() )
      {
         host_ = parent.getHost();

printf( "resolving path, parent size %u\n", parent.getPath().size() );

         stringVector_t resolvedPath( parent.getPath() );

         unsigned i ;
         for( i = 0 ; ( i < path_.size() ) && ( 0 < resolvedPath.size() ); i++ )
         {
            std::string const &dir( path_[i] );
            if( 0 == strcmp( "..", dir.c_str() ) )
            {
               if( 0 < resolvedPath.size() )
                  resolvedPath.pop_back();
            }
            else
               break;
         }

         for( ; i < path_.size(); i++ )
         {
            resolvedPath.push_back( path_[i] );
         }
         
         path_ = resolvedPath ;
      }
   }
}

void parsedURL_t :: getAbsolute( std::string &absolute ) const
{
   if( protocol_.size() )
   {
      absolute = protocol_ ;
      absolute += "://" ;
      if( host_.size() )
      {
         absolute += host_ ;
         if( 0 != port_ )
            absolute += ultoa_t( port_, 0 ).getValue();

         absolute += '/' ;

         for( unsigned i = 0 ; i < path_.size() ; i++ )
         {
            absolute += path_[i];
            absolute += '/' ;
         }
         
         absolute += file_ ;
      
      } // have host
   } // have protocol
   else
      absolute = "none:" ;
}

static void printURL( parsedURL_t const &url )
{
   printf( "protocol : %s\n", url.getProtocol().c_str() );
   printf( "host     : %s\n", url.getHost().c_str() );
   printf( "port     : %d\n", url.getPort() );
   printf( "path     : " );
   stringVector_t const &path = url.getPath();
   for( unsigned i = 0 ; i < path.size(); i++ )
      printf( "%s/", path[i].c_str() );
   printf( "\n" );

   printf( "file     : %s\n", url.getFile().c_str() );
}

typedef std::vector<parsedURL_t> urlStack_t ;

static urlStack_t urlStack_ ;

//
// returns true and the url if established or if 
// the input url is absolute
//
bool currentURL( std::string &url )
{
   parsedURL_t parsed( url );

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
//   printURL( parsed );
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

static JSBool
jsCurrentURL( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   JSString *jsCurrent = 0 ;

   std::string current ;
   if( currentURL( current ) )
   {
      jsCurrent = JS_NewStringCopyN( cx, current.c_str(), current.size() );
   }
   else
      jsCurrent = JS_NewStringCopyZ( cx, "unknown" );

   *rval = STRING_TO_JSVAL( jsCurrent );
   return JS_TRUE ;
}

static JSBool
jsAbsoluteURL( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && ( JSVAL_IS_STRING( argv[0] ) ) )
   {
      JSString *urlString = JS_ValueToString( cx, argv[0] );
      std::string url( JS_GetStringBytes( urlString ), JS_GetStringLength( urlString ) );
      std::string absolute ;
      if( absoluteURL( url, absolute ) )
         *rval = STRING_TO_JSVAL( JS_NewStringCopyN( cx, absolute.c_str(), absolute.size() ) );
      else
         *rval = JSVAL_FALSE ;
   }
   else
      *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSBool
jsIsRelativeURL( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && ( JSVAL_IS_STRING( argv[0] ) ) )
   {
      JSString *urlString = JS_ValueToString( cx, argv[0] );
      std::string url( JS_GetStringBytes( urlString ), JS_GetStringLength( urlString ) );
      if( isRelativeURL( url ) )
         *rval = JSVAL_TRUE ;
      else
         *rval = JSVAL_FALSE ;
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSFunctionSpec image_functions[] = {
    {"currentURL",           jsCurrentURL,       1 },
    {"absoluteURL",          jsAbsoluteURL,      1 },
    {"isRelativeURL",        jsIsRelativeURL,    1 },
    {0}
};

bool initJSURL( JSContext *cx, JSObject *glob )
{
   if( JS_DefineFunctions( cx, glob, image_functions) )
   {
      urlStack_.push_back( parsedURL_t( "http://linuxbox/images/test.html" ) );
      return true ;
   }
   else
      return false ;
}

