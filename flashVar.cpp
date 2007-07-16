/*
 * Module flashVar.cpp
 *
 * This module defines the readFlashVar() and writeFlashVar()
 * routines as declared in flashVar.h
 *
 *
 * Change History : 
 *
 * $Log: flashVar.cpp,v $
 * Revision 1.14  2007-07-16 18:36:15  ericn
 * -don't re-write duplicate variables
 *
 * Revision 1.13  2007/07/15 21:45:32  ericn
 * -added -erase command for testing
 *
 * Revision 1.12  2007/05/09 15:34:43  ericn
 * -change to make flashVar useful in scripts
 *
 * Revision 1.11  2006/02/13 21:12:49  ericn
 * -use 2.6 include files by default
 *
 * Revision 1.10  2006/12/01 18:37:43  tkisky
 * -allow variables to be saved to file
 *
 * Revision 1.9  2006/03/29 18:47:39  tkisky
 * -minor
 *
 * Revision 1.8  2005/11/22 17:41:58  tkisky
 * -fix write FLASHVARDEV use
 *
 * Revision 1.7  2005/11/16 14:57:42  ericn
 * -use FLASHVARDEV environment variable if present
 *
 * Revision 1.6  2005/11/16 14:49:44  ericn
 * -use debugPrint, not printf and comments
 *
 * Revision 1.5  2005/11/06 00:49:24  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.4  2005/08/12 04:18:43  ericn
 * -allow compile against 2.6
 *
 * Revision 1.3  2004/02/07 12:14:34  ericn
 * -allow zero-length values
 *
 * Revision 1.2  2004/02/03 06:11:12  ericn
 * -allow empty strings
 *
 * Revision 1.1  2004/02/01 09:53:18  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "flashVar.h"
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

//#define DEBUGPRINT
#include "debugPrint.h"

static char const deviceName_[] = {
   "/dev/mtd2"
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

static unsigned const pageSize = 4096 ;

// returns 0 if successful, non-zero on errors
static int flashVarOffset(int fdMTD, unsigned &offset, unsigned &maxSize){
   int rval = -1 ;
   mtd_info_t meminfo;
   if( ioctl( fdMTD, MEMGETINFO, (unsigned long)&meminfo) == 0)
   {
      debugPrint( "flags       0x%lx\n", meminfo.flags ); 
      debugPrint( "size        0x%lx\n", meminfo.size ); 
      debugPrint( "erasesize   0x%lx\n", meminfo.erasesize ); 
      //	    debugPrint( "oobblock    0x%lx\n", meminfo.oobblock ); 
      debugPrint( "oobsize     0x%lx\n", meminfo.oobsize ); 
      debugPrint( "ecctype     0x%lx\n", meminfo.ecctype ); 
      debugPrint( "eccsize     0x%lx\n", meminfo.eccsize ); 
      debugPrint( "numSectors  0x%lx\n", meminfo.size / meminfo.erasesize );
      
      offset = meminfo.size-meminfo.erasesize ;
      
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

class readVars_t {
public:
   readVars_t( void );
   ~readVars_t( void );

   bool worked( void ) const { return worked_ ; }
   char const *startChar( void ) const { return image_ ; }
   unsigned    bytesUsed( void ) const { return bytesUsed_ ; }
   unsigned    maxSize( void ) const { return maxSize_ ; }

//
//
//
   bool        worked_ ;
   char const *image_ ;
   unsigned    bytesUsed_ ;
   unsigned    maxSize_ ;
};

readVars_t :: readVars_t( void )
   : worked_( false )
   , image_( 0 )
   , bytesUsed_( 0 )
   , maxSize_( 0 )
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
      if (bDevice && ( 0 == flashVarOffset(fd, offset, maxSize_) )){
         debugPrint( "flashVar offset 0x%x, max size %u\n", offset, maxSize_ );
      } else {
	maxSize_ = 0x40000;
      }
      if (maxSize_) {
            image_ = (char *)malloc( maxSize_ );
            debugPrint( "maxSize 0x%x\n", maxSize_ );

            char *nextIn = (char *)image_ ;
            while( bytesUsed_ < maxSize_ )
            {
               int numRead = read( fd, nextIn, pageSize );
	       if (numRead>0) {
                  bytesUsed_ += numRead ;
                  nextIn    += numRead ;
	       }
               if( pageSize == (unsigned)numRead )
               {
                  if( '\xff' == nextIn[-1] )
                     break; // end of data
               }
               else
               {
                  if (bDevice) perror( "read" );
                  break;
               }
            }

#ifdef TESTREST
            unsigned bytesRead = bytesUsed_ ;
            char    *nextBlank = nextIn ;
            while( bytesRead < maxSize_ )
            {
               int numRead = read( fd, nextBlank, pageSize );
               if( pageSize == numRead )
               {
                  if( ( '\xff' == *nextBlank )
                      &&
                      ( 0 == memcmp( nextBlank, nextBlank+1, pageSize-1 ) ) )
                  {
                     bytesRead += pageSize ;
                     nextBlank += pageSize ;
                  }
                  else
                  {
                     fprintf( stderr, "non-blank at offset %u\n", bytesRead );
                     break;
                  }
               }
               else
               {
                  perror( "read2" );
                  break;
               }
            }
            
            if( bytesRead == maxSize_ )
               printf( "remainder of %u bytes is blank\n", maxSize_ );
            else
               fprintf( stderr, "something isn't blank at position %lu\n", bytesUsed_ );
#endif

            //
            // the tail of image_[0..bytesUsed_-1] is blank, walk until non-blank
            //
            while( 0 < bytesUsed_ )
            {
               if( '\xff' == nextIn[-1] )
               {
                  --nextIn ;
                  --bytesUsed_ ;
               }
               else
                  break;
            }

            worked_ = true ;
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
}

class varIter_t {
public:
   varIter_t( readVars_t const &rv );

   bool next( char const *&name, unsigned &nameLen,
              char const *&data, unsigned &dataLen );

private:
   readVars_t const &rv_ ;
   char const *const start_ ;
   char const *const end_ ;
   char const       *next_ ;
};

varIter_t::varIter_t( readVars_t const &rv )
   : rv_( rv )
   , start_( rv.worked() ? rv.startChar() : 0 )
   , end_( start_ + ( rv.worked() ? rv.bytesUsed() : 0 ) )
   , next_( start_ )
{
}

bool varIter_t::next
   ( char const *&name, unsigned &nameLen,
     char const *&data, unsigned &dataLen )
{
   name  = next_ ; nameLen = 0 ;
   data  = 0 ;     dataLen = 0 ;

   enum {
      nameChar,
      dataChar,
      done
   } state = nameChar ;

   while( ( done != state ) && ( next_ < end_ ) )
   {
      char const c = *next_++ ;

//debugPrint( "state %d, char %c <%02x>\n", state, c, c );
      if( nameChar == state )
      {
         if( '=' == c )
         {
            nameLen = next_ - name - 1 ;
            data    = next_ ;
            state = dataChar ;
         } // end of name
         else if( '\xff' == c )
         {
            state = done ;
         } // end of variables
      } // name char
      else
      {
         if( '\n' == c )
         {
            dataLen = next_ - data - 1 ;
            state = done ;
         }
         else if( '\xff' == c )
         {
            state = done ;
         }
      } // data char
   }

   if( ( 0 < nameLen ) && ( done == state ) )
   {
      return true ;
   }
   else
   {
      name = data = 0 ;
      nameLen = dataLen = 0 ;
      return false ;
   } // trash outputs
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


struct entry_t {
	char const *name ; 
	unsigned    nameLen ;
	char const *data ; 
	unsigned    dataLen ;
};
void writeEntries(int fd, entry_t* pe,int cnt,int totalExpected)
{
	int numWritten = 0 ;
	for ( int i=0; i < cnt; i++ ) {
		entry_t const &entry = pe[i];
		numWritten += write( fd, entry.name, entry.nameLen );
		numWritten += write( fd, "=", 1 );
		numWritten += write( fd, entry.data, entry.dataLen );
		numWritten += write( fd, "\n", 1 );
	}
	if( numWritten == totalExpected ) {
		printf( "wrote %u bytes of unique data\n", numWritten );
	} else  printf( "write error: %d of %u\n", numWritten, totalExpected );
}

//
// writes the specified flash variable with the specified value
//
void writeFlashVar( char const *name,
                    char const *value )
{
   readVars_t rv ;
   char const *devName = GetFlashDev();
   debugPrint( "opening flash device %s\n", devName );
   if( rv.worked() )
   {
      unsigned const nameLen = strlen( name );
      unsigned const dataLen = strlen( value );
      unsigned const avail = rv.maxSize() - rv.bytesUsed();
      unsigned int bDevice = IsDevice(devName);
      if (bDevice && ( avail > nameLen + dataLen + 2 ))
      {
         int fd = open( devName, O_WRONLY );
         if( 0 <= fd )
         {
            mtd_info_t meminfo;
            if( ioctl( fd, MEMGETINFO, (unsigned long)&meminfo) == 0)
            {
               if( meminfo.erasesize == rv.maxSize() )
               {
                  unsigned offset = meminfo.size-meminfo.erasesize+ rv.bytesUsed();
                  if( offset == (unsigned)lseek( fd, offset, SEEK_SET ) )
                  {
                     //
                     // build memory image of new entry
                     //
                     char *entryStart = (char *)rv.startChar() + rv.bytesUsed();
                     memcpy( entryStart, name, nameLen );
                     char *upd = entryStart + nameLen ;
                     *upd++ = '=' ;
                     memcpy( upd, value, dataLen );
                     upd += dataLen ;
                     *upd++ = '\n' ;
                     unsigned const entryLen = upd-entryStart ;
                     int const numWrote = write( fd, entryStart, entryLen );
                     if( (unsigned)numWrote == entryLen )
                     {
//                        printf( "wrote %u bytes of new entry\n", numWrote );
                     }
                     else
                        perror( "write" );
                  }
                  else
                     perror( "lseek2" );
               }
               else
                  fprintf( stderr, "erase size mismatch: rv %u, mi %u\n", rv.maxSize(), meminfo.erasesize );
            }
            else {
               perror( "MEMGETINFO" );
	    }

            close( fd );
         }
         else
            perror(devName);
      }
      else
      {
         fprintf( stderr, "Collapse vars\n" );
         
         // 
         // first, find number of entries by searching for newlines
         //
         unsigned maxEntries = 0 ;
         unsigned spaceLeft = rv.bytesUsed();
         char const *next = rv.startChar();
         while( 0 < spaceLeft )
         {
            void const *nl = memchr( next, '\n', spaceLeft );
            if( nl )
            {
               maxEntries++ ;
               unsigned offs = (char *)nl - next + 1 ;
               next      += offs ;
               spaceLeft -= offs ;
            }
            else
               break;
         }

         
         if ( 1 )
         {
            printf( "%u entries in list\n", maxEntries+1 );
            
            unsigned numUnique = 0 ;
            unsigned uniqueSize = 0 ;
            entry_t * const entries = (entry_t *)malloc(sizeof(entry_t)*(maxEntries+1) );
            memset( entries, 0, maxEntries*sizeof(entry_t) );

            varIter_t it( rv );

            entry_t nextEntry ;
            while( it.next( nextEntry.name, nextEntry.nameLen, nextEntry.data, nextEntry.dataLen ) )
            {
               unsigned i ;
               for( i = 0 ; i < numUnique ; i++ )
               {
                  if( ( entries[i].nameLen == nextEntry.nameLen )
                      &&
                      ( 0 == memcmp( entries[i].name, nextEntry.name, nextEntry.nameLen ) ) )
                  {
                     break;
                  }
               }
               if( i < numUnique )
               {
                  uniqueSize -= entries[i].dataLen ;
                  uniqueSize += nextEntry.dataLen ;

                  entries[i].data    = nextEntry.data ;
                  entries[i].dataLen = nextEntry.dataLen ;
               } // duplicate
               else
               {
                  entries[numUnique++] = nextEntry ;
                  uniqueSize += nextEntry.nameLen ;
                  uniqueSize += nextEntry.dataLen ;
               } // first timer
            } // for each entry in flash

            // 
            // now add original entry (remember why we're here)
            // 
            unsigned i ;
            for( i = 0 ; i < numUnique ; i++ )
            {
               if( ( entries[i].nameLen == nameLen )
                   &&
                   ( 0 == memcmp( entries[i].name, name, nameLen ) ) )
               {
                  break;
               }
            }

            if( i < numUnique )
            {
               uniqueSize -= entries[i].dataLen ;
               uniqueSize += dataLen ;
               entries[i].data    = value ;
               entries[i].dataLen = dataLen ;
            } // duplicate
            else
            {
               entries[i].name    = name ;
               entries[i].nameLen = nameLen ;
               entries[i].data    = value ;
               entries[i].dataLen = dataLen ;
               uniqueSize += nameLen ;
               uniqueSize += dataLen ;
               numUnique++ ;
            } // new entry 

            uniqueSize += 2*numUnique ;
            printf( "%u unique entries, %u bytes\n", numUnique, uniqueSize );

            if( uniqueSize < rv.maxSize() )
            {
printf( "saving %u bytes\n", uniqueSize );

               int fd = open( devName, O_WRONLY );
               if( 0 <= fd )
               {
		  if (bDevice) {
                     mtd_info_t meminfo;
                     if( 0 == ioctl( fd, MEMGETINFO, (unsigned long)&meminfo) )
                     {
                        if( meminfo.erasesize == rv.maxSize() )
                        {
                           unsigned offset = meminfo.size-meminfo.erasesize ;
                           erase_info_t erase;
                           erase.start = offset ;
                           erase.length = meminfo.erasesize;
                           if( 0 == ioctl( fd, MEMERASE, (unsigned long)&erase) )
                           {
                              if( offset == (unsigned)lseek( fd, offset, SEEK_SET ) )
                              {
				writeEntries(fd, entries,numUnique,uniqueSize);
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
		  } else {
			writeEntries(fd, entries,numUnique,uniqueSize);
		  }
      
                  close( fd );
               }
               else
                  perror(devName);
            }
            else
               fprintf( stderr, "Flash parameter area is full!\n" );

            free( entries );
         }
         else
            fprintf( stderr, "Error counting entries: why is size so big?\n" );
      }
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
                   ( 0 == flashVarOffset(fd,offset,maxSize) ) ){
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
            else if( 0 == strcmp( varName, "-fill" ) )
            {
               char const * const name = "TEST" ;
               unsigned const nameLen = strlen( name );
               char const * const value = "THis is a reaaaallllly realllllyy lllllllllllllllllllllllllllllllooooooooooooooooooooooooooooooooonnnnnnnnnnnnnnnnnnnnnnnnnnggggggggggggggg test" ;
               unsigned const valueLen = strlen( value );
               unsigned iterations = 0 ;
               {
                  long long start = tickMs();
                  readVars_t rv ;
                  long long end = tickMs();
                  if( rv.worked() )
                  {
                     iterations = (rv.maxSize()-rv.bytesUsed())
                                  /
                                  (nameLen+valueLen+2);
                  }
                  else
                     fprintf( stderr, "Error parsing flash variables\n" );
               } // limit scope of rv

               if( 0 < iterations )
               {
                  printf( "writing %u iterations of test data\n", iterations );
                  for( unsigned i = 0 ; i < iterations ; i++ )
                     writeFlashVar( name, value );
               }
               
               long long start = tickMs();
               readVars_t rv ;
               long long end = tickMs();
               if( rv.worked() )
               {
                  printf( "%u of %u bytes used (parsed in %lld ms)\n", 
                          rv.bytesUsed(), rv.maxSize(), end-start );
               }
               else
                  fprintf( stderr, "Error parsing flash variables\n" );
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
            long long start = tickMs();
            readVars_t rv ;
            long long end = tickMs();

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
