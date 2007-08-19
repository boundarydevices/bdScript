/*
 * pxaregs - tool to display and modify PXA250's registers at runtime
 *
 * (c) Copyright 2002 by M&N Logistik-Lösungen Online GmbH
 * set under the GPLv2
 *
 * $Id: pxamem.c,v 1.1 2007-08-19 18:41:04 ericn Exp $
 *
 * Please send patches to h.schurig, working at mn-logistik.de
 * - added fix from Bernhard Nemec
 * - i2c registers from Stefan Eletzhofer
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

typedef int bool ;
#define true 1
#define false 0

static bool dump = false ;
static bool fill = false ;
static bool set = true ;

static void parseArgs( int *argc, char **argv )
{
   int i ;
   for( i = 1 ; i < *argc ; i++ ){
      char *arg = argv[i];
      if( '-' == *arg ){
         switch( arg[1] ){
            case 'd' : 
            case 'D' :
            {
               dump = true ;
               set = false ; 
               fill = false ;
               memcpy( argv+i, argv+i+1, ((*argc)-i)*sizeof(*argv));
               i-- ;
               (*argc)-- ;
               break ;
            }
            case 's' : 
            case 'S' :
            {
               dump = false ;
               set = true ; 
               fill = false ;
               memcpy( argv+i, argv+i+1, ((*argc)-i)*sizeof(*argv));
               i-- ;
               (*argc)-- ;
               break ;
            }
            
            case 'f' : 
            case 'F' :
            {
               dump = false ;
               set  = false ; 
               fill = true ;
               memcpy( argv+i, argv+i+1, ((*argc)-i)*sizeof(*argv));
               i-- ;
               (*argc)-- ;
               break ;
            }
         }
      }
   }
}

#define MAXMEM (1280*800*2)
#define MAP_SIZE 4096
#define MAP_MASK ( MAP_SIZE - 1 )

static int fd = -1 ;

static unsigned char *mapMem( unsigned long addr ){
   void *map, *regaddr;
   unsigned long val;

   if (fd == -1) {
      fd = open("/dev/mem", O_RDWR | O_SYNC);
      if (fd<0) {
          perror("open(\"/dev/mem\")");
          exit(1);
      }
   }

   map = mmap(0, MAXMEM, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK );
   if (map == (void*)-1 ) {
       perror("mmap()");
       exit(1);
   }

   return (unsigned char *)map + (addr & MAP_MASK);
}

static void unmapMem( void *mem, unsigned long addr )
{
   msync(mem,MAXMEM,MS_SYNC|MS_INVALIDATE);
   munmap( mem - (addr & MAP_MASK),MAXMEM);
}

static void readBytes( void *dest, unsigned long addr, unsigned long count ){
   unsigned char *src = mapMem(addr);
   memcpy(dest, src, count);
}

static unsigned long readLong(unsigned long addr)
{
   unsigned long rval ;
   readBytes(&rval, addr, sizeof(rval));
   return rval ;
}

static void setLong(unsigned long addr, unsigned long value)
{
   unsigned char *dest = mapMem(addr);
   memcpy(dest, &value, sizeof(value));
}

static void setBytes(unsigned long addr, unsigned long count, unsigned long value)
{
   unsigned char *dest = mapMem(addr);
   printf( "filling %u at address 0x%lx bytes with 0x%lx\n", count, addr, value);
   while( count >= sizeof(value) ){
      setLong(addr,value);
      addr += sizeof(value);
      count -= sizeof(value);
   }
}

static void usage(char const *prog){
      printf( "Usage: %s arguments\n"
              "Where arguments fits one of these patterns\n"
              "\n"
              "Get memory location\n"
              "   address\n"
              "\n"
              "Get memory range\n"
              "   address count\n"
              "\n"
              "Set memory location (longword)\n"
              "   [-s] address value\n"
              "\n"
              "Fill memory range\n"
              "   -f address count value(longword)\n",
              prog );
}

int main(int argc, char *argv[])
{
   parseArgs( &argc, argv );
   
   if( 2 == argc ){
      unsigned long addr = strtoul(argv[1],0,0);
      printf( "addr[0x%lx] == 0x%lx\n", addr, readLong(addr) );
   } else if ( 3 == argc ){
      if( set ){
         unsigned long addr = strtoul(argv[1],0,0);
         unsigned long value = strtoul(argv[2],0,0);
         setLong(addr,value);
      } else if( dump ){
         unsigned long addr = strtoul(argv[1],0,0);
         unsigned long count = strtoul(argv[2],0,0);
         unsigned char *const data = (unsigned char *)malloc(count);
         unsigned long i ;
         readBytes(data,addr,count);
         for( i = 0 ; i < count ; i++ ){
            if( 0 == (i&15))
               printf( "%08lx   ", i );
            printf( "%02x ", data[i] );
            if( 7 == (i&7) )
               printf( "  " );
            if( 15 == (i&15) )
               printf( "\n" );
         }
         if( 0 != (count&15))
            printf( "\n" );
         printf( "dump 0x%lx bytes from address 0x%lx here\n", count, addr );
      } else {
         printf( "unknown parameter set\n" );
         usage(argv[0]);
      }
   } else if ( 4 == argc ){
      if( fill ){
         unsigned long addr = strtoul(argv[1],0,0);
         unsigned long count = strtoul(argv[2],0,0);
         unsigned long value = strtoul(argv[3],0,0);
         setBytes(addr,count,value);
      } else {
         printf( "unknown parameter set\n" );
         usage(argv[0]);
      }
   } else {
      usage(argv[0]);
   }

   if( 0 <= fd ){
      close(fd);
      fd = -1 ;
   }
   return 0 ;
}
