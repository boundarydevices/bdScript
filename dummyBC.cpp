/*
 * Program dummyBC.cpp
 *
 * This module defines a loopback barcode interface
 * for Linux. 
 *
 * It accepts barcode requests on a TCP socket, and responds
 * with dummy responses (generally approved).
 *
 *       enter<tab>identifier<tab>symbology<tab>barcode
 *       exit<tab>identifier<tab>symbology<tab>barcode
 *       retrieval<tab>identifier<tab>symbology<tab>barcode
 *
 * The identifier is passed to the Javascript side in matched
 * responses.
 *
 * Change History : 
 *
 * $Log: dummyBC.cpp,v $
 * Revision 1.1  2003-02-09 02:25:07  ericn
 * -added dummy barcode approval program
 *
 *
 * Copyright Boundary Devices, 2002
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <sys/time.h>

static unsigned short port = htons( 0xEFD1 );
static unsigned const maxRxSize = 0x200 ;

static char const usage[] = {
   "Usage: enter|exit|retrieve\\tidentifier\\tsymbology\\tbarcode\n"
};

static void processCmd( std::string const &cmdLine,
                        int                fd )
{
   std::string writeable( cmdLine );

   std::string parts[4];
   char *next = (char *)writeable.c_str();
   unsigned i ;
   for( i = 0 ; i < 4 ; i++ )
   {
      char *savePtr ;
      char const *tok = strtok_r( next, "\t", &savePtr );
      if( tok )
      {
         parts[i] = tok ;
         next = 0 ;
      }
      else
         break;
   }

   if( 4 == i )
   {
      bool validCmd = false ;
      
      char const *cmd = parts[0].c_str();
      if( 0 == strcmp( "enter", cmd ) )
      {
         validCmd = true ;
      }
      else if( 0 == strcmp( "exit", cmd ) )
      {
         validCmd = true ;
      }
      else if( 0 == strcmp( "retrieve", cmd ) )
      {
         validCmd = true ;
      }
      else
         fprintf( stderr, "unknown barcode cmd %s\n", cmd );

      if( validCmd )
      {
         std::string responseString ;
         responseString = parts[1];
         responseString += '\t' ;
         responseString += "approved" ; // approved ? "approved" : "denied" ;
         responseString += '\t' ;
         responseString += 'r' ; // ledRed ? 'R' : 'r' ;
         responseString += 'g' ; // ledGreen ? 'G' : 'g' ;
         responseString += 'f' ; // ledFlash ? 'F' : 'f' ;
         
         responseString += "\tmsg line1" ;
         responseString += "\tmsg line2" ;
         responseString += "\tmsg line3" ;
   
         responseString += '\r' ;
         write( fd, responseString.c_str(), responseString.size() );
      }
      else
         fprintf( stderr, usage );
   }
   else
      fprintf( stderr, usage );

}

static void unconfirmedAcknowledge( char const        *bc, 
                                    sockaddr_in const &from )
{
   printf( "unconfirmed ack %s\n", bc );
}

static void process( int fd )
{
   std::string curCmd ;

   while( 1 )
   {
      char inBuf[256];
      int numRead = read( fd, inBuf, sizeof( inBuf ) - 1 );
      if( 0 < numRead )
      {
         inBuf[numRead] = '\0' ;
         for( int i = 0 ; i < numRead ; i++ )
         {
            char const c = inBuf[i];
            if( ( '\r' == c ) || ( '\n' == c ) )
            {
               if( 0 < curCmd.size() )
               {
                  processCmd( curCmd, fd );
                  curCmd = "" ;
               }
            }
            else
               curCmd += c ;
         } // parse for complete lines
      }
      else
         break; // eof
   }
}

int main( int argc, char const *const argv[] )
{
   if( 2 == argc )
   {
      int tcpFd = socket( AF_INET, SOCK_STREAM, 0 );
      if( 0 <= tcpFd )
      {
         sockaddr_in myAddress ;
   
         memset( &myAddress, 0, sizeof( myAddress ) );
   
         myAddress.sin_family      = AF_INET;
         myAddress.sin_addr.s_addr = 0 ; // local
         myAddress.sin_port        = htons( (unsigned short)strtoul( argv[1], 0, 0 ) );
   
         if( 0 == bind( tcpFd, (struct sockaddr *) &myAddress, sizeof( myAddress ) ) )
         {
            while( true )
            {
               if( 0 == listen( tcpFd, 20 ) )
               {
                  struct sockaddr_in clientAddr ;
                  socklen_t sockAddrSize = sizeof( clientAddr );
   
                  int newFd = ::accept( tcpFd, (struct sockaddr *)&clientAddr, &sockAddrSize );
                  if( 0 <= newFd )
                  {
                     process( newFd );
                     close( newFd );
                  }
                  else
                  {
                     fprintf( stderr, "accept:%m\n" );
                     break;
                  }
               }
               else
               {
                  fprintf( stderr, "listen:%m\n" );
                  break;
               }
            }
         }
         else
            fprintf( stderr, ":bind 0x%x:%m\n", ntohs( port ) );
      }
      else
         fprintf( stderr, ":socket:%m\n" );
   }
   else
      fprintf( stderr, "Usage : bc port#\n" );

   return 0 ;
}
