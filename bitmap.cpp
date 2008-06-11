/*
 * Module bitmap.cpp
 *
 * This module defines the methods of the bitmap_t
 * class as described in bitmap.h
 *
 * Change History : 
 *
 * $Log: bitmap.cpp,v $
 * Revision 1.6  2008-06-11 18:12:28  ericn
 * remove dependency on macros.h
 *
 * Revision 1.5  2004/11/16 07:29:33  tkisky
 * -add SetBox
 *
 * Revision 1.4  2004/10/28 21:27:40  tkisky
 * -ClearBox function added
 *
 * Revision 1.3  2004/08/01 18:00:59  ericn
 * -conditional debug prints
 *
 * Revision 1.2  2004/07/25 22:34:56  ericn
 * -added source offset param to bltFrom
 *
 * Revision 1.1  2004/07/04 21:35:14  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */
#include <stdio.h>
#include "bitmap.h"

// #define DEBUGPRINT 1
#include "debugPrint.h"

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

void bitmap_t::rect( unsigned x, unsigned y, unsigned w, unsigned h, bool setNotClear )
{
	if( x < bitWidth_ ) {
		unsigned const wMax = bitWidth_-x ;
		if( wMax < w ) w = wMax ;
		if( y < bitHeight_ ) {
			unsigned hMax = bitHeight_-y ;
			if( hMax < h ) h = hMax ;
			if( setNotClear ) SetBox(bits_,x,y,w,h,bytesPerRow_);
			else ClearBox(bits_,x,y,w,h,bytesPerRow_);
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
      debugPrint( "horizontal line at y: %u, x:%u-%u, width:%u, %s\n",
              y1, x1, x2, penWidth, setNotClear ? "set" : "clear" );
      rect( x1, y1, x2-x1, penWidth, setNotClear );
   }
   else
   {
   } // diagonals not supported (yet)
}

//bool bdebug=false;

void bitmap_t::bltFrom
   ( unsigned char       *bitStart,
     unsigned             startOffs,
     unsigned char const *src,
     unsigned             count,
     unsigned             srcOffs )
{
   unsigned char *nextOut = bitStart+(startOffs/8);
   unsigned char outMask = 0x80 >> (startOffs&7);
   unsigned char inMask  = 0x80 >> (srcOffs&7);
   unsigned char inChar  = *src++ ;
   unsigned char outChar = *nextOut ; // preload
//	if (bdebug) printf("B:%x-%x ",(unsigned)nextOut,((unsigned)nextOut)+((count+7)>>3));

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

#define BigEndianToHost(a) (a<<24)|((a&0xff00)<<8)|((a&0xff0000)>>8)|(a>>24)

void bitmap_t::ClearBox(unsigned char* bmp,unsigned x,unsigned y,unsigned w,unsigned h,unsigned bmpStride )
{
	long unsigned int * base;
	long unsigned startMask;
	long unsigned endMask;
	unsigned remWidth;
	unsigned rowsLeft=h;
	unsigned char* dest = bmp + (y * bmpStride) + (x>>3);
	{
		unsigned unaligned = ((unsigned)dest)&3;
		unsigned startBit = (x&7) + (unaligned<<3);	//counting from MSB
		unsigned bitsInFirst = 32-startBit;
		base = (long unsigned int *)(dest - unaligned);
		startMask = 0xffffffff >> startBit;
//		bdebug = (x==0x2a)&&(y==0x1b1);	//(w==0x29) && (h==0xd4)
		if ( bitsInFirst >= w ) {
			startMask -= (0xffffffff >> (startBit+w));
			if ((bmpStride&3)==0) {
				//easy common case
				startMask = ~BigEndianToHost(startMask);
				while (rowsLeft) {
//					if (bdebug) printf("C:%x-%x ",(unsigned)d,((unsigned)d)+4+((remWidth+7)>>3));
					*base &= startMask;
					base = (long unsigned int *)(((unsigned char*)base) + bmpStride);
					rowsLeft--;
				}
				return;
			}
			remWidth = 0;
			endMask = 0xffffffff;
		} else {
			remWidth = w - bitsInFirst;
			endMask = (remWidth & 31) ? (0xffffffff >> (remWidth & 31)) : 0;
		}
	}
	startMask = ~BigEndianToHost(startMask);
	endMask = BigEndianToHost(endMask);
//	if (bdebug) printf("startMask:%08x, endMask:%08x remWidth:%x\r\n",startMask,endMask,remWidth);
	if ((bmpStride&3)==0) {
		//easy case
		while (rowsLeft) {
			unsigned colsLeft = remWidth;
			long unsigned int* d = base;
//			if (bdebug) printf("C:%x-%x ",(unsigned)d,((unsigned)d)+4+((remWidth+7)>>3));
			*d++ &= startMask;
			while (colsLeft > 32) {
				*d++ = 0;
				colsLeft-=32;
			}
			*d &= endMask;

			base = (long unsigned int *)(((unsigned char*)base) + bmpStride);
			rowsLeft--;
		}
	} else {
		while (rowsLeft) {
			unsigned colsLeft = remWidth;
			long unsigned int* d = base;
			*d++ &= startMask;
			while (colsLeft > 32) {
				*d++ = 0;
				colsLeft-=32;
			}
			*d &= endMask;

			dest += bmpStride;
			{
				unsigned unaligned = ((unsigned)dest)&3;
				unsigned startBit = (x&7) + (unaligned<<3);	//counting from MSB
				unsigned bitsInFirst = 32-startBit;
				base = (long unsigned int *)(dest - unaligned);
				startMask = 0xffffffff >> startBit;
				if ( bitsInFirst >= w ) {
					startMask -= (0xffffffff >> (startBit+w));
					remWidth = 0;
					endMask = 0xffffffff;
				} else {
					remWidth = w - bitsInFirst;
					endMask = (remWidth & 31) ? (0xffffffff >> (remWidth & 31)) : 0;
				}
			}
			startMask = ~BigEndianToHost(startMask);
			endMask = BigEndianToHost(endMask);
			rowsLeft--;
		}
	}
//	if (bdebug) printf("\r\n");
}

void bitmap_t::SetBox(unsigned char* bmp,unsigned x,unsigned y,unsigned w,unsigned h,unsigned bmpStride )
{
	long unsigned int * base;
	long unsigned startMask;
	long unsigned endMask;
	unsigned remWidth;
	unsigned rowsLeft=h;
	unsigned char* dest = bmp + (y * bmpStride) + (x>>3);
	{
		unsigned unaligned = ((unsigned)dest)&3;
		unsigned startBit = (x&7) + (unaligned<<3);	//counting from MSB
		unsigned bitsInFirst = 32-startBit;
		base = (long unsigned int *)(dest - unaligned);
		startMask = 0xffffffff >> startBit;
		if ( bitsInFirst >= w ) {
			startMask -= (0xffffffff >> (startBit+w));
			if ((bmpStride&3)==0) {
				//easy common case
				startMask = BigEndianToHost(startMask);
				while (rowsLeft) {
					*base |= startMask;
					base = (long unsigned int *)(((unsigned char*)base) + bmpStride);
					rowsLeft--;
				}
				return;
			}
			remWidth = 0;
			endMask = 0xffffffff;
		} else {
			remWidth = w - bitsInFirst;
			endMask = (remWidth & 31) ? (0xffffffff >> (remWidth & 31)) : 0;
		}
	}
	startMask = BigEndianToHost(startMask);
	endMask = ~BigEndianToHost(endMask);
	if ((bmpStride&3)==0) {
		//easy case
		while (rowsLeft) {
			unsigned colsLeft = remWidth;
			long unsigned int* d = base;
			*d++ |= startMask;
			while (colsLeft > 32) {
				*d++ = ~0;
				colsLeft-=32;
			}
			*d |= endMask;

			base = (long unsigned int *)(((unsigned char*)base) + bmpStride);
			rowsLeft--;
		}
	} else {
		while (rowsLeft) {
			unsigned colsLeft = remWidth;
			long unsigned int* d = base;
			*d++ |= startMask;
			while (colsLeft > 32) {
				*d++ = ~0;
				colsLeft-=32;
			}
			*d |= endMask;

			dest += bmpStride;
			{
				unsigned unaligned = ((unsigned)dest)&3;
				unsigned startBit = (x&7) + (unaligned<<3);	//counting from MSB
				unsigned bitsInFirst = 32-startBit;
				base = (long unsigned int *)(dest - unaligned);
				startMask = 0xffffffff >> startBit;
				if ( bitsInFirst >= w ) {
					startMask -= (0xffffffff >> (startBit+w));
					remWidth = 0;
					endMask = 0xffffffff;
				} else {
					remWidth = w - bitsInFirst;
					endMask = (remWidth & 31) ? (0xffffffff >> (remWidth & 31)) : 0;
				}
			}
			startMask = BigEndianToHost(startMask);
			endMask = ~BigEndianToHost(endMask);
			rowsLeft--;
		}
	}
}
