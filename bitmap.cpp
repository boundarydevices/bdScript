/*
 * Module bitmap.cpp
 *
 * This module defines the methods of the bitmap_t
 * class as described in bitmap.h
 *
 * Change History : 
 *
 * $Log: bitmap.cpp,v $
 * Revision 1.1  2004-07-04 21:35:14  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "bitmap.h"
//#include "hexDump.h"
#include <stdio.h>

bitmap_t :: bitmap_t
   ( unsigned char *bits,
     unsigned       bitWidth,
     unsigned       bitHeight )
   : bits_( bits )
   , bitWidth_( bitWidth )
   , bitHeight_( bitHeight )
   , bytesPerRow_( bytesPerRow(bitWidth) )
{
}

void bitmap_t::setBits
   ( unsigned char *bitStart,
     unsigned       startOffs,
     unsigned       count )
{
   static unsigned char const startMasks[] = {
      '\xff',
      '\x7f',
      '\x3f',
      '\x1f',
      '\x0f',
      '\x07',
      '\x03',
      '\x01'
   };
//   printf( "set: %p, %u, %u\n", bitStart, startOffs, count );
//   dumpHex( "before set:", bitStart, (startOffs+count-1+7)/8 );
   unsigned char *next = bitStart+(startOffs/8);
   unsigned char *stop = bitStart+((startOffs+count-1)/8);
   unsigned char const lead = ( startOffs & 7 );
   unsigned char const tail = ( startOffs+count-1 ) & 7 ;
   unsigned char mask = startMasks[lead];

   while( next <= stop )
   {
      if( next == stop )
         mask &= ~(startMasks[tail]);
      *next++ |= mask ;
      mask = '\xff' ;
   }
//   dumpHex( "after set:", bitStart, (startOffs+count-1+7)/8 );
}

void bitmap_t::clearBits
   ( unsigned char *bitStart,
     unsigned       startOffs,
     unsigned       count )
{
   static unsigned char const startMasks[] = {
      '\x00',
      '\x80',
      '\xc0',
      '\xe0',
      '\xf0',
      '\xf8',
      '\xfc',
      '\xfe'
   };

//   printf( "clear: %p, %u, %u\n", bitStart, startOffs, count );
//   dumpHex( "before clear:", bitStart, (startOffs+count-1+7)/8 );
   unsigned char *next = bitStart+(startOffs/8);
   unsigned char *stop = bitStart+((startOffs+count-1)/8);
   unsigned char const lead = ( startOffs & 7 );
   unsigned char const tail = ( startOffs+count-1 ) & 7 ;
   unsigned char mask = startMasks[lead];

   while( next <= stop )
   {
      if( next == stop )
         mask |= ~(startMasks[tail]);
      *next++ &= mask ;
      mask = '\x00' ;
   }
//   dumpHex( "after clear:", bitStart, (startOffs+count-1+7)/8 );
}

void bitmap_t::rect
   ( unsigned x, unsigned y,
     unsigned w, unsigned h,
     bool setNotClear )
{
   if( x < bitWidth_ )
   {
      unsigned const wMax = bitWidth_-x ;
      if( wMax < w )
         w = wMax ;
      if( y < bitHeight_ )
      {
         void (*bitChange)( unsigned char *bitStart,
                            unsigned       startOffs,
                            unsigned       count );
         if( setNotClear )
            bitChange = setBits ;
         else
            bitChange = clearBits ;
         
         unsigned hMax = bitHeight_-y ;
         if( hMax < h )
            h = hMax ;

         unsigned char *startOfRow = bits_+(y*bytesPerRow_);
         unsigned const y2 = y + h ;

         for( ; y < y2 ; y++ )
         {
            bitChange( startOfRow, x, w );
            startOfRow += bytesPerRow_ ;
         }
      }
   }
}

void bitmap_t::line
   ( unsigned x1, unsigned y1,
     unsigned x2, unsigned y2,
     unsigned penWidth,
     bool setNotClear )
{
   if( x1 == x2 )
   {
      rect( x1, y1, penWidth, y2-y1, setNotClear );
   }
   else if( y1 == y2 )
   {
      printf( "horizontal line at y: %u, x:%u-%u, width:%u, %s\n", 
              y1, x1, x2, penWidth, setNotClear ? "set" : "clear" );
      rect( x1, y1, x2-x1, penWidth, setNotClear );
   }
   else
   {
   } // diagonals not supported (yet)
}

void bitmap_t::bltFrom
   ( unsigned char       *bitStart,
     unsigned             startOffs,
     unsigned char const *src,
     unsigned             count )
{
   unsigned char *nextOut = bitStart+(startOffs/8);
   unsigned char outMask = 0x80 >> (startOffs&7);
   unsigned char inMask  = 0x80 ;
   unsigned char inChar  = *src++ ;
   unsigned char outChar = *nextOut ; // preload

   while( 0 < count-- )
   {
      bool set = inChar & inMask ;
      if( set )
         outChar |= outMask ;
      else
         outChar &= ~outMask ;
      inMask >>= 1 ;
      if( 0 == inMask )
      {
         inMask = 0x80 ;
         inChar = *src++ ;
      }
      outMask >>= 1 ;
      if( 0 == outMask )
      {
         *nextOut++ = outChar ;
         outChar = *nextOut ;
         outMask = 0x80 ;
      }
   }
   
   if( 0x80 != outMask )
      *nextOut = outChar ;
}

