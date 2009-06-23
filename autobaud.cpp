/*
 * rtsCTS.cpp
 *
 * This program will test the ability to get the state of the
 * CTS pin, toggle the RTS pin, and spew data over a serial port.
 * 
 */
 
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#define __u32 unsigned
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/poll.h>
#include <ctype.h>
#include "baudRate.h"
#include <stdlib.h>
#include <linux/serial.h>
#include "physMem.h"
#include <string.h>
#include "tickMs.h"
#include "hexDump.h"

static bool volatile doExit = false ;

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\r\n" );

   exit(1);
}

static void toggleRTS( int fdSerial )
{
   int line ;
   if( 0 == ioctl(fdSerial, TIOCMGET, &line) )
   {
      line ^= TIOCM_RTS ;
      if( 0 == ioctl(fdSerial, TIOCMSET, &line) )
      {
         printf( "toggled\r\n" );
      }
      else
         perror( "TIOCMGET" );
   }
   else
      perror( "TIOCMGET" );
}

static void setRaw( int fd, 
                    int baud, 
                    int databits, 
                    char parity, 
                    unsigned stopBits,
                    struct termios &oldState )
{
   tcgetattr(fd,&oldState);

   /* set raw mode for keyboard input */
   struct termios newState = oldState;
   newState.c_cc[VMIN] = 1;

   bool nonStandard = false ;
   unsigned baudConst ;
   if( baudRateToConst( baud, baudConst ) ){
      cfsetispeed(&newState, baudConst);
      cfsetospeed(&newState, baudConst);
   }
   else {
      cfsetispeed(&newState,B38400);
      cfsetospeed(&newState,B38400);
      fprintf( stderr, "Non-standard baud <%u>, old constant %u\n", baud, cfgetispeed(&newState) );
      bool worked = false ;
      struct serial_struct nuts;
      int rval = ioctl(fd, TIOCGSERIAL, &nuts);
      if( 0 == rval ){
         unsigned const divisor = nuts.baud_base / baud ;
         nuts.custom_divisor = divisor ;
         printf( "custom divisor %lu (baud base %u)\n", nuts.custom_divisor, nuts.baud_base );
         nuts.flags &= ~ASYNC_SPD_MASK;
         nuts.flags |= ASYNC_SPD_CUST;
         rval = ioctl(fd, TIOCSSERIAL, &nuts);
         if( 0 == rval ){
            printf( "baud changed\n" );
            rval = ioctl(fd, TIOCGSERIAL, &nuts);
            if( 0 == rval ){
               printf( "divisor is now %u\n", nuts.custom_divisor );
               worked = (nuts.custom_divisor == divisor);
               if( worked ){
                  physMem_t pm( 0x01c20000, 0x1000, O_RDWR );
                  unsigned long div0 ;
                  memcpy(&div0, (char const *)pm.ptr() + 0x820, sizeof(div0) );
                  unsigned long div1 ;
                  memcpy(&div1, (char const *)pm.ptr() + 0x824, sizeof(div1) );
                  printf( "divisors: 0x%08lx, 0x%08lx\n", div0, div1 );
                  nonStandard = true ;
               }
            }
            else
               perror( "TIOCGSERIAL2" );
         }
         else
            perror( "TIOCSSERIAL" );
      }
      else
         perror( "TIOCGSERIAL" );
   } // non-standard serial

   //
   // Note that this doesn't appear to work!
   // Reads always seem to be terminated at 16 chars!
   //
   newState.c_cc[VTIME] = 0; // 1/10th's of a second, see http://www.opengroup.org/onlinepubs/007908799/xbd/termios.html

   newState.c_cflag &= ~(PARENB|CSTOPB|CSIZE|CRTSCTS);              // Mask character size to 8 bits, no parity, Disable hardware flow control

   if( 'E' == parity )
   {
      newState.c_cflag |= PARENB ;
      newState.c_cflag &= ~PARODD ;
   }
   else if( 'O' == parity )
   {
      newState.c_cflag |= PARENB | PARODD ;
   }
   else if( 'S' == parity )
   {
      newState.c_cflag |= PARENB | IGNPAR | CMSPAR ;
      newState.c_cflag &= ~PARODD ;
   }
   else if( 'M' == parity )
   {
      newState.c_cflag |= PARENB | IGNPAR | CMSPAR | PARODD ;
   }
   else {
   } // no parity... already set

   newState.c_cflag |= (CLOCAL | CREAD |CS8);                       // Select 8 data bits
   if( 7 == databits ){
      newState.c_cflag &= ~CS8 ;
   }

   if( 1 != stopBits )
      newState.c_cflag |= CSTOPB ;

   newState.c_lflag &= ~(ICANON | ECHO );                           // set raw mode for input
   newState.c_iflag &= ~(IXON | IXOFF | IXANY|INLCR|ICRNL|IUCLC);   //no software flow control
   newState.c_oflag &= ~OPOST;                      //raw output
   tcsetattr( fd, TCSANOW, &newState );

   if( nonStandard ){
      physMem_t pm( 0x01c20000, 0x1000, O_RDWR );
      unsigned long div0 ;
      memcpy(&div0, (char const *)pm.ptr() + 0x820, sizeof(div0) );
      unsigned long div1 ;
      memcpy(&div1, (char const *)pm.ptr() + 0x824, sizeof(div1) );
      printf( "divisors now: 0x%08lx, 0x%08lx\n", div0, div1 );
   }
}

static void showStatus( int fd )
{
   int line ;
   if( 0 == ioctl(fd, TIOCMGET, &line) )
   {
      printf( "line status 0x%04x: ", line );
      if( line & TIOCM_CTS )
         printf( "CTS " );
      else
         printf( "~CTS " );
      if( line & TIOCM_RTS )
         printf( "RTS " );
      else
         printf( "~RTS " );
      printf( "\r\n" );
   }
   else
      perror( "TIOCMGET" );
}

static void waitModemChange( int fd )
{
   printf( "wait modem status..." ); fflush(stdout);
   int line = TIOCM_RNG | TIOCM_DSR | TIOCM_CTS | TIOCM_CD ;
   int result = ioctl(fd, TIOCMIWAIT, &line);
   printf( "\r\n" );
   if( 0 == result ){
      printf( "changed\r\n" );
      showStatus(fd);
   }
   else
      perror( "TIOCMWAIT" );
}

static char const spewOutput[] = {
   "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890\r\n"
};

static void showCounts(int fd)
{
	struct serial_icounter_struct  icount ;
	int rval = ioctl(fd,TIOCGICOUNT,&icount);
	if( 0 == rval ){
		printf( "TIOCGICOUNT worked\n" );
		printf( "CTS:	%d\n", icount.cts );
		printf( "DSR:	%d\n", icount.dsr );
		printf( "RNG:	%d\n", icount.rng );
		printf( "DCD:	%d\n", icount.dcd );
		printf( "rx:	%d\n", icount.rx );
		printf( "tx:	%d\n", icount.tx );
		printf( "FE:	%d\n", icount.frame );
		printf( "OVR:	%d\n", icount.overrun );
		printf( "PE:	%d\n", icount.parity );
		printf( "BRK:	%d\n", icount.brk );
		printf( "bufo:	%d\n", icount.buf_overrun );
	}
	else
		perror("TIOCGICOUNT");
}

#ifndef dim
#define dim(__arr)   (sizeof(__arr)/sizeof(__arr[0]))
#endif

static unsigned const baudRates[] = {
/*
   300
,  600
,  1200
,*/
   2400
,  4800
,  9600
,  19200
,  38400
,  57600
,  115200
};

static unsigned const numBaudRates = dim(baudRates);

static unsigned const charLengths[] = {
   7
,  8
};

static unsigned const numCharLengths = dim(charLengths);

static char const parityValues[] = {
   'N'
,  'E'
,  'O'
,  'M'
,  'S'
};

static unsigned const numParity = dim(parityValues);

struct results_t {
   unsigned baud ;
   unsigned charLen ;
   char     parity ;
   unsigned pe ;
   unsigned fe ;
   unsigned rx ;
   unsigned brk ;
   char     inData[256];
   unsigned inLen ;
};

int compareResults(const void *lhs, const void *rhs){
   struct results_t const &rl = *(struct results_t const *)lhs ;
   struct results_t const &rr = *(struct results_t const *)rhs ;
   int cmp ;

   cmp = rl.fe - rr.fe ;
   if( 0 < cmp )
      return 1 ;
   else if( 0 > cmp )
      return -1 ;
   
   cmp = rl.pe - rr.pe ;
   if( 0 < cmp )
      return 1 ;
   else if( 0 > cmp )
      return -1 ;

   cmp = rl.rx - rr.rx ;
   if( 0 < cmp )
      return 1 ;
   else if( 0 > cmp )
      return -1 ;
   
   cmp = rl.brk - rr.brk ;
   if( 0 < cmp )
      return 1 ;
   else if( 0 > cmp )
      return -1 ;
   
   cmp = rl.baud - rr.baud ;
   if( 0 < cmp )
      return 1 ;
   else if( 0 > cmp )
      return -1 ;
   
   cmp = rl.charLen - rr.charLen ;
   if( 0 < cmp )
      return 1 ;
   else if( 0 > cmp )
      return -1 ;

   cmp = rl.parity - rr.parity ;
   if( 0 < cmp )
      return 1 ;
   else
      return -1 ;
}

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      char const *const deviceName = argv[1];
      int const fdSerial = open( deviceName, O_RDWR );
      if( 0 <= fdSerial )
      {
         fcntl( fdSerial, F_SETFD, FD_CLOEXEC );
         fcntl( fdSerial, F_SETFL, O_NONBLOCK );
         
         printf( "device %s opened\n", deviceName );
         struct termios oldSerialState;

         struct termios oldStdinState;
         
         showStatus(fdSerial);
         
         signal( SIGINT, ctrlcHandler );
         pollfd fds[2]; 
         fds[0].fd = fdSerial ;
         fds[0].events = POLLIN | POLLERR ;
         fds[1].fd = fileno(stdin);
         fds[1].events = POLLIN | POLLERR ;

         struct termios oldState ;

         unsigned const numResults = numBaudRates*numParity*numCharLengths ;
         struct results_t *const results = new struct results_t [numResults];
         memset(results,0,sizeof(results[0])*numResults);
         unsigned ridx = 0 ;
         for( unsigned b = 0 ; b < numBaudRates ; b++ ){
            for( unsigned char p = 0 ; p < numParity ; p++ ){
               for( unsigned char clen = 0 ; clen < numCharLengths ; clen++ ){
                  printf( "%6u %u %c:", baudRates[b], charLengths[clen], parityValues[p] ); fflush(stdout);
                  setRaw(fdSerial, baudRates[b], charLengths[clen], parityValues[p], 1, oldState );
                  
                  struct serial_icounter_struct  starticount ;
                  int rval = ioctl(fdSerial,TIOCGICOUNT,&starticount);
                  if( 0 != rval ){
                     perror( "TIOCGICOUNT" );
                     return -1 ;
                  }
                  char inData[256];
                  unsigned inLen = 0 ;
                  unsigned long numRx = 0 ;
                  long long const startTick = tickMs();
                  do {
                     int const numReady = ::poll( fds, 2, 100 );
                     if( 0 < numReady )
                     {
                        for( unsigned i = 0 ; i < 2 ; i++ )
                        {
                           if( fds[i].revents & POLLIN )
                           {
                              char inBuf[80];
                              int numRead = read( fds[i].fd, inBuf, sizeof(inBuf) );
                              if( fdSerial == fds[i].fd ){
                                 numRx += numRead ;
                              }
                              if(numRead > inLen){
                                 memcpy(inData,inBuf,numRead);
                                 inLen = numRead ;
                              } // save longest read
                           }
                        }
                     }
                  } while( 2000 > (tickMs()-startTick) );
                  
                  struct serial_icounter_struct  endicount ;
                  rval = ioctl(fdSerial,TIOCGICOUNT,&endicount);
                  if( 0 != rval ){
                     perror( "TIOCGICOUNT" );
                     return -1 ;
                  }
                  struct results_t &r = results[ridx++];
                  r.baud    = baudRates[b];
                  r.charLen = charLengths[clen];
                  r.parity  = parityValues[p];
                  r.pe = endicount.parity-starticount.parity ;
                  r.fe = endicount.frame-starticount.frame ;
                  r.rx = endicount.rx-starticount.rx ;
                  r.brk = endicount.brk-starticount.brk ;

                  printf( "rx: %3d\tpe: %3d\tfe: %3d\t brk: %3d\n"
                          , r.rx 
                          , r.pe
                          , r.fe
                          , r.brk
                          );
                  if( 0 < inLen ){
                     memcpy(r.inData,inData,inLen);
                     r.inLen = inLen ;
                     hexDumper_t dumpRx(inData, inLen);
                     while( dumpRx.nextLine() )
                        printf( "%s\n", dumpRx.getLine() );
                  }
               }
            }
         }

         printf( "sorted results:\n" );
         qsort(results,numResults,sizeof(results[0]),compareResults);
         for( ridx = 0 ; ridx < numResults ; ridx++ ){
            struct results_t const &r = results[ridx];
            printf( "%6u %u %c:\trx: %3d\tpe: %3d\tfe: %3d\t brk: %3d\t"
                    , r.baud
                    , r.charLen
                    , r.parity
                    , r.rx 
                    , r.pe
                    , r.fe
                    , r.brk
                    );
            hexDumper_t dumpRx(r.inData, r.inLen);
            while( dumpRx.nextLine() )
               printf( "%s\n", dumpRx.getLine() );
         }
         
         close( fdSerial );
      }
      else
         perror( deviceName );
   }
   else
      fprintf( stderr, "Usage: setBaud /dev/ttyS0 baudRate\n" );
   return 0 ;
} 
