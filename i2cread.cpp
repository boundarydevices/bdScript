#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
typedef unsigned short __u16 ;
typedef unsigned char  __u8 ;
typedef long int       __s32 ;
#include "i2c-dev.h"
#include <ctype.h>
#include <sys/mman.h>
#include "tickMs.h"

static char *map = 0 ;

#define I2CBASE 0x40301000

#define IBMR (0x40301680-I2CBASE)   //  I2C Bus Monitor Register - IBMR
#define IDBR (0x40301688-I2CBASE)   //  I2C Data Buffer Register - IDBR
#define ICR (0x40301690-I2CBASE)    //  I2C Control Register - ICR
#define ISR (0x40301698-I2CBASE)    //  I2C Status Register - ISR
#define ISAR (0x403016A0-I2CBASE)   //  I2C Slave Address Register - ISAR

#define MAP_SIZE 4096

#define READREG(addr) (*((unsigned long volatile *)(map+addr)))
#define WRITEREG(addr,value)  *((unsigned long volatile *)(map+addr)) = value
#define SETREGBIT(addr,bit)   *((unsigned long volatile *)(map+addr)) |= (1<<bit)
#define CLRREGBIT(addr,bit)   *((unsigned long volatile *)(map+addr)) &= ~((unsigned long)(1<<bit))

#define ICR_RESET    0x6f00         // ICR_UR   ~ICR_IUE 
#define ICR_SLENABLE 0x2F40         //~ICR_UR    ICR_IUE

#define ICR_TB_BIT   3
#define ICR_TB (1<<ICR_TB_BIT)
#define ICR_UR_BIT 14

#define ISR_SAD_BIT  9
#define ISR_SAD   (1<<ISR_SAD_BIT)
#define ISR_BED   (1<<10)
#define ISR_MASTER_RX   1
#define ISR_SSD_BIT   4
#define ISR_SSD   (1<<ISR_SSD_BIT)

#define ISR_IRF_BIT 7
#define ISR_IRF (1<<7)

#define ISR_ACKNAKBIT 1
#define ISR_ACKNAK (1<<ISR_ACKNAKBIT)


static void dumpRegs()
{
   printf( "IBMR == 0x%08lx\n", READREG(IBMR));
   printf( "IDBR == 0x%08lx\n", READREG(IDBR));
   printf( "ICR == 0x%08lx\n", READREG(ICR));
   printf( "ISR == 0x%08lx\n", READREG(ISR));
   printf( "ISAR == 0x%08lx\n", READREG(ISAR));
}

static bool read( char     *data,   // out
                  unsigned &len )   // in+out
{
   unsigned const max = len ;
   len = 0 ;

   WRITEREG(ICR,ICR_RESET); // 
   WRITEREG(ISR,0x7ff);    // ack interrupts
   usleep(1000);

   WRITEREG(ICR,ICR_SLENABLE);
   CLRREGBIT(ICR,ICR_UR_BIT);

   bool worked = false ;
   for( unsigned i = 0 ; !worked && ( i < 1 ); i++ ){
      long long start = tickMs();
      do {
         unsigned long isr = READREG(ISR);
         if( isr & ISR_SAD ){
            if( isr & ISR_MASTER_RX ){
               printf( "Master rx\n" );
               break ;
            }
   
            if( isr & ISR_SSD ){
   //            SETREGBIT(ISR,ISR_SSD_BIT);
               printf( "Slave stop: no data\n" );
               return true ;
            }
   
            if( isr & ISR_ACKNAK ){
   //            SETREGBIT(ISR,ISR_SSD_BIT);
               printf( "nak waiting for slave\n" );
               SETREGBIT(ISR,ISR_ACKNAKBIT);
               return true ;
            }

            SETREGBIT(ISR,9);

            worked = true ;
            break ;
         }
         else if( isr & ISR_BED ){
            printf( "bus error\n" );
            break ;
         }
   
         long long now = tickMs();
         if( now-start > 2000 ){
            printf( "timeout waiting for slave\n" );
            dumpRegs();
            break ;
         }
      } while( 1 );
      if( !worked ){
         printf( "retry\n" );
         WRITEREG(ICR,ICR_RESET); // 
         usleep((i+1)*1000);
      
         CLRREGBIT(ICR,ICR_UR_BIT);
         usleep((i+1)*1000);
         
         WRITEREG(ICR,ICR_SLENABLE);
      }
   }

   do {
      SETREGBIT(ICR,ICR_TB_BIT);
      long long start = tickMs();
      do { // wait for rx
         unsigned long isr = READREG(ISR);
         if( isr & ISR_IRF ){
            char const byte = READREG(IDBR);
            data[len++] = byte ;
            SETREGBIT(ISR,ISR_IRF_BIT); // ack
            if( isr & ISR_SSD ){
//               SETREGBIT(ISR,ISR_SSD_BIT);
               return true ;
            }
            break ;
         } else if( isr & ISR_SSD ){
               return true ;
         }
         long long now = tickMs();
         if( now-start > 2000 ){
            printf( "timeout waiting for data\n" );
            return false ;
         }
      } while( 1 );
   } while( 1 );

   return true ;
}

int main( int argc, char const * const argv[] )
{
   printf( "Hello, %s\n", argv[0] );

   unsigned long const addr = ( 1 < argc ) ? strtoul(argv[1],0,0) : 0x28 ;

   int fd = open( "/dev/mem", O_RDWR | O_SYNC);
   if( 0 <= fd ){
      map = (char *)mmap( 0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, I2CBASE );
      if (map == (char*)-1 ) {
         perror("mmap()");
         exit(1);
      }

      WRITEREG(ISAR, addr);

      char inBuf[80];
      unsigned len = sizeof(inBuf);
      long long const start = tickMs();
      if( read( inBuf, len ) ){
         printf( "read %u bytes in %llu ms\n", len, tickMs()-start );
         for( unsigned i = 0 ; i < len ; i++ ){
            printf( "%02x ", inBuf[i] );
         }
         printf( "\n" );
      }
      else
         printf( "Error reading I2C data\n" );

//      WRITEREG(ICR,ICR_RESET);
      close( fd );
   }
   else
      perror( "/dev/mem" );

   return 0 ;
}
