/*
 * Module sm501Cursor.cpp
 *
 * This module defines the methods of the sm501Cursor_t
 * class as declared in sm501Cursor.h
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "sm501Cursor.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "fbDev.h"
#include "dither.h"
#include <linux/sm501-int.h>

static sm501Cursor_t *instance_ = 0 ;

sm501Cursor_t::sm501Cursor_t
	( image_t const &img )
{
   dither_t dither( (unsigned short const *)img.pixData_, img.width_, img.height_ );
   
   unsigned sm501_cursorWidth = 64 ;
   unsigned sm501_cursorHeight = 64 ;
   //
   // SM-501 cursor is 64x64 pixels, each of 
   // which is two bits long:
   //      00      - transparent
   //      01      - color 0 (black)
   //      02      - color 1 (white)
   unsigned cursorBytes = sm501_cursorWidth*sm501_cursorHeight*2/8 ; 
   fbDevice_t &fb = getFB();
   unsigned long reg = SMIPCURSOR_ADDR ;
   int res = ioctl( fb.getFd(), SM501_READREG, &reg );
   if( 0 == res ){
      unsigned long cursorOffs = reg & ((8<<20)-1);
      printf( "Cursor offset == 0x%lx (decimal %lu)\n", cursorOffs, cursorOffs );
      reg = SMIDISPCTRL_FBADDR ;
      res = ioctl( fb.getFd(), SM501_READREG, &reg );
      if( 0 == res ){
         unsigned long fbOffs = reg & ((8<<20)-1);
         printf( "FB offset == 0x%lx (decimal %lu)\n", fbOffs, fbOffs );
         reg = cursorOffs - fbOffs ;
      }
      else {
         perror("SM501_READREG");
         reg = 0 ;
      }
   }
   else {
     perror("SM501_READREG");
     reg = 0 ;
   }
   unsigned long *cursorLongs = (unsigned long *)
                               ((char *)fb.getRow(0) + reg );
   memset( cursorLongs, 0, cursorBytes );
   
   // cursor is right after panel display area
   unsigned const height = img.height_ > sm501_cursorHeight ? sm501_cursorHeight : img.height_ ;
   unsigned const width =  img.width_ > sm501_cursorWidth ? sm501_cursorWidth : img.width_ ;
   unsigned char const *alpha = (unsigned char *)img.alpha_ ;

   for( unsigned y = 0 ; y < height ; y++ ){
          unsigned long accum = 0 ;
          unsigned long shift = 0 ;
          unsigned char whichLong = 0 ;
   
          for( unsigned x = 0 ; x < width ; x++ ){
                  unsigned char value ;
                  if( alpha && alpha[x] ){
                          value = 2-dither.isBlack(x,y);
                  }
                  else
                          value = 0 ;
                  accum |= value<<shift ;
                  shift += 2 ;
                  if( 32 <= shift ){
                          cursorLongs[whichLong++] = accum ;
                          accum = 0 ;
                          shift = 0 ;
                  }
          }
          cursorLongs += (sm501_cursorWidth/2/8);
          alpha += img.width_ ;
   }
}

sm501Cursor_t::~sm501Cursor_t( void )
{
        deactivate();
	if( this == instance_ )
		instance_ = 0 ;
}

void sm501Cursor_t::setPos( unsigned x, unsigned y )
{
   fbDevice_t &fb = getFB();
   struct reg_and_value rv ;
   rv.reg_ = SMIPCURSOR_LOC ;
   rv.value_ = (y << 16) | x ;
   ioctl( fb.getFd(), SM501_WRITEREG, &rv );
}

void sm501Cursor_t::getPos( unsigned &x, unsigned &y )
{
   x = y = 0 ;
   fbDevice_t &fb = getFB();
   unsigned long reg = SMIPCURSOR_LOC ;
   int res = ioctl( fb.getFd(), SM501_READREG, &reg );
   if( 0 == res ){
      x = reg & 0xFFFF ;
      y = (reg & 0x7fff0000 ) >> 16 ;
   }
}

void sm501Cursor_t::activate(void)
{
   fbDevice_t &fb = getFB();
   unsigned long reg = SMIPCURSOR_ADDR ;
   int res = ioctl( fb.getFd(), SM501_READREG, &reg );
   if( 0 == res ){
      struct reg_and_value rv ;
      rv.reg_ = SMIPCURSOR_ADDR ;
      rv.value_ = reg | 0x80000000 ;
      ioctl( fb.getFd(), SM501_WRITEREG, &rv );
   }
}

void sm501Cursor_t::deactivate(void)
{
   fbDevice_t &fb = getFB();
   unsigned long reg = SMIPCURSOR_ADDR ;
   int res = ioctl( fb.getFd(), SM501_READREG, &reg );
   if( 0 == res ){
      struct reg_and_value rv ;
      rv.reg_ = SMIPCURSOR_ADDR ;
      rv.value_ = reg & ~0x80000000 ;
      ioctl( fb.getFd(), SM501_WRITEREG, &rv );
   }
}

sm501Cursor_t *sm501Cursor_t::get(void)
{
	return instance_ ;
}

void sm501Cursor_t::destroy()
{
	if( instance_ )
		instance_->deactivate();
}


