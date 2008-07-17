#include <stdio.h>
#include "memFile.h"
#include <zlib.h>
#include <string.h>

struct rle_t {
   unsigned char copy ; // copy this number
   unsigned char zero ; // zero this number
   // 'copy' bytes of data go here
};

class compression_t {
public:
   virtual ~compression_t( void ){}
   virtual unsigned bytesOut( void ) const = 0 ;
   virtual char const *bytes( void ) const = 0 ;
   virtual char const *fileExt( void ) const = 0 ;
private:
};

class zlibCompression_t : public compression_t {
public:
   zlibCompression_t( char const *xorred, unsigned bytes );
   virtual ~zlibCompression_t( void ){ if( outBuf_ != inBuf_ ) delete [] outBuf_ ; }

   virtual unsigned bytesOut( void ) const { return outBytes_ ; }
   virtual char const *bytes( void ) const { return outBuf_ ; }

   virtual char const *fileExt( void ) const { return "gz" ; }

   int         result_ ;
   unsigned    outBytes_ ;
   char const *inBuf_ ;
   char       *outBuf_ ;
};

class noZerosXorred_t : public compression_t {
public:
   noZerosXorred_t( char const *xorred, unsigned bytes );
   virtual ~noZerosXorred_t( void ){ if( outBuf_ != inBuf_ ) delete [] outBuf_ ; }

   virtual unsigned bytesOut( void ) const { return outBytes_ ; }
   virtual char const *bytes( void ) const { return outBuf_ ; }
   virtual char const *fileExt( void ) const { return "rle" ; }

   int         result_ ;
   unsigned    outBytes_ ;
   char const *inBuf_ ;
   char       *outBuf_ ;
};


class copyCompressor_t : public compression_t {
public:
   copyCompressor_t( char const *xorred, unsigned bytes );
   virtual ~copyCompressor_t( void ){}

   virtual unsigned bytesOut( void ) const { return outBytes_ ; }
   virtual char const *bytes( void ) const { return inBuf_ ; }
   virtual char const *fileExt( void ) const { return "xor" ; }

   unsigned    outBytes_ ;
   char const *inBuf_ ;
};

#define NUM_COMPRESSORS 3

int main( int argc, char const * const argv[] )
{
   if( 1 == argc ){
      fprintf( stderr, "Usage: %s infile infile ...\n", argv[0] );
      return -1 ;
   }

   memFile_t *prev = new memFile_t( argv[1] );
   if( prev->worked() ){
      unsigned long totalBytes = 0 ;
      char *const xorred = new char [prev->getLength()];
      unsigned long totalZipped = 0 ;
      unsigned long maxZipped = 0 ;
      unsigned maxZipFrame = 0 ;
      
      for( unsigned i = 2 ; i < argc ; i++ ){
         memFile_t *cur = new memFile_t( argv[i] );
         if( !cur->worked() ){
            perror(argv[i]);
            return -1 ;
         }
         if( cur->getLength() != prev->getLength() ){
            printf( "size mismatch at frame %u\n", i );
            return -1 ;
         }
         totalBytes += prev->getLength();
         char const *r = (char const *)prev->getData();
         char const *f = (char const *)cur->getData();
         
         for( unsigned j = 0 ; j < cur->getLength(); j++ ){
            char const x = *r++ ^ *f++ ;
            bool isSame = (0==x);
            xorred[j] = x ;
         }

         compression_t *compressors[NUM_COMPRESSORS] = {
            new zlibCompression_t( xorred, prev->getLength() )
         ,  new noZerosXorred_t( xorred, prev->getLength() )
         ,  new copyCompressor_t( xorred, prev->getLength() )
         };

         totalZipped += compressors[0]->bytesOut();
         printf( "%s:%lu", argv[i], prev->getLength(), compressors[0]->bytesOut() );

         delete prev ;
         prev = cur ;

         for( unsigned c = 0 ; c < NUM_COMPRESSORS ; c++ ){
            char outFile[512];
            snprintf( outFile, sizeof(outFile)-1, "%s.%s", argv[i], compressors[c]->fileExt() );
            printf( ":%u %s", compressors[c]->bytesOut(), compressors[c]->fileExt() );
            FILE *fOut = fopen( outFile, "wb" );
            fwrite( compressors[c]->bytes(), compressors[c]->bytesOut(), 1, fOut );
            fclose( fOut );
            delete compressors[c];
         }
         printf( "\n" );
      }
      totalBytes += prev->getLength();
   }
   else
      perror( argv[1] );

   return 0 ;
}

zlibCompression_t::zlibCompression_t( char const *xorred, unsigned numInBytes )
{
   outBuf_ = new char [numInBytes];
   inBuf_ = xorred ;
   outBytes_ = numInBytes ;
   result_ = 0 ;

   z_stream strm ;
   memset( &strm, 0, sizeof(strm));
   strm.next_in = (Bytef *)xorred ;
   strm.avail_in = numInBytes ;
   strm.next_out = (Bytef *)outBuf_ ;
   strm.avail_out = numInBytes ;

   if( Z_OK == (result_ = deflateInit( &strm, Z_DEFAULT_COMPRESSION )) ){
      if( Z_STREAM_END == ( result_ = deflate( &strm, Z_FINISH) ) ){
         result_ = Z_OK ;
      }
      else if( Z_OK == result_ ){
         result_ = Z_BUF_ERROR ;
      } // output bigger than input
   }
   
   if( Z_OK == result_ ){
      outBytes_ = strm.total_out ;
   }
   else {
      delete [] outBuf_ ;
      outBuf_ = (char *)inBuf_ ;
      outBytes_ = numInBytes ;
   }
}

noZerosXorred_t::noZerosXorred_t( char const *xorred, unsigned numInBytes )
{
   inBuf_ = xorred ;
   outBuf_ = new char [numInBytes];
   outBytes_ = 0 ;

   result_ = 0 ;
   unsigned numLeft = numInBytes ;
   while( (0 < numLeft) && (outBytes_ < numInBytes ) ){
      char const *start = xorred ;
      unsigned char num = 0 ;
      while( (0 < numLeft) && (255 > num) && (0 != *xorred ) ){
         xorred++ ;
         numLeft-- ;
         num++ ;
      }
      outBuf_[outBytes_++] = (char)num ;
      if( num + outBytes_ >= numInBytes ){
         outBytes_ = numInBytes ;
         break;
      }
      memcpy( outBuf_+ outBytes_, xorred - num, num );
      outBytes_ += num ;

      num = 0 ;
      while( (0 < numLeft) && (255 > num) && (0 == *xorred ) ){
         xorred++ ;
         numLeft-- ;
         num++ ;
      }
      outBuf_[outBytes_++] = (char)num ;
   }
   
   if( outBytes_ < numInBytes ){
   }
   else {
      delete [] outBuf_ ;
      outBuf_ = (char *)inBuf_ ;
      outBytes_ = numInBytes ;
   }
}

copyCompressor_t::copyCompressor_t( char const *xorred, unsigned bytes )
   : inBuf_( xorred )
   , outBytes_( bytes )
{
}

