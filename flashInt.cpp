/*
 * Program flashInt.cpp
 *
 * This test program allows interaction via the 
 * keyboard with a flash movie.
 *
 *
 * Change History : 
 *
 * $Log: flashInt.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "flash/flash.h"
#include "flash/swf.h"
#include "flash/movie.h"

#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/poll.h>
#include "fbDev.h"
#include "memFile.h"
#include <unistd.h>
#include "tickMs.h"
#include <ctype.h>
#include "debugPrint.h"
#include "hexDump.h"

static bool volatile doExit = false ;

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\r\n" );

   doExit = true ;
}

static void setRaw( int fd, struct termios &oldState )
{
   tcgetattr(fd,&oldState);

   /* set raw mode for keyboard input */
   struct termios newState = oldState;
   newState.c_cc[VMIN] = 1;

   //
   // Note that this doesn't appear to work!
   // Reads always seem to be terminated at 16 chars!
   //
   newState.c_cc[VTIME] = 0; // 1/10th's of a second, see http://www.opengroup.org/onlinepubs/007908799/xbd/termios.html

   newState.c_cflag &= ~(PARENB|CSTOPB|CSIZE|CRTSCTS);              // Mask character size to 8 bits, no parity, Disable hardware flow control
   newState.c_cflag |= (CLOCAL | CREAD |CS8);                       // Select 8 data bits
   newState.c_lflag &= ~(ICANON | ECHO );                           // set raw mode for input
   newState.c_iflag &= ~(IXON | IXOFF | IXANY|INLCR|ICRNL|IUCLC);   //no software flow control
   newState.c_oflag &= ~OPOST;                      //raw output
   tcsetattr( fd, TCSANOW, &newState );
}

static void
getUrl(char *url, char *target, void *client_data)
{
	debugPrint("GetURL : %s\n", url);
}

static void
getSwf(char *url, int level, void *client_data)
{
	debugPrint("GetSwf : %s\n", url);
}


int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
      printf( "Hello, %s\n", argv[0] );
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         FlashHandle const hFlash = FlashNew();
         int status ;
   
         // Load level 0 movie
         do {
            status = FlashParse( hFlash, 0, (char *)fIn.getData(), fIn.getLength() );
            printf( "parse: %04x\n", status );
         } while( status & FLASH_PARSE_NEED_DATA );
         
         if( FLASH_PARSE_START & status )
         {
            struct FlashInfo fi;
            FlashGetInfo( hFlash, &fi );
            printf( "   rate %lu\n"
                    "   count %lu\n"
                    "   width %lu\n"
                    "   height %lu\n"
                    "   version %lu\n", 
                    fi.frameRate, fi.frameCount, fi.frameWidth, fi.frameHeight, fi.version );

            fbDevice_t &fb = getFB();
            
            unsigned targetX = 0 ;
            unsigned targetY = 0 ;
            unsigned targetWidth = fb.getWidth();
            unsigned targetHeight = fb.getHeight();
            if( 2 < argc ){
               targetX = strtoul(argv[2],0,0);
               if( 3 < argc ){
                  targetY = strtoul(argv[3],0,0);
                  if( 4 < argc ){
                     targetWidth = strtoul(argv[4],0,0);
                     if( 5 < argc ){
                        targetHeight = strtoul(argv[5],0,0);
                     }
                  }
               }
            }
            if( targetHeight+targetY > fb.getHeight() )
            {
               printf( "invalid height/y: %u/%u, max %u\n", targetHeight+targetY, fb.getHeight() );
               return 0 ;
            }

            if( targetWidth+targetX > fb.getWidth() )
            {
               printf( "invalid width/x: %u/%u, max %u\n", targetWidth+targetX, fb.getWidth() );
               return 0 ;
            }

            printf( "display at %u:%u, %ux%u\n", targetX, targetY, targetWidth, targetHeight );

            fb.doubleBuffer();

            FlashDisplay display ;
            memset( &display, 0, sizeof( display ) );
            display.width  = fb.getWidth() ;
            display.height = targetHeight;

            unsigned const scratchLen = display.width*display.height ;
            unsigned short * const scratch = new unsigned short [ scratchLen ];

            display.pixels = scratch ; // fb.getRow(targetY) + targetX ;
            display.bpl    = display.width*sizeof(unsigned short);
            display.depth  = 
            display.bpp    = sizeof(unsigned short);
            display.flash_refresh = 0 ;
            display.clip_x = 0 ;
            display.clip_y = 0 ;
            display.clip_width = targetWidth ;
            display.clip_height = targetHeight ;
         
            FlashGraphicInit( hFlash, &display );
            FlashSetGetUrlMethod( hFlash, getUrl, 0);
            FlashSetGetSwfMethod( hFlash, getSwf, 0);

            FlashMovie *const fh = (FlashMovie *)hFlash ;
            rectangle_t copyBack[2] = {0};
            copyBack[0].xLeft_  = targetX ;
            copyBack[0].yTop_   = targetY ;
            copyBack[0].width_  = targetWidth ;
            copyBack[0].height_ = targetHeight ;
            
            signal( SIGINT, ctrlcHandler );
            
            struct termios oldTtyState ;
            if( isatty(0) )
               setRaw( 0, oldTtyState );

            int c ;
            while( !doExit && ( EOF != ( c = fgetc(stdin) ) ) )
            {
               switch( toupper(c) )
               {
                  case 'Q' :
                  case '\x03' : // <ctrl-c>
                  case '\x04' : // <ctrl-d>
                     doExit = 1 ;
                     break ;
                  case 'E' :
                  {
                      struct timeval wakeTime ;
                      display.pixels = fb.getRow(targetY) + targetX ;
                      fh->gd->canvasBuffer = (unsigned char *)display.pixels ;
                      display.flash_refresh = 0 ;
                      long long start = tickMs();
                      long wakeUp = FlashExec( hFlash, FLASH_WAKEUP, 0, &wakeTime);
                      long long end = tickMs();
                      unsigned long execTime = end-start ;
                      printf( "exec: rval 0x%lx, elapsed %lu ms, refresh %d, wake %lu/%lu\r\n", 
                              wakeUp, execTime, display.flash_refresh, 
                              wakeTime.tv_sec, wakeTime.tv_usec );
                      if( display.flash_refresh )
                         fb.flip(copyBack);
                      break ;
                  }
                  default:
                     if( isprint(c) )
                        printf( "unknown cmd '%c'\r\n"
                                "      e        - exec\r\n"
                                "      q        - quit\r\n"
                                "      <ctrl-c> - exit\r\n"
                                "      <ctrl-d> - exit\r\n"
                                );
               }
            }

            if( isatty(0) )
               tcsetattr( 0, TCSANOW, &oldTtyState );          

            hexDumper_t dumpScratch( scratch, 1024 );
            while( dumpScratch.nextLine() )
               printf( "%s\n", dumpScratch.getLine() );

            for( unsigned i = 0 ; i < scratchLen ; i++ )
               if( 0 != scratch[i] )
               {
                  printf( "Non-zero scratch byte %u\n", i );
                  break ;
               }
         }
         else
            printf( "Error parsing flash\n", status );

         FlashClose( hFlash );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: flashInt movie.swf\n" );
   return 0 ;
} 
