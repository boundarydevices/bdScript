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
 * Revision 1.1  2002-11-29 18:38:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "parsedURL.h"
#include "ultoa.h"
#include <stdio.h>

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
}

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
               pathParts_.push_back( protocol_ );
               if( 0 < protocol_.size() )
                  pathParts_.push_back( "" );
               protocol_ = "" ;
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

         stringVector_t resolvedPath( parent.getPathParts() );

         unsigned i ;
         for( i = 0 ; ( i < pathParts_.size() ) && ( 0 < resolvedPath.size() ); i++ )
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
      if( host_.size() )
      {
         absolute += host_ ;
         if( 0 != port_ )
            absolute += ultoa_t( port_, 0 ).getValue();

         absolute += '/' ;

         for( unsigned i = 0 ; i < pathParts_.size() ; i++ )
         {
            absolute += pathParts_[i];
            absolute += '/' ;
         }
         
         absolute += file_ ;
      
      } // have host
   } // have protocol
   else
      absolute = "none:" ;
}


