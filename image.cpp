/*
 * Module image.cpp
 *
 * This module defines the image_t methods
 * as declared in image.h
 *
 *
 * Change History : 
 *
 * $Log: image.cpp,v $
 * Revision 1.1  2003-10-18 19:16:16  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "image.h"

image_t :: ~image_t( void )
{
   unload();
}

void image_t :: unload( void )
{
   if( pixData_ )
      delete [] (unsigned short *) pixData_ ;
   if( alpha_ )
      delete [] (unsigned char *) alpha_ ;
   pixData_ = alpha_ = 0 ;
}
