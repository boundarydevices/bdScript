/*
 * Module imgToPNG.cpp
 *
 * This module defines the imageToPNG() routine
 * as declared in imgToPNG.h
 *
 *
 * Change History : 
 *
 * $Log: imgToPNG.cpp,v $
 * Revision 1.2  2006-08-16 02:30:06  ericn
 * -use alpha if present
 *
 * Revision 1.1  2006/05/07 15:41:17  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "imgToPNG.h"
#include <png.h>
#include <stdio.h>
#include <obstack.h>
#include <assert.h>
#include <stdlib.h>
#include "fbDev.h"
#include <memory>

struct chunk_t {
	char	       *data_ ;
	struct chunk_t *prev_ ;
	unsigned 	max_ ;
	unsigned 	len_ ;
	unsigned        total_ ;
};

unsigned const PAGESIZE = 4096 ;
unsigned const DATASIZE = PAGESIZE-sizeof(struct chunk_t);

static chunk_t *newChunk( chunk_t *prev, unsigned prevTotal )
{
	chunk_t *ch = (chunk_t *)new char [PAGESIZE];
	ch->data_  = (char *)(ch + 1);
	ch->prev_  = prev ;
	ch->max_   = DATASIZE ;
	ch->len_   = 0 ;
	ch->total_ = prevTotal ;
	
	return ch ;
}

static void freeChunks( chunk_t *last )
{
	while( last )
	{
		chunk_t *prev = last ;
		last = prev->prev_ ;
		delete [] (char *)prev ;
	}
}

static chunk_t *get_png_chunk( png_structp png_ptr )
{
	return (chunk_t *)png_get_io_ptr(png_ptr);
}

static void write_png_data
	( png_structp 	png_ptr
        , png_bytep 	data
	, png_size_t 	length )
{
	chunk_t *ch = get_png_chunk(png_ptr);
	if( 0 == ch )
		ch = newChunk( ch, 0 );
	while( length )
	{
		if( ch->len_ == ch->max_ )
		{
			ch = newChunk(ch, ch->total_);
		}
		unsigned left = ch->max_ - ch->len_ ;
		if( left > length )
			left = length ;
		memcpy( ch->data_+ch->len_, data, left );
		data += left ;
		length -= left ;
		ch->len_ += left ;
		ch->total_ += left ;
	}
	
	png_init_io(png_ptr,(FILE *)ch);
}

static void flush_png_data( png_structp png_ptr )
{
	chunk_t *ch = get_png_chunk(png_ptr);
	if( ch ){
		freeChunks(ch);
		png_init_io(png_ptr,0);
	}
}

static void collapse_png_data( 
	chunk_t *last,
	void   *&data,
	unsigned &len
)
{
	len = last->total_ ;

	unsigned char *const bytes = (unsigned char *)malloc(last->total_);
	unsigned char *tail = bytes+last->total_ ;
	while( last )
	{
		tail -= last->len_ ;
		assert( tail >= bytes );

		memcpy( tail, last->data_, last->len_ );

		last = last->prev_ ;
		assert( ( 0 != last ) || ( tail == bytes ) );
	}

	assert( tail == bytes );
	data = tail ;
}

static void rgb16_to_24( 
	unsigned short const *inRow,
	unsigned	             width,
   unsigned char const  *alpha,
	unsigned char        *outRow
)
{
	unsigned char *nextOut = outRow ;
	for( unsigned col = 0 ; col < width ; col++ )
	{
		unsigned short const rgb16 = *inRow++ ;
		*nextOut++ = fbDevice_t::getRed(rgb16);
		*nextOut++ = fbDevice_t::getGreen(rgb16);
		*nextOut++  =fbDevice_t::getBlue(rgb16);
      unsigned char const a = (0 != alpha) ? *alpha++ : 255 ;
      *nextOut++ = a ;
	}
}
	

bool imageToPNG( image_t const &img,
		 void const   *&pngData,
		 unsigned      &size )
{
	pngData = 0 ;
	size    = 0 ;
	
	png_structp png_ptr = png_create_write_struct
		(PNG_LIBPNG_VER_STRING, (png_voidp)0, 0, 0 );
	if( png_ptr ){
		png_infop info_ptr = png_create_info_struct(png_ptr);
		if(info_ptr){

			if( 0 == setjmp( png_jmpbuf( png_ptr ) ) ) {
				png_set_write_fn( png_ptr, 0, write_png_data, flush_png_data );
				png_set_IHDR( png_ptr, info_ptr, img.width_, img.height_, 8,
						PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, 
						PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
				png_write_info( png_ptr, info_ptr );
				
				unsigned short const *nextIn = (unsigned short const *)img.pixData_ ;
				unsigned char *const outRow = new unsigned char [img.width_*4];

				for( unsigned row = 0; row < img.height_ ; row++, nextIn += img.width_ )
				{
					rgb16_to_24(nextIn,img.width_,(unsigned char *)img.alpha_,outRow);
					png_write_row( png_ptr, outRow );
				}

				delete [] outRow ;

				chunk_t *ch = get_png_chunk(png_ptr);
				if( ch ){
					void *outData ;
					collapse_png_data(ch,outData,size);
					pngData = outData ;
					flush_png_data(png_ptr);
				}
				
				png_write_end(png_ptr, info_ptr);

			}

			png_destroy_info_struct(png_ptr, &info_ptr);

		}
		else
                        png_destroy_write_struct(&png_ptr, 0);

	}

	return (0 != pngData);
}


#ifdef __STANDALONE__

#include "fbDev.h"
#include "imgFile.h"

int main( int argc, char const * const argv[] )
{
	fbDevice_t &fb = getFB();
	image_t img ;
	memset( &img, 0, sizeof( img ) );
	
	if( 1 < argc )
	{
		if( imageFromFile( argv[1], img ) )
			printf( "%s: %u x %u pixels\n", argv[1], img.width_, img.height_ );
		else
			perror( argv[1] );
		
	}
	else
	{
		img.pixData_ = fb.getRow(0);
		img.width_   = fb.getWidth();
		img.height_  = fb.getHeight();
		img.alpha_   = 0 ;
	}

	if( img.pixData_ )
	{
		void const *outData ;
		unsigned outSize ;
		if( imageToPNG( img, outData, outSize ) )
		{
			printf( "%u bytes of output\n", outSize );
			char const *outFile = ( 2 < argc ) ? argv[2] : "/tmp/img.png" ;
			FILE *fOut = fopen( outFile, "wb" );
			if( fOut )
			{
				fwrite( outData, 1, outSize, fOut );
				fclose( fOut );
				printf( "%u bytes written to %s\n", outSize, outFile );
			}
			else
				perror( outFile );
			free((void *)outData);
		}
		else
			printf( "Error converting image\n" );
	}

	if( img.pixData_ == fb.getRow(0) )
		img.pixData_ = 0 ; // don't let destructor delete		

	return 0 ;
}

#endif
