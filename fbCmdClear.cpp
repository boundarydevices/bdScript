/*
 * Module fbCmdClear.cpp
 *
 * This module defines the methods of the fbCmdClear_t
 * class as declared in fbCmdClear.h
 *
 *
 * Change History : 
 *
 * $Log: fbCmdClear.cpp,v $
 * Revision 1.1  2006-10-16 22:45:31  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbCmdClear.h"
#include "fbCmdWait.h"
#include "fbDev.h"
#include <linux/sm501-int.h>
#include <string.h>
#include <sys/ioctl.h>
#include "fbDev.h"

static unsigned long const clrCommand_[] = {
    0x30100000    // -- load register immediate 0x100000 - 2d source
,   0x00000014    // -- 20 (0x14) DWORDS
,   0x00000000    //   _2D_Source
,   0x00000000    //   _2D_Destination
,   0x04000300    //   _2D_Dimension         --- screen size... needs fixup
,   0x00000000    //   _2D_Control
,   0x04000001    //   _2D_Pitch             --- dest screen size... needs fixup
,   0x00000000    //   _2D_Foreground        -- foreground color
,   0x0000FFFF    //   _2D_Background        -- background color
,   0x00100001    //   _2D_Stretch_Format
,   0x00000000    //   _2D_Color_Compare
,   0x00000000    //   _2D_Color_Compare_Mask
,   0xFFFFFFFF    //   _2D_Mask
,   0x00000000    //   _2D_Clip_TL           -- needs to be fixed up
,   0x04000300    //   _2D_Clip_BR           -- needs to be fixed up
,   0x00000000    //   _2D_Mono_Pattern_Low
,   0x00000000    //   _2D_Mono_Pattern_High
,   0x04000400    //   _2D_Window_Width      -- needs fixup
,   0x00180000    //   _2D_Source_Base       -- needs fixup
,   0x00000000    //   _2D_Destination_Base  -- needs fixup
,   0x00000000    //   _2D_Alpha
,   0x00000000    //   _2D_Wrap
,   0x1010000C    // -- load register 0x10000C  _2D_Control
,   0x808100CC    //    rectangle fill
};

#define CLRREG(REGNUM) ( ( ((REGNUM)-(SMIDRAW_2D_Source)) / sizeof(clrCommand_[0]) ) + 2 )


fbCmdClear_t::fbCmdClear_t
   ( unsigned long    destRamOffs,
     unsigned         destx,
     unsigned         desty,
     unsigned         destw,
     unsigned         desth,
     unsigned short   rgb16 )
   : fbCommand_t(sizeof(clrCommand_))
   , data_( new unsigned long [size()/sizeof(data_[0])] )
   , cmdMem_( data_ )
{
   memcpy( cmdMem_, clrCommand_, size() );

   fbDevice_t &fb = getFB();

   destRamOffs += (desty*fb.getWidth()*2);
   desty = 0 ;

   unsigned const bottom = desty+desth ;
   unsigned const right = destx+destw ;

   cmdMem_[CLRREG(SMIDRAW_2D_Destination)]  = ( destx << 16 ) | desty ;
   cmdMem_[CLRREG(SMIDRAW_2D_Dimension)]    = (destw<<16) | desth ;
   cmdMem_[CLRREG(SMIDRAW_2D_Foreground)]   = rgb16 ;
   cmdMem_[CLRREG(SMIDRAW_2D_Clip_TL)]      = 1 << 13 ;
   cmdMem_[CLRREG(SMIDRAW_2D_Clip_BR)]      = ( bottom << 16 ) | right ;
   cmdMem_[CLRREG(SMIDRAW_2D_Pitch)]        = ( fb.getWidth() << 16 ) | 1 ;
   cmdMem_[CLRREG(SMIDRAW_2D_Window_Width)] = ( fb.getWidth() << 16 ) | 1 ;
   cmdMem_[CLRREG(SMIDRAW_2D_Source_Base)]  = 0 ;
   cmdMem_[CLRREG(SMIDRAW_2D_Destination_Base)] = destRamOffs ;
}

fbCmdClear_t::~fbCmdClear_t( void )
{
   if( data_ )
      delete [] data_ ;
}

void const *fbCmdClear_t::data( void ) const 
{
   return cmdMem_ ;
}

void fbCmdClear_t::retarget( void *fbMem )
{
   cmdMem_ = (unsigned long *)fbMem ;
}

unsigned fbCmdClear_t::getDestX( void ) const 
{
   return cmdMem_[CLRREG(SMIDRAW_2D_Destination)] >> 16 ;
}

void fbCmdClear_t::setDestX( unsigned xPos )
{
   unsigned prevX = getDestX();
   if( prevX != xPos ){
      printf( "move clear to xPos: %u\n", xPos );
      cmdMem_[CLRREG(SMIDRAW_2D_Destination)] &= 0x0000FFFF ;
      cmdMem_[CLRREG(SMIDRAW_2D_Destination)] |= (xPos << 16 );
      unsigned short right = cmdMem_[CLRREG(SMIDRAW_2D_Clip_BR)] & 0xFFFF ;
      right -= prevX ;
      right += xPos ;
      cmdMem_[CLRREG(SMIDRAW_2D_Clip_BR)] = ( cmdMem_[CLRREG(SMIDRAW_2D_Clip_BR)] & 0xFFFF0000 ) | right ;
   }
}


#ifdef MODULETEST
#include <stdio.h>
#include <unistd.h>
#include "fbCmdListSignal.h"
#include "sm501alpha.h"
#include "fbCmdWait.h"
#include "fbCmdFinish.h"
#include "fbMem.h"

int main( int argc, char const * const argv[] )
{
   if( 5 < argc ){
      unsigned x = strtoul( argv[1], 0, 0 );
      unsigned y = strtoul( argv[2], 0, 0 );
      unsigned w = strtoul( argv[3], 0, 0 );
      unsigned h = strtoul( argv[4], 0, 0 );
      unsigned rgb16 = strtoul( argv[5], 0, 0 );
      
      fbCmdListSignal_t &cmdListDev = fbCmdListSignal_t::get();
      if( !cmdListDev.isOpen() ){
         perror( "cmdListDev" );
         return -1 ;
      }

      fbCommandList_t cmdList ;
      cmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
      cmdList.push( new fbCmdClear_t( 0, x, y, w, h, rgb16 ) );
      cmdList.push( new fbFinish_t );
      fbPtr_t cmdListMem( cmdList.size() );
      cmdList.copy( cmdListMem.getPtr() );

      unsigned long cmdListOffs = cmdListMem.getOffs();
      int numWritten = write( cmdListDev.getFd(), &cmdListOffs, sizeof(cmdListOffs) );
      if( numWritten == sizeof(cmdListOffs) ){
         printf( "command complete\n" );
      }
      else
         perror( "cmdListWrite" );

   }
   else
      fprintf( stderr, "Usage: %s x y w h RGB16\n", argv[0] );

   return 0 ;
}

#endif
