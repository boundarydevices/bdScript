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
 * Revision 1.2  2006-10-25 23:28:07  ericn
 * -add typeName() utility routine
 *
 * Revision 1.1  2003/10/18 19:16:16  ericn
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

static char const *typeNames_[] = {
   "unknown"
 , "JPEG"
 , "PNG"
 , "GIF"
};

char const *image_t::typeName( type_e type )
{
   if( (unsigned)type > imgGIF )
      type = unknown ;
   return typeNames_[type];
}
