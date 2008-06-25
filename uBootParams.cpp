/*
 * Module uBootParams.cpp
 *
 * This module defines the methods of the uBootParams_t
 * class as declared in uBootParams.h
 *
 *
 * Change History : 
 *
 * $Log: uBootParams.cpp,v $
 * Revision 1.1  2008-06-25 22:07:24  ericn
 * -Import
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "uBootParams.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <mtd/mtd-user.h>
#include <assert.h>

uBootParams_t::uBootParams_t( char const *fileName )
   : worked_( false )
   , head_ ( 0 )
   , dataMax_( 0 )
   , dataLen_( 0 )
   , dataBuf_( 0 )
{
   int fdDev = open( fileName, O_RDONLY );
   if( 0 > fdDev ){
      perror( fileName );
      return ;
   }

   long int eof = lseek( fdDev, 0, SEEK_END );
   if( 0 >= eof ){
      perror( "lseek" );
      close( fdDev );
      return ;
   }
   
   dataBuf_ = new char [eof];
   long int start = lseek( fdDev, 0, SEEK_SET );
   int numRead = read( fdDev, dataBuf_, eof );
   if( numRead != eof ){
      fprintf( stderr, "read:%d of %lu from pos %ld:%m\n", numRead, eof, start );
      delete [] dataBuf_ ;
      close( fdDev );
      return ;
   }

   uLong crc ; memcpy(&crc,dataBuf_,sizeof(crc));
   char const *nextIn = dataBuf_ + sizeof(crc);
   uLong computed = crc32( 0, (Bytef *)nextIn, eof-sizeof(crc) );

   if( crc == computed ){
      worked_ = true ;
      char * const dataEnd = dataBuf_+eof ;
      while( (nextIn < dataEnd) && *nextIn ){
         char const *nameStart = nextIn ;
         char c ;
         char const *nameEnd = 0 ;
         while( (nextIn < dataEnd) && ('\0' != (c=*nextIn)) ){
            if( '=' == c ){
               nameEnd = nextIn++ ;
               break;
            }
            else
               nextIn++ ;
         }
         if( nameEnd ){
            char const *valueStart = nextIn ;
            while( (nextIn < dataEnd) && ('\0' != (c=*nextIn)) ){
               nextIn++ ;
            }
            if( (nextIn < dataEnd) && ('\0' == c) ){
   #if 0
               fwrite( nameStart, 1, nameEnd-nameStart, stdout );
               fputc(':',stdout);
               fwrite( valueStart, 1, nextIn-valueStart, stdout );
               fputc('\n',stdout );
   #endif
               nextIn++ ;
               *(char *)nameEnd = 0 ;
               listItem_t *item = new listItem_t ;
               item->next_ = head_ ;
               item->name_ = nameStart ;
               item->value_ = valueStart ;
               head_ = item ;
            }
            else {
               fprintf( stderr, "unexpected end-of-data\n" );
               worked_ = false ;
               break;
            }
         }
         else {
            fprintf( stderr, "unexpected end-of-name\n" );
            worked_ = false ;
            break;
         }
      }
   } else if( (0xffffffff == crc) && ('\xff' == *nextIn) ){
      memset( dataBuf_, 0, dataMax_ );
      worked_ = true ;
   }
   else {
      fprintf( stderr, "U-Boot crc mismatch: read 0x%08lx, computed 0x%08lx\n", crc, computed );
   }

   if( worked_ ){
      dataMax_ = eof ;
      dataLen_ = nextIn-dataBuf_ ;
   }
   close( fdDev );
}

uBootParams_t::~uBootParams_t( void )
{
   while( head_ ){
      listItem_t *tmp = head_ ;
      head_ = tmp->next_ ;
      delete tmp ;
   }
   if( dataBuf_ ){
      delete [] dataBuf_ ;
   }
}

char const *uBootParams_t::find( char const *name ) const 
{
   uBootParamIter_t iter( *this );
   while( iter.valid() ){
      if( 0 == strcmp(name,iter.name()) ){
         return iter.value();
      }
      ++iter ;
   }
   return 0 ;
}

bool uBootParams_t::set( char const *name, char const *value )
{
   uBootParamIter_t iter( *this );
   while( iter.valid() ){
      if( 0 == strcmp(name,iter.name()) ){
         unsigned offset = iter.value()-dataBuf_ ;
         unsigned const oldLen = strlen(iter.value());
         unsigned const newLen = strlen(value);
         
         if( oldLen >= newLen ){
            strcpy((char *)iter.value(), value );
            return true ;
         }
         unsigned spaceAvail = dataMax_ - dataLen_ ;
         if( spaceAvail > newLen ){
            char *newValue = (char *)dataBuf_ + dataLen_ ;
            strcpy( newValue, value );
            iter.changeValue( newValue );
            dataLen_ += newLen + 1 ;
         }
         else {
            fprintf( stderr, "Insufficient space: need %u have %u\n", newLen+1, spaceAvail );
            return false ;
         }
         return true ;
      }
      ++iter ;
   }

   // value not found
   unsigned spaceAvail = dataMax_ - dataLen_ ;
   unsigned nameLen = strlen(name);
   unsigned valueLen = strlen(value);
   if( spaceAvail > nameLen+valueLen+2 ){
      listItem_t *item = new listItem_t ;
      item->next_ = head_ ;
      char *nextOut = (char *)dataBuf_ + dataLen_ ;
      memcpy(nextOut, name, nameLen);
      item->name_ = nextOut ;
      nextOut += nameLen ;
      *nextOut++ = '\0' ;
      strcpy(nextOut, value);
      item->value_ = nextOut ;
      nextOut += valueLen + 1 ;
      dataLen_ = nextOut - dataBuf_ ;
      head_ = item ;
      return true ;
   }
   return false ;
}

static bool isDevice(char const* devName)
{
    return ((devName[0]=='/') && (devName[1]=='d') && (devName[2]=='e') && (devName[3]=='v'));
}

bool uBootParams_t::save( char const *fileName )
{
   bool worked = false ;
   int fd = open( fileName, O_WRONLY );
   if( 0 <= fd )
   {
      char *const writeable = new char [dataMax_];
      memset( writeable, 0, dataMax_ );
      uLong crc ;
      char *nextOut = writeable + sizeof(crc);
      listItem_t *item = head_ ;
      while( item ){
         unsigned len = strlen(item->name_);
         memcpy(nextOut,item->name_,len);
         nextOut += len ;
         *nextOut++ = '=' ;
         len = strlen(item->value_);
         strcpy(nextOut,item->value_);
         nextOut += len + 1 ; // skip NULL
         item = item->next_ ;
      }
      assert(nextOut < writeable+dataMax_);
      crc = crc32( 0, (Bytef *)writeable+sizeof(crc), dataMax_-sizeof(crc));
      memcpy(writeable,&crc,sizeof(crc));

      bool bDevice = isDevice(fileName);
      if (bDevice) {
         mtd_info_t meminfo;
         if( 0 == ioctl( fd, MEMGETINFO, (unsigned long)&meminfo) )
         {
            if( meminfo.erasesize == dataMax_ )
            {
               erase_info_t erase;
               erase.start = 0 ;
               erase.length = meminfo.erasesize;
               if( 0 == ioctl( fd, MEMERASE, (unsigned long)&erase) )
               {
                  if( 0 == (unsigned)lseek( fd, 0, SEEK_SET ) )
                  {
                     unsigned numWritten = write( fd, writeable, dataMax_ );

                     worked = (numWritten == dataMax_ );
                  }
                  else
                     perror( "lseek3" );
               }
               else
                  fprintf( stderr, "erase error %m\n" );
            }
            else
               fprintf( stderr, "erase size mismatch: rv %u, mi %u\n", dataMax_, meminfo.erasesize );
         }
         else
            perror( "MEMGETINFO" );
      } else {
            fprintf( stderr, "save to file here\n" );
      }
      delete [] writeable ;
      close( fd );
   }
   else
      perror( fileName );

   return worked ;
}

#ifdef STANDALONE

static void dump( uBootParams_t const &params )
{
   uBootParamIter_t iter( params );
   while( iter.valid() ){
      printf( "%s -->\t%s\n", iter.name(), iter.value() );
      ++iter ;
   }
}

int main( int argc, char const * const argv[] ){
   if( 2 <= argc ){
      uBootParams_t params( argv[1] );
      if( params.worked() ){
         if( 2 == argc ){
            dump( params );
         }
         else if( 3 == argc ){
            char const *value = params.find(argv[2]);
            if( value )
               printf( "%s\n", value );
            else
               fprintf( stderr, "%s: not found\n", argv[2] );
         }
         else if( 4 == argc ){
            if( params.set( argv[2], argv[3] ) ){
               if( params.save( argv[1] ) ){
                  printf( "%s\n", argv[3] );
               }
               else
                  fprintf( stderr, "Error saving data\n" );
            }
            else
               fprintf( stderr, "Error changing value %s to %s\n", argv[2], argv[3] );
         }
         else
            fprintf( stderr, "Usage: %s /dev/mtdX [name [value]]\n", argv[0] );
      }
      else
         fprintf( stderr, "Error parsing variables from %s\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s /dev/mtdX [name [value]]\n", argv[0] );

   return 0 ;
}

#endif
