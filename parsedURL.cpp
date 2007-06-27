/*
 * Module parsedURL.cpp
 *
 * This module defines the methods of the parsedURL_t 
 * class as declared in parsedURL.h
 *
 *
 * Change History : 
 *
 * $Log: parsedURL.cpp,v $
 * Revision 1.6  2007-06-27 02:29:44  ericn
 * -put colon between host name and port
 *
 * Revision 1.5  2006/02/13 21:07:52  ericn
 * -add constants for http and https
 *
 * Revision 1.4  2005/11/06 00:49:49  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.3  2003/01/31 13:26:44  ericn
 * -added startsAtRoot flag and test program
 *
 * Revision 1.2  2002/11/30 18:00:30  ericn
 * -modified to include host always
 *
 * Revision 1.1  2002/11/29 18:38:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "parsedURL.h"
#include "ultoa.h"
#include <stdio.h>

std::string const protocolHTTP_( "http" );
std::string const protocolHTTPS_( "https" );

void printURL( parsedURL_t const &url )
{
   printf( "protocol : %s\n", url.getProtocol().c_str() );
   printf( "host     : %s\n", url.getHost().c_str() );
   printf( "port     : %d\n", url.getPort() );
   printf( "path     : /" );
   stringVector_t const &path = url.getPathParts();
   for( unsigned i = 0 ; i < path.size(); i++ )
      printf( "%s/", path[i].c_str() );
   printf( "\n" );

   printf( "file     : %s\n", url.getFile().c_str() );
   printf( "relative ? %s\n", url.isRelative() ? "yes" : "no" );
   printf( "fromRoot ? %s\n", url.startsAtRoot() ? "yes" : "no" );
}

parsedURL_t :: parsedURL_t( void )
   : protocol_( "" ),
     host_( "" ),
     port_( 0 ),
     file_( "" ),
     startsAtRoot_( false )
{
}

parsedURL_t :: parsedURL_t( std::string const &url )
   : protocol_( "" ),
     host_( "" ),
     port_( 0 ),
     file_( "" ),
     startsAtRoot_( false )
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
               startsAtRoot_ = ( 0 == protocol_.size() );
               if( !startsAtRoot_ )
               {
                  pathParts_.push_back( protocol_ );
                  protocol_ = "" ;
               }
               
               pathParts_.push_back( "" );

               state = statePath_ ;
            } // path name, no protocol or host
            else if( '.' == c )
            {
               protocol_ += c ;

               pathParts_.push_back( protocol_ );
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
               pathParts_.push_back( "" );
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
               pathParts_.push_back( "" );
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
               pathParts_.push_back( "" );
            }
            else
               pathParts_.back() += c ;

            break;
         }
         case stateError_:
            i = url.size();
            break;
      }
   }

   if( ( stateError_ != state ) && ( 0 < pathParts_.size() ) )
   {
      file_ = pathParts_.back();
      pathParts_.pop_back();
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

         unsigned i = 0 ;
         if( !startsAtRoot_ )
         {
            stringVector_t resolvedPath( parent.getPathParts() );
   
            for( ; ( i < pathParts_.size() ) && ( 0 < resolvedPath.size() ); i++ )
            {
               std::string const &dir( pathParts_[i] );
               if( 0 == strcmp( "..", dir.c_str() ) )
               {
                  if( 0 < resolvedPath.size() )
                     resolvedPath.pop_back();
               }
               else
                  break;
            }

            for( ; i < pathParts_.size(); i++ )
            {
               resolvedPath.push_back( pathParts_[i] );
            }
            
            pathParts_ = resolvedPath ;
         
         } // use parent's path
      }
      else
      {
      }
   }
}

std::string parsedURL_t :: getPath( void ) const
{
   std::string rval( "/" );
   for( unsigned i = 0 ; i < pathParts_.size(); i++ )
   {
      rval += pathParts_[i];
      rval += '/' ;
   }
   rval += file_ ;
   return rval ;
}

void parsedURL_t :: getAbsolute( std::string &absolute ) const
{
   if( protocol_.size() )
   {
      absolute = protocol_ ;
      absolute += "://" ;
      absolute += host_ ;
      if( 0 != port_ ){
         absolute += ':' ;
         absolute += ultoa_t( port_, 0 ).getValue();
      }

      absolute += '/' ;

      for( unsigned i = 0 ; i < pathParts_.size() ; i++ )
      {
         absolute += pathParts_[i];
         absolute += '/' ;
      }
      
      absolute += file_ ;
   } // have protocol
   else
      absolute = "none:" ;
}


#ifdef STANDALONE
#include <stdio.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      parsedURL_t first( argv[1] );
      printURL( first );
      for( int arg = 2 ; arg < argc ; arg++ )
      {
         parsedURL_t rel( argv[arg] );
         printf( "---> %s before fixup\n", argv[arg] );
         printURL( rel );
         rel.fixup( first );
         printf( "---> %s after fixup\n", argv[arg] );
         printURL( rel );
	 std::string absolute ;
         rel.getAbsolute(absolute);

	 printf( "absolute: %s\n", absolute.c_str() );
      }
   }
   else
      fprintf( stderr, "Usage : parsedURL url\n" );

   return 0 ;
}
#endif
