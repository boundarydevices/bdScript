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
 * Revision 1.6  2003-08-24 22:06:52  ericn
 * -no linger, reuse address, signal handler\n
 *
 * Revision 1.5  2003/07/04 04:26:18  ericn
 * -no, really. Host IP, then port
 *
 * Revision 1.4  2003/07/04 02:42:04  ericn
 * -added support for hostIP (for compatibility)
 *
 * Revision 1.3  2003/03/19 21:59:07  ericn
 * -removed hardcoded port
 *
 * Revision 1.2  2003/02/10 01:17:38  ericn
 * -modified to support retrieval
 *
 * Revision 1.1  2003/02/09 02:25:07  ericn
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
#include <fcntl.h>
#include <signal.h>

static unsigned const maxRxSize = 0x200 ;

static char const usage[] = {
   "Usage: enter|exit|retrieve\\tidentifier\\tsymbology\\tbarcode\n"
};

static char const enterMsgs[] = {
   "\tentry messages\tgo here"
};

static char const exitMsgs[] = {
   "\texit messages\tgo here"
};

static char const reportMsgs[] = {
   "\thistory report\t\tmessages go here\t\tnormally history of barcode"
};

static char const approved[] = {
   "\tapproved\trGF"
};

static char const denied[] = {
   "\tdenied\tRgf"
};

static char const report[] = {
   "\treport\t   "
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
      char const *cmd = parts[0].c_str();
      char const *approval = 0 ;
      char const *msgs = 0 ;
      if( 0 == strcmp( "enter", cmd ) )
      {
         approval = approved ;
         msgs = enterMsgs ;
      }
      else if( 0 == strcmp( "exit", cmd ) )
      {
         approval = approved ;
         msgs = exitMsgs ;
      }
      else if( 0 == strcmp( "retrieve", cmd ) )
      {
         approval = report ;
         msgs = reportMsgs ;
      }
      else
         fprintf( stderr, "unknown barcode cmd %s\n", cmd );

      if( 0 != approval )
      {
         std::string responseString ;
         responseString = parts[1];
         responseString += approval ;
         responseString += msgs ;
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

int tcpFd = -1 ;

static void closeHandle( void )
{
   printf( "closing TCP socket\n" ); fflush( stdout );
   if( 0 <= tcpFd )
      close( tcpFd );
   printf( "done closing TCP socket\n" ); fflush( stdout );
}

static struct sigaction sa;

static void closeOnSignal( int sig )
{
   printf( "signal %d\n", sig );
   closeHandle();
}

int main( int argc, char const *const argv[] )
{
   if( 3 == argc )
   {
      atexit( closeHandle );
      tcpFd = socket( AF_INET, SOCK_STREAM, 0 );
      if( 0 <= tcpFd )
      {
         // Initialize the sa structure
         sa.sa_handler = closeOnSignal ;
         sigemptyset(&sa.sa_mask);
         sa.sa_flags = 0;

         // Set up the signal handler
         sigaction(SIGHUP, &sa, NULL);
         sigaction(SIGABRT, &sa, NULL);
         sigaction(SIGSEGV, &sa, NULL);
         sigaction(SIGKILL, &sa, NULL);

         int doit = 1 ;
         int result = setsockopt( tcpFd, SOL_SOCKET, SO_REUSEADDR, &doit, sizeof( doit ) );
         if( 0 != result )
            fprintf( stderr, "SO_REUSEADDR:%d:%m\n", result );
         
         struct linger linger; /* allow a lingering, graceful close; */ 
         linger.l_onoff  = 0 ; 
         linger.l_linger = 0 ; 
         result = setsockopt( tcpFd, SOL_SOCKET, SO_LINGER, &linger, sizeof( linger ) );
         if( 0 != result )
            fprintf( stderr, "SO_REUSEADDR:%d:%m\n", result );

         fcntl( tcpFd, F_SETFD, FD_CLOEXEC );
         sockaddr_in myAddress ;

         memset( &myAddress, 0, sizeof( myAddress ) );
   
         myAddress.sin_family      = AF_INET;
         myAddress.sin_addr.s_addr = 0 ; // local
         myAddress.sin_port        = htons( (unsigned short)strtoul( argv[2], 0, 0 ) );
   
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
                     doit = 1 ;
                     fcntl( newFd, F_SETFD, FD_CLOEXEC );
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
            fprintf( stderr, ":bind %s:%m\n", argv[2] );
      }
      else
         fprintf( stderr, ":socket:%m\n" );
   }
   else
      fprintf( stderr, "Usage : bc hostIP port#\n" );

   return 0 ;
}
