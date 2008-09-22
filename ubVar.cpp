/*
 * Module ubVar.cpp
 *
 * This module defines the readFlashVar() and writeFlashVar()
 * routines as declared in ubVar.h
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "ubVar.h"
#include <fcntl.h>
#include <time.h>
#include <sys/mount.h>
#ifndef KERNEL_2_4
#include <linux/types.h>
#include <mtd/mtd-user.h>
#else
#include <linux/mtd/mtd.h>
#endif
#include <syscall.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "hexDump.h"

// #define DEBUGPRINT
#include "debugPrint.h"

#define pageSize 4096

static char const deviceName_[] = {
   "/dev/mtd0"
};
char const * GetFlashDev()
{
   char const *devName = getenv( "FLASHVARDEV" );
   if( 0 == devName ) devName = deviceName_;
   return devName;
}

unsigned int IsDevice(char const* devName)
{
    return ((devName[0]=='/') && (devName[1]=='d') && (devName[2]=='e') && (devName[3]=='v'));
}

// returns 0 if successful, non-zero on errors
static int ubVarOffset(int fdMTD, unsigned &offset, unsigned &maxSize){
   int rval = -1 ;
   mtd_info_t meminfo;
   if( ioctl( fdMTD, MEMGETINFO, (unsigned long)&meminfo) == 0)
   {
      debugPrint( "flags       0x%lx\n", meminfo.flags ); 
      debugPrint( "size        0x%lx\n", meminfo.size ); 
      debugPrint( "erasesize   0x%lx\n", meminfo.erasesize ); 
      //            debugPrint( "oobblock    0x%lx\n", meminfo.oobblock ); 
      debugPrint( "oobsize     0x%lx\n", meminfo.oobsize ); 
      debugPrint( "ecctype     0x%lx\n", meminfo.ecctype ); 
      debugPrint( "eccsize     0x%lx\n", meminfo.eccsize ); 
      debugPrint( "numSectors  0x%lx\n", meminfo.size / meminfo.erasesize );

      offset = 0 ;

      if( offset == (unsigned)lseek( fdMTD, offset, SEEK_SET ) )
      {
         // nearest size in pages (probably always equal)
         maxSize = ( meminfo.erasesize / pageSize ) * pageSize ;
         rval = 0 ;
      } else perror( "lseek" );
   } else perror( "MEMGETINFO" );

   return rval ;
}

// #define TESTREST 1

struct listEntry_t {
   struct listEntry_t *next_ ;
   char const         *name_ ;
   char const         *value_ ;
};

class readVars_t {
public:
   readVars_t( void );
   ~readVars_t( void );

   bool worked( void ) const { return worked_ ; }
   char const *startChar( void ) const { return image_ ; }
   unsigned    bytesUsed( void ) const { return bytesUsed_ ; }
   unsigned    maxSize( void ) const { return maxSize_ ; }

   bool                worked_ ;
   char const         *image_ ;
   unsigned            bytesUsed_ ;
   unsigned            maxSize_ ;
   struct listEntry_t *vars_ ;
};

readVars_t :: readVars_t( void )
   : worked_( false )
   , image_( 0 )
   , bytesUsed_( 0 )
   , maxSize_( 0 )
   , vars_( 0 )
{
   image_ = 0 ;
   bytesUsed_ = 0 ;
   maxSize_ = 0 ;

   char const *devName = GetFlashDev();
   unsigned int bDevice = IsDevice(devName);
   debugPrint( "opening flash device %s\n", devName );

   int fd = open( devName, O_RDONLY );
   if ( 0 <= fd )
   {
      unsigned offset ;
      if (bDevice && ( 0 == ubVarOffset(fd, offset, maxSize_) )){
         debugPrint( "ubVar offset 0x%x, max size %u\n", offset, maxSize_ );
      } else {
        maxSize_ = 0x40000;
      }
      if (maxSize_) {
            image_ = (char *)malloc( maxSize_ );
            debugPrint( "maxSize 0x%x\n", maxSize_ );

            int numRead = ::read( fd, (void *)image_, maxSize_ );
            if(maxSize_== (unsigned)numRead) {
               unsigned long crc ;
               memcpy(&crc, image_, sizeof(crc));
               char *nextIn = (char *)image_ + 4 ;
               unsigned bytesLeft = maxSize_-sizeof(crc);
               debugPrint( "crc == %lx\n", crc );
               unsigned long calculated = crc32( 0, (Bytef *)nextIn, bytesLeft );
               debugPrint( "computed == %lx\n", calculated );
               if( crc == calculated ){
                  char *const end = nextIn + bytesLeft ;
                  while( nextIn < end ){
                     // find variable name
                     while( ( '\0' == *nextIn ) && ( nextIn < end ) ){
                        nextIn++ ;
                     }
                     if( nextIn < end ){
                        struct listEntry_t *entry = new struct listEntry_t ;
                        entry->next_ = vars_ ;
                        entry->name_ = nextIn ;
                        entry->value_ = 0 ;
                        while( ('=' != *nextIn) && (nextIn < end) ){
                           nextIn++ ;
                        }
                        if( nextIn < end ){
                           *nextIn++ = 0 ;
                           debugPrint( "%s:", entry->name_ );
                           entry->value_ = nextIn ;
                           vars_ = entry ;
                           while( ( '\0' != *nextIn ) && ( nextIn < end ) ){
                              nextIn++ ;
                           }
                           debugPrint( "%s\n", entry->value_ );
                           bytesUsed_ = nextIn-(char *)image_ + 1 ;
                        }
                        else
                           fprintf( stderr, "missing = at offs %u\n", entry->name_ - (char *)image_ );

                        if( !entry->value_ ){
                           delete entry ;
                        }
                     }
                  }
                  worked_ = true ;
               }
               else
                  fprintf( stderr, "%s: crc mismatch: 0x%lx/0x%lx\n", __FUNCTION__, crc, calculated );
            }
            else
               perror( devName );
      }
      close( fd );
   }
   else
      perror(devName);
}

readVars_t :: ~readVars_t( void )
{
   if( 0 != image_ )
      free( (char *)image_ );
   while( 0 != vars_ ){
      struct listEntry_t *entry = vars_ ;
      vars_ = entry->next_ ;
      delete entry ;
   }
}

class varIter_t {
public:
   varIter_t( readVars_t const &rv );

   bool next( char const *&name, unsigned &nameLen,
              char const *&data, unsigned &dataLen );

private:
   readVars_t const   &rv_ ;
   struct listEntry_t *next_ ;
};

varIter_t::varIter_t( readVars_t const &rv )
   : rv_( rv )
   , next_( rv.worked() ? rv.vars_ : 0 )
{
}

bool varIter_t::next
   ( char const *&name, unsigned &nameLen,
     char const *&data, unsigned &dataLen )
{
   if( next_ ){
      name  = next_->name_ ; nameLen = strlen(name);
      data  = next_->value_ ; dataLen = strlen(data);
      next_ = next_->next_ ;
      return true ;
   }
   name = data = 0 ;
   nameLen = dataLen = 0 ;
   return false ;
}

char const *readFlashVar( char const *varName )
{
   readVars_t rv ;
   if( rv.worked() )
   {
      unsigned const varNameLen = strlen( varName );

      char const *rVal = 0 ;
      unsigned    rLen = 0 ;
      varIter_t it( rv );

      char const *name ; unsigned nameLen ;
      char const *data ; unsigned dataLen ;
      while( it.next( name, nameLen, data, dataLen ) )
      {
debugPrint( "%s %s\n", varName,name );
         if( varNameLen == nameLen )
         {
            if( 0 == memcmp( varName, name, varNameLen ) )
            {
               rVal = data ;
               rLen = dataLen ;
            } // match
         }
      }

      if( ( 0 != rVal ) && ( 0 <= rLen ) )
      {
         char *out = (char *)malloc( rLen + 1 );
         if( out )
         {
            memcpy( out, rVal, rLen );
            out[rLen] = '\0' ; // NULL terminate
debugPrint( "%s\n", out );

            return out ;
         }
      }
   }

   return 0 ;
}

//
// writes the specified flash variable with the specified value
//
void writeFlashVar( char const *name,
                    char const *value )
{
   char const *devName = GetFlashDev();
   debugPrint( "opening flash device %s\n", devName );
   readVars_t rv ;
   if( rv.worked() )
   {
      unsigned const nameLen = strlen( name );
      unsigned const dataLen = strlen( value );
      unsigned const avail = rv.maxSize() - rv.bytesUsed();
      unsigned int bDevice = IsDevice(devName);
      if (bDevice && ( avail > nameLen + dataLen + 2 ))
      {
         printf( "set value %s to %s here\n", name, value );
         struct listEntry_t *next = rv.vars_ ;
         while( 0 != next ){
            if(0 == strcmp(next->name_, name)){
               break;
            }
            else
               next = next->next_ ;
         }

         char *nextOut = (char *)rv.image_ + rv.bytesUsed();
         if(0==next){
            next = new struct listEntry_t ;
            next->name_ = nextOut ;
            strcpy((char *)next->name_, name );
            nextOut += nameLen + 1 ;
            next->value_ = nextOut ; // decided below
            strcpy((char *)next->value_, value );
            nextOut += dataLen + 1 ;
            next->next_ = rv.vars_ ;
            rv.vars_ = next ;
         } else if( dataLen <= strlen(next->value_) ){
            strcpy((char *)next->value_,value);
         } else {
            next->value_ = nextOut ; // decided below
            strcpy((char *)next->value_, value );
            nextOut += dataLen+1 ;
         }

         char *const outImg = new char [rv.maxSize()];
         memset(outImg,0,rv.maxSize());
         nextOut = outImg+sizeof(unsigned long);
         next = rv.vars_ ;
         while( next ){
            char const *nextIn = next->name_ ;
            while( *nextIn ){
               *nextOut++ = *nextIn++ ;
            }
            *nextOut++ = '=' ;
            nextIn = next->value_ ;
            while( *nextIn ){
               *nextOut++ = *nextIn++ ;
            }
            *nextOut++ = '\0' ;
            next = next->next_ ;
         }
         unsigned long crc = crc32(0,(Bytef *)outImg+sizeof(unsigned long),rv.maxSize()-sizeof(unsigned long));
         memcpy(outImg,&crc,sizeof(crc));
         hexDumper_t dump(outImg,nextOut-outImg);
         while( dump.nextLine() )
            printf( "%s\n", dump.getLine() );
         
         int fd = open( devName, O_WRONLY );
         if( 0 <= fd )
         {
            mtd_info_t meminfo;
            if( 0 == ioctl( fd, MEMGETINFO, (unsigned long)&meminfo) )
            {
               if( meminfo.erasesize == rv.maxSize() )
               {
                  unsigned const offset = 0 ;
                  erase_info_t erase;
                  erase.start = offset ;
                  erase.length = meminfo.erasesize;
                  if( 0 == ioctl( fd, MEMERASE, (unsigned long)&erase) )
                  {
                     if( offset == (unsigned)lseek( fd, offset, SEEK_SET ) )
                     {
                        int numWritten = write(fd,outImg,rv.maxSize());
                        if( (unsigned)numWritten != rv.maxSize() ){
                           perror( "write" );
                        }
                     }
                     else
                        perror( "lseek3" );
                  }
                  else
                     fprintf( stderr, "erase error %m\n" );
               }
               else
                  fprintf( stderr, "erase size mismatch: rv %u, mi %u\n", rv.maxSize(), meminfo.erasesize );
            }
            else
               perror( "MEMGETINFO" );

            close( fd );
         }
         else
            fprintf(stderr, "open %s: %m\n", devName);
         delete [] outImg ;
      }
      else
         printf( "save variables to file here\n" );
   }
}

#ifdef STANDALONE
#include "tickMs.h"

int main( int argc, char const * const argv[] )
{
   char const *const varName = argv[1];
   switch( argc )
   {
      case 2:
         {
            char const * value = readFlashVar( varName );
            if( value )
            {
               printf( "%s\n", value );
               free( (char *)value );
            }
            else if( 0 == strcmp( varName, "-erase" ) ){
               char const *devName = GetFlashDev();
               int fd = open( devName, O_RDWR );
               unsigned offset, maxSize ;
               if( ( 0 <= fd )
                   &&
                   ( 0 == ubVarOffset(fd,offset,maxSize) ) ){
                     erase_info_t erase;
                     erase.start = offset ;
                     erase.length = maxSize ;
                     if( 0 == ioctl( fd, MEMERASE, (unsigned long)&erase) )
                     {
                        printf( "Ok\n" );
                     }
                     else
                        fprintf( stderr, "erase error %m\n" );
               }
               else
                  fprintf( stderr, "%s: %m\n", devName );
            }
            else {
               fprintf( stderr, "%s is not set\n", varName );
               return -1 ;
            }
            break;
         } // read flash var
      case 3:
         {
            char const * oldval = readFlashVar( varName );
            if( ( 0 == oldval ) || ( 0 != strcmp(argv[2],oldval) ) )
            {
               writeFlashVar( varName, argv[2] );
            }
            break;
         } // write flash var
      default:
         {
            readVars_t rv ;

            if( rv.worked() )
            {
               varIter_t it( rv );

               char const *name ; unsigned nameLen ;
               char const *data ; unsigned dataLen ;
               while( it.next( name, nameLen, data, dataLen ) )
               {
                  write( 1, name, nameLen );
                  write( 1, "=", 1 );
                  write( 1, data, dataLen );
                  write( 1, "\n", 1 );
               }
               printf( "used %u of %u bytes\n", rv.bytesUsed_, rv.maxSize_ );
            }
         } // dump flash data
   }
   return 0 ;
}

#endif
