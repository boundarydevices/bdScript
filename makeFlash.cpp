/*
 * Program makeFlash.cpp
 *
 * This program will produce a flash image from a set of parts.
 *
 * Parts are specified on the command line in much the same form as
 * the kernel command line options for mtdparts:
 *
 *    256k(ub:u-boot/u-boot.bin),128k(ubparam),1664k(kernel:linux-neon-2.6/arch/arm/boot/uImage),30592k(rootfs:userland/jffs2.img),128k(param)
 *
 * If you look carefully, this __is__ the same form used by MTD except 
 * that some of the items in the list contain :filename after the name
 * of the MTD partition. 
 * 
 * If the filename is present, this program will attempt to read
 * and copy the specified file to the start of the partition.
 *
 * In either case, this program will fill in the remainder of each
 * partition with 0xFFs, up until the last partition with a filename
 * specified. This program will truncate the image at that point, 
 * presuming that the tool used to write the image to flash will 
 * pad with FFs as necessary.
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <zlib.h>

#define ONEMB (1<<20)

struct triplet_t {
   unsigned          size ;
   char              name[32];
   char              fileName[512];
   struct triplet_t *next ;
};

struct triplet_t *parseTriplet( char const *cTriplet )
{
   char const *match = strchr( cTriplet, '(' );
   if( match && ( match > cTriplet ) && ( match < cTriplet + 20 ) ){
      char cSize[30];
      unsigned len = match-cTriplet;
      
      strncpy(cSize,cTriplet,len);
      cSize[len] = 0 ;

      struct triplet_t trip ;
      char *endptr ;
      trip.size = strtoul(cSize,&endptr,0);
      if( trip.size ){
         if( endptr && *endptr ){
            if( 'k' == *endptr ){
               trip.size *= (1<<10);
            } else if( 'm' == *endptr ){
               trip.size *= (1<<20);
            }
         } // terminator not NULL

         char const *nameStart = match + 1 ;
         char matchChar = ':' ;
         match = strchr(nameStart, matchChar );
         if( !match ){
            matchChar = ')' ;
            match = strchr(nameStart, matchChar );
         }
         if( match && (match > nameStart) && (match < nameStart+20)){
            len = match-nameStart ;
            strncpy(trip.name, nameStart, len);
            trip.name[len] = '\0' ;

            if( ':' == matchChar ){
               char const *fileNameStart = match+1 ;
               match = strchr(fileNameStart, ')');
               if( match && (match > fileNameStart) && (match-fileNameStart<sizeof(trip.fileName)) ){
                  len = match-fileNameStart ;
                  strncpy(trip.fileName, fileNameStart, len);
                  trip.fileName[len] = '\0' ;
                  struct stat st ;
                  char const *fileName = trip.fileName ;
                  if( '*' == fileName[0] )
                     fileName++ ;
                  if( (0 == stat(fileName, &st))
                      &&
                      (0 < st.st_size) ){
                     if( st.st_size > trip.size ){
                        fprintf( stderr, "partition %s: %d exceeds maximum %u bytes\n",
                                 trip.name, st.st_size, trip.size );
                        trip.size = 0 ; // flag failure
                     }
                  }
                  else {
                     perror( fileName );
                     trip.size = 0 ; // flag failure
                  }
               }
               else
                  trip.size = 0 ; // flag failure
            }
            else
               trip.fileName[0] = '\0' ;

            if( trip.size ){
               return new triplet_t(trip);
            }
         } // found end of name
      } // have valid size
   } // found open paren
   
   return 0 ;
}

int main( int argc, char *argv[] )
{
   if( 3 == argc ){
      char const *outFileName = argv[1];
      FILE *fOut = fopen( outFileName, "wb" );
      struct triplet_t *trips = 0 ;
      struct triplet_t *endTrip = 0 ;
      if( fOut ){
         unsigned totalSize = 0 ;
         unsigned numTrips = 0 ;
         unsigned lastData = 0 ;
         char *cTriplet = strtok(argv[2],"," );
         while( cTriplet ){
            struct triplet_t *trip = parseTriplet( cTriplet );
            if( trip ){
               if( 0 == trips ){
                  trips = endTrip = trip ;
               }
               else {
                  endTrip->next = trip ;
                  endTrip = trip ;
                  trip->next = 0 ;
               }
               if( trip->fileName[0] ){
                  lastData = numTrips ;
               }
               ++numTrips ;
               totalSize += trip->size ;
            } else {
               fprintf( stderr, "Error parsing triplet <%s>\n", cTriplet );
               return -1 ;
            }
            cTriplet = strtok( 0, "," );
         }
         printf( "%u partitions, last with data is [%u]\n", numTrips, lastData );
         printf( "total size is %u, %uM\n", totalSize, (totalSize+((1<<20)-1)) >> 20 );

         // allocate some FFFFs for writing
         char *const ffs = new char [ONEMB];
         memset( ffs, -1, ONEMB );

         unsigned t ;
         unsigned long pos = 0 ;
         for( t = 0 ; t <= lastData ; t++ ){
            assert( 0 != trips );
            struct triplet_t *trip = trips ;
            trips = trip->next ;

            int spaceLeft = (int)trip->size ;
            printf( "%08lX %08lx %-10s ", ftell(fOut), trip->size, trip->name );
            if( trip->fileName[0] ){
               printf( "%s", trip->fileName );
               if( '*' != trip->fileName[0] ){
                  FILE *fIn = fopen(trip->fileName, "rb");
                  if( fIn ){
                     char inBuf[ONEMB];
                     int numRead ;
                     while( 0 < (numRead = fread( inBuf, 1, sizeof(inBuf), fIn)) ){
                        fwrite(inBuf,numRead,1,fOut);
                        spaceLeft -= numRead ;
                        assert(0 <= spaceLeft);
                     }
                     fclose( fIn );
                  } else {
                     perror( trip->fileName );
                     return -1 ;
                  }
               } else {
                  fprintf( stderr, "Process U-Boot parameters here\n" );
                  char *const ubData = new char [trip->size];
                  FILE *fIn = fopen(trip->fileName+1, "rb");
                  if( fIn ){
                     char *next = ubData + 4 ;
                     int numRead = fread( next, 1, trip->size-4, fIn );
                     if( 0 < numRead ){
// fwrite(next,1,numRead,stdout);
                        char *const ubEnd = ubData+numRead ;
                        while( 0 != (next=(char *)memchr(next,'\n',ubEnd-next)) ){
                           *next++ = '\0' ;
                        }
                        next = ubData+4 ;
                        unsigned long crc = crc32(0,(Bytef *)next,trip->size-4);
                        memcpy(ubData,&crc,sizeof(crc));
                        int numWritten = fwrite(ubData,1,trip->size,fOut);
                     }
                     else {
                        perror( trip->fileName+1 );
                        return -1 ;
                     }
                     fclose(fIn);
                  } else {
                     perror( trip->fileName );
                     return -1 ;
                  }
                  delete [] ubData ;
               } // U-Boot parameter section
            } // fill with some data
            if( t < lastData ){
               while( 0 < spaceLeft ){
                  unsigned count = (spaceLeft > ONEMB) ? ONEMB : spaceLeft ;
                  int numWritten = fwrite(ffs, 1, count,fOut);
                  assert( 0 <= numWritten );
                  spaceLeft -= numWritten ;
               }
            }
            printf( "\n" );
            pos += trip->size ;
         }
         fclose( fOut );

         for( ; t < numTrips ; t++ ){
            assert( 0 != trips );
            struct triplet_t *trip = trips ;
            trips = trip->next ;
            printf( "%08lX %08lx %-10s \n", pos, trip->size, trip->name );
            pos += trip->size ;
         }
      }
      else
         perror( outFileName );
   }
   else
      fprintf( stderr
               , "Usage: %s outputFile flashparts\n"
                 "flashparts is a set of size(name[,filename) triplets separated by commas\n"
                 "size may be specified in k(ilobytes) or m(egabytes)\n"
               , argv[0] );

   return 0 ;
}
