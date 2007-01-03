#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "i2c-dev.h"
#include <ctype.h>
#include <sys/mman.h>
#include "tickMs.h"

static void unescape( char     *s,        // in+out
                      unsigned &length )  // out
{
   enum state_t {
      normal,
      backslash,
      hex1,
      hex2
   };

   char *const start = s ;
   char *nextOut = start ;
   char c ;
   state_t state = normal ;

   while( 0 != (c = *s++) ){
      switch( state ){
         case normal: {
            if( '\\' == c ){
               state = backslash ;
            }
            else 
               *nextOut++ = c ;
            break ;
         }
         case backslash: {
            if( 'n' == c ){
               *nextOut++ = '\n' ;
            }
            else if( 'r' == c ){
               *nextOut++ = '\r' ;
            }
            else if( '\\' == c ){
               *nextOut++ = '\\' ;
            }
            else if( 'x' == c ){
               state = hex1 ;
            }
            break ;
         }
         case hex1: {
            if( ('0' <= c) && ('9' >= c) )
               *nextOut = (c-'0')<< 4;
            else if( ('a' <= c) && ('f' >= c) )
               *nextOut = (c-'a'+10)<< 4;
            else if( ('A' <= c) && ('F' >= c) )
               *nextOut = (c-'A'+10)<< 4;
            else
               state = normal ;
            if( state != normal )
               state = hex2 ;
            break ;
         }
         case hex2: {
            if( ('0' <= c) && ('9' >= c) )
               *nextOut++ += c-'0' ;
            else if( ('a' <= c) && ('f' >= c) )
               *nextOut++ += c-'a'+10 ;
            else if( ('A' <= c) && ('F' >= c) )
               *nextOut++ += c-'a'+10 ;
            state = normal ;
            break ;
         }
      }
   }

   length = nextOut-start ;
}

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

#define ICR_RESET    0x47a0         // ICR_UR   ~ICR_IUE 
#define ICR_MAENABLE 0x07e0         //~ICR_UR    ICR_IUE

#define ICR_TB_BIT   3
#define ICR_TB (1<<ICR_TB_BIT)
#define ICR_UR_BIT 14

#define ICR_START_BIT 0
#define ICR_STOP_BIT 1

#define ICR_ALDIE_BIT 12
#define ICR_ALDIE (1<<ICR_ALDIE_BIT)

#define ISR_SAD_BIT  9
#define ISR_SAD   (1<<ISR_SAD_BIT)
#define ISR_BED   (1<<10)
#define ISR_MASTER_RX   1

#define ISR_ALD_BIT 5
#define ISR_ALD (1<<ISR_ALD_BIT)

#define ISR_IRF_BIT 7
#define ISR_IRF (1<<7)

#define ISR_ITE_BIT 6
#define ISR_ITE   (1<<ISR_ITE_BIT)

static void dumpRegs()
{
   printf( "IBMR == 0x%08lx\n", READREG(IBMR));
   printf( "IDBR == 0x%08lx\n", READREG(IDBR));
   printf( "ICR == 0x%08lx\n", READREG(ICR));
   printf( "ISR == 0x%08lx\n", READREG(ISR));
   printf( "ISAR == 0x%08lx\n", READREG(ISAR));
}


int main( int argc, char const * const argv[] )
{
   if( 2 < argc ){
      unsigned long const addr = strtoul(argv[1],0,0);
   
      int fd = open( "/dev/mem", O_RDWR | O_SYNC);
      if( 0 <= fd ){
         map = (char *)mmap( 0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, I2CBASE );
         if (map == (char*)-1 ) {
            perror("mmap()");
            exit(1);
         }
   
         char *data = strdup(argv[2]);
         unsigned len ;
         unescape( data, len );
         for( unsigned i = 0 ; i < len ; i++ ){
            printf( "%02x ", data[i] );
         }
         printf( "\n" );

         WRITEREG(ISAR, addr);
         WRITEREG(ICR,ICR_RESET); // 
         WRITEREG(ISR,0x7ff);    // ack interrupts
         usleep(1000);
      
         WRITEREG(ICR,ICR_MAENABLE);
         CLRREGBIT(ICR,ICR_UR_BIT);

         WRITEREG(IDBR, addr); // write address byte

         SETREGBIT(ICR,ICR_START_BIT);
         do {
printf( "tx %02x\n", READREG(IDBR));

            SETREGBIT(ICR,ICR_TB_BIT);
            long long start = tickMs();
            unsigned long prevISR = 0 ;
            do {
               unsigned long isr = READREG(ISR);
               if( isr & ISR_ITE ){
                  WRITEREG(ISR,ISR_ITE); // ack
                  if( isr & ISR_ALD ){
                     printf( "ALD!\n" );
                     WRITEREG(ISR,ISR_ALD);
//                     SETREGBIT(ISR,ISR_ALD_BIT); // ack
                  }
                  break ;
               } else if( isr & ISR_BED ){
                  printf( "Bus error\n" );
                  WRITEREG(ISR,ISR_BED);
                  len++ ;
                  goto err ; // again ;
               }
/*
               else if( isr != prevISR )
                  printf( "WriteWait: %08lx\n", isr );
*/

               prevISR = isr ;
               long long now = tickMs();
               if( now-start > 2000 ){
                  printf( "timeout waiting for data\n" );
                  goto err ;
               }
            } while( 1 );

            CLRREGBIT(ICR,ICR_START_BIT);
            WRITEREG(IDBR,*data++);

/*
            if(0==len)
               SETREGBIT(ICR,ICR_STOP_BIT);
*/
again:
            SETREGBIT(ICR,ICR_TB_BIT);
         } while( 0 < len-- );

         // wait for transmit empty
         long long start = tickMs();
         do {
            unsigned long isr = READREG(ISR);
            if( isr & ISR_ITE ){
               SETREGBIT(ISR,ISR_ITE_BIT); // ack
               if( isr & ISR_ALD )
                  SETREGBIT(ISR,ISR_ALD_BIT); // ack
               break ;
            } else if( isr & ISR_BED ){
               goto err ;
            }
            long long now = tickMs();
            if( now-start > 2000 ){
               printf( "timeout waiting for data\n" );
               goto err ;
            }
         } while( 1 );

         WRITEREG(ICR,ICR_RESET);
         close( fd );
      }
      else
         perror( "/dev/mem" );
   }
   else
      fprintf( stderr, "Usage: %s slaveAddr data(C-escaped)\n", argv[0] );

   return 0 ;

err:
printf( "Bailing out\n" );
//   WRITEREG(ICR,ICR_RESET);
   return -1 ;

}

