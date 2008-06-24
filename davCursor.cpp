/*
 * Module davCursor.cpp
 *
 * This module defines the methods of the davCursor_t
 * class as declared in davCursor.h
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "davCursor.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define OSDREG_RECTCUR 0x01c72610
#define OSDREG_CURXP 0x01c72688
#define OSDREG_CURYP 0x01c7268C
#define OSDREG_CURXL 0x01c72690
#define OSDREG_CURYL 0x01c72694

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_ALIGN(__addr) ((__addr>>PAGE_SHIFT)<<PAGE_SHIFT)

#define OSDREG_BASE (PAGE_ALIGN(OSDREG_RECTCUR))

static davCursor_t *instance_ = 0 ;

davCursor_t::davCursor_t
	( unsigned w, 
	  unsigned h, 
	  unsigned color )
	: fdMem_( open( "/dev/mem", O_RDWR ) )
	, mem_( 0 )
	, reg_rectcur_( 0 )
	, reg_curxp_( 0 )
	, reg_curyp_( 0 )
	, reg_curxl_( 0 )
	, reg_curyl_( 0 )
{
	if( 0 <= fdMem_ ){
		mem_ = mmap( 0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fdMem_, OSDREG_BASE );
		if( MAP_FAILED == mem_ ){
			perror( "mmap:/dev/mem" );
			mem_ = 0 ;
			return ;
		}
		reg_rectcur_ = (unsigned *)((char *)mem_ + OSDREG_RECTCUR-OSDREG_BASE);
		reg_curxp_ = (unsigned *)((char *)mem_ + OSDREG_CURXP-OSDREG_BASE);
		reg_curyp_ = (unsigned *)((char *)mem_ + OSDREG_CURYP-OSDREG_BASE);
		reg_curxl_ = (unsigned *)((char *)mem_ + OSDREG_CURXL-OSDREG_BASE);
		reg_curyl_ = (unsigned *)((char *)mem_ + OSDREG_CURYL-OSDREG_BASE);
		setWidth(w);
		setHeight(h);
		setColor(color);
		*reg_rectcur_ |= 0x3E ;
		activate();
                instance_ = this ;
	}
	else 
                perror( "/dev/mem" );

}

davCursor_t::~davCursor_t( void )
{
        deactivate();
	if( mem_ ){
		munmap(mem_,PAGE_SIZE);
		mem_ = 0 ;
	}
	if( 0 <= fdMem_ ){
		close( fdMem_ );
	}
	if( this == instance_ )
		instance_ = 0 ;
}

void davCursor_t::setColor( unsigned color )
{
	if( reg_rectcur_ ){
		*reg_rectcur_ = ((*reg_rectcur_) & 0xFF) | ((color&0xff)<<8);
	}
}

unsigned davCursor_t::getColor( void ) const 
{
	return reg_rectcur_ ? (*reg_rectcur_ >> 8) & 0xff : 0 ;
}

void davCursor_t::setPos( unsigned x, unsigned y )
{
	if( reg_curxp_ && reg_curyp_ ){
		*reg_curxp_ = x ;
		*reg_curyp_ = y ;
	}
}

void davCursor_t::getPos( unsigned &x, unsigned &y )
{
	x = reg_curxp_ ? *reg_curxp_ : 0 ;
	y = reg_curyp_ ? *reg_curyp_ : 0 ;
}

unsigned davCursor_t::getWidth( void ) const 
{
	return reg_curxl_ ? *reg_curxl_ : 0 ;
}

unsigned davCursor_t::getHeight( void ) const 
{
	return reg_curyl_ ? *reg_curyl_ : 0 ;
}

void davCursor_t::setWidth( unsigned w )
{
	if( reg_curxl_ )
		*reg_curxl_ = w ;
}

void davCursor_t::setHeight( unsigned h )
{
	if( reg_curyl_ )
		*reg_curyl_ = h ;
}

void davCursor_t::activate(void)
{
	if( reg_rectcur_ )
		*reg_rectcur_ |= 1 ;
}

void davCursor_t::deactivate(void)
{
	if( reg_rectcur_ )
		*reg_rectcur_ &= ~1 ;
}

davCursor_t *davCursor_t::get(void)
{
	return instance_ ;
}

void davCursor_t::destroy()
{
	if( instance_ )
		instance_->deactivate();
}

#ifdef MODULETEST_DAVCURSOR

#include <stdio.h>
#include <unistd.h>
#include "fbDevices.h"
#include <signal.h>
#include <stdlib.h>

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   davCursor_t::destroy();
   exit(1);
}

int main( int argc, char const * const argv[] )
{
	printf( "Hello, %s\n", argv[0] );
	davCursor_t cursor ;
	unsigned x = 0, y = 0, w = 1, h = 1, color = 0 ;
	fbDevices_t &devs = getFbDevs();
	fbDevice_t &osd0 = devs.osd0();
	
	signal( SIGINT, ctrlcHandler );

	while( (x+w < osd0.width()) && (y+h < osd0.height()) ){
		cursor.setPos(x,y);
		x++ ;
		y++ ;
		w = ((w+1) & 0x0f) | 1 ;
		h = ((h+1) & 0x0f) | 1 ;
		cursor.setWidth(w);
		cursor.setHeight(h);
		cursor.setColor(color++);
		color &= 0xff ;
		usleep(10000);
	}
	sleep( 10 );
	return 0 ;
}

#endif

