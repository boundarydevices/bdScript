#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mount.h>
#include <linux/mtd/mtd.h>
#include <syscall.h>
#include <signal.h>

#define __NR_myIoctl  __NR_ioctl
#define __NR_myOpen   __NR_open
#define __NR_myClose  __NR_close
#define __NR_mySignal __NR_signal

extern int errno ;
_syscall3( int,reboot, int, param1, int,param2, int,param3 );
_syscall3( int,write, int, fd, void const *, data, unsigned, dataBytes );
_syscall3( int,read, int, fd, void *, data, unsigned, dataBytes );
_syscall3( int,myIoctl, int, fd, int, cmd, unsigned long, arg );
_syscall2( int,myOpen, char const *, filename, unsigned, mode );
_syscall1( int,myClose, int, fd );
_syscall2( int,mySignal, int, signo, void *, action );
_syscall2( int,setpgid, int, pid, int, pgid );

//
// These routines are here to reduce the dependence on glibc
// (which is in the process of being reprogrammed)
//
static int myPrint( char const *msg )
{
   unsigned len = 0 ;
   char const *next = msg ;
   while( *next++ )
      len++ ;
   return write(1, msg, len );
}

static char const hexCharSet[] = { '0','1','2','3', '4','5','6','7', '8','9','A','B', 'C','D','E','F' };
static void myPrintHex( unsigned long v )
{
   unsigned char const *bytes = (unsigned char const *)&v ;
   unsigned i ;
   for( i = 0 ; i < 4 ; i++ )
   {
      unsigned char const nextByte = bytes[3-i];
      if( nextByte || ( 3 == i ) )
      {
         unsigned char const highNib = nextByte >> 4 ;
         unsigned char const lowNib = nextByte & 0x0F ;
         write( 1, hexCharSet+highNib, 1 );
         write( 1, hexCharSet+lowNib, 1 );
      }
   }
}

static void doReboot()
{
   reboot( (int) 0xfee1dead, 672274793, 0x89abcdef ); // allows <Ctrl-Alt-Del>
   reboot( (int) 0xfee1dead, 672274793, 0x1234567 ); // doesn't
}

static int myRead( unsigned int fd, unsigned char buf[], unsigned bufSize )
{
   return read( fd, buf, bufSize );
}

static int myWrite( unsigned int fd, unsigned char const buf[], unsigned bufSize )
{
   return write( fd, buf, bufSize );
}

int mtdErase( int Fd )
{

	mtd_info_t meminfo;

        mySignal(SIGTERM,SIG_IGN);
        mySignal(SIGHUP,SIG_IGN);
        setpgid( 0, 0 );

	if (myIoctl(Fd,MEMGETINFO,(unsigned long)&meminfo) == 0)
	{
           erase_info_t erase;
           int count ;

           myPrint( "flags       0x" ); myPrintHex( meminfo.flags ); myPrint( "\r\n" );
           myPrint( "size        0x" ); myPrintHex( meminfo.size ); myPrint( "\r\n" );
           myPrint( "erasesize   0x" ); myPrintHex( meminfo.erasesize ); myPrint( "\r\n" );
           myPrint( "oobblock    0x" ); myPrintHex( meminfo.oobblock ); myPrint( "\r\n" );
           myPrint( "oobsize     0x" ); myPrintHex( meminfo.oobsize ); myPrint( "\r\n" );
           myPrint( "ecctype     0x" ); myPrintHex( meminfo.ecctype ); myPrint( "\r\n" );
           myPrint( "eccsize     0x" ); myPrintHex( meminfo.eccsize ); myPrint( "\r\n" );

           count = meminfo.size / meminfo.erasesize ;

		erase.start = 0 ;

		erase.length = meminfo.erasesize;

		for (; count > 0; count--) {
                        myPrint("\rPerforming Flash Erase of length 0x" );
                        myPrintHex( erase.length );
                        myPrint(" at offset 0x" );
                        myPrintHex( erase.start );

			if (myIoctl(Fd,MEMERASE,(unsigned long)&erase) != 0)
			{      
				myPrint( "\r\nMTD Erase failure");
				return 8;
			}
			erase.start += meminfo.erasesize;
		}
		myPrint(" done\n");
	}
	return 0;
}

int main(int argc,char **argv, char **envp)
{
     int regcount;
     int devFd;
     int fileFd ;
     int res = 0;
     int arg ;

     if (2 >= argc)
     {
             myPrint( "usage: progFlash device fileName [reboot]\n");
             return 16;
     }

     // Open the device
     if ((devFd = myOpen( argv[1], O_RDWR)) < 0)
     {
             myPrint( argv[1] ); myPrint( ":File open error\r\n");
             
             return 8;
     }

     // Open the content file
     if ((fileFd = myOpen( argv[2], O_RDONLY)) < 0)
     {
             myPrint( argv[2] ); myPrint( ":File open error\r\n");
             myClose( devFd );
             return 8;
     }

     res = mtdErase(devFd);
     if( 0 == res )
     {
        unsigned char dataBuf[8192];
        int           numRead ;

        myPrint( argv[1] );
        myPrint( " erased successfully\n" );

        while( 0 < ( numRead = myRead( fileFd, dataBuf, sizeof( dataBuf ) ) ) )
        {
           int numWritten ;
           numWritten = myWrite( devFd, dataBuf, numRead );
           if( numWritten == numRead )
           {
              myPrint( "." );
           }
           else
           {
              myPrint( "read 0x" ); myPrintHex( numRead ); myPrint( " bytes, wrote 0x" ); 
                                    myPrintHex( numWritten ); myPrint( "\r\n" );
           }
        }
        myPrint( "\r\n" );
        
     }
     else
        myPrint( "!!!!! erase error\n" );

     myClose( fileFd );
     myClose( devFd );

     if( 3 < argc )
        doReboot();

     return res;
}
