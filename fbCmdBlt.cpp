/*
 * Module fbCmdBlt.cpp
 *
 * This module defines the methods of the fbBlt_t class
 * as declared in fbCmdBlt.h
 *
 *
 * Change History : 
 *
 * $Log: fbCmdBlt.cpp,v $
 * Revision 1.6  2008-10-29 15:27:46  ericn
 * -prevent compiler warnings
 *
 * Revision 1.5  2002-12-15 05:38:48  ericn
 * -added swapSource() method
 *
 * Revision 1.4  2006/12/13 21:31:32  ericn
 * -allow re-targeting RAM
 *
 * Revision 1.2  2006/10/16 22:37:17  ericn
 * -add membbers getDestX(), setDestX(), getWidth()
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbCmdBlt.h"
#include "fbCmdWait.h"
#include "fbCmdFinish.h"
#include "fbDev.h"
#include <linux/sm501-int.h>
#include <string.h>
#include <sys/ioctl.h>

static unsigned long const bltCommand_[] = {
    0x30100000    // -- load register immediate 0x100000 - 2d source
,   0x00000014    // -- 20 (0x14) DWORDS
,   0x00000000    //   _2D_Source
,   0x00000000    //   _2D_Destination
,   0x04000300    //   _2D_Dimension
,   0x00000000    //   _2D_Control
,   0x04000001    //   _2D_Pitch
,   0x0000f800    //   _2D_Foreground
,   0x000007E0    //   _2D_Background
,   0x00100000    //   _2D_Stretch_Format
,   0x00000000    //   _2D_Color_Compare
,   0x00000000    //   _2D_Color_Compare_Mask
,   0xFFFFFFFF    //   _2D_Mask
,   0x00000000    //   _2D_Clip_TL
,   0x04000300    //   _2D_Clip_BR
,   0x00000000    //   _2D_Mono_Pattern_Low
,   0x00000000    //   _2D_Mono_Pattern_High
,   0x04000400    //   _2D_Window_Width
,   0x00180000    //   _2D_Source_Base
,   0x00000000    //   _2D_Destination_Base
,   0x00000000    //   _2D_Alpha
,   0x00000000    //   _2D_Wrap
,   0x1010000C    // -- load register 0x10000C  _2D_Control
,   0x800000CC    //    bitblt
};

#define BLTREG(REGNUM) ( ( ((REGNUM)-(SMIDRAW_2D_Source)) / sizeof(bltCommand_[0]) ) + 2 )
#define BLTCMDREG      ((sizeof(bltCommand_)/sizeof(bltCommand_[0]))-1)
#define BLTWITHALPHA 0x808400CC
#define BLTBLT       0x808000CC
#define BLTCMD_TRANSPARENT 0x00000500


fbBlt_t::fbBlt_t( 
   unsigned long    destRamOffs,
   unsigned         destx,
   unsigned         desty,
   unsigned         destw,
   unsigned         desth,
   fbImage_t const &srcImg,
   unsigned         srcx,
   unsigned         srcy,
   unsigned         w,
   unsigned         h )
   : fbCommand_t(sizeof(bltCommand_))
   , data_( new unsigned long [size()/sizeof(data_[0])] )
   , cmdMem_( data_ )
   , skip_( false )
{
   memcpy( cmdMem_, bltCommand_, size() );

   set( destRamOffs,
        destx, desty, destw, desth,
        srcImg,
        srcx, srcy, w, h );
}

void fbBlt_t::set( 
   unsigned long    destRamOffs,
   unsigned         destx,
   unsigned         desty,
   unsigned         destw,
   unsigned         desth,
   fbImage_t const &srcImg,
   unsigned         srcx,
   unsigned         srcy,
   unsigned         w,
   unsigned         h )
{
   destRamOffs += (desty*destw*2);
   desty = 0 ;

   cmdMem_[BLTREG(SMIDRAW_2D_Destination)]  = ( destx << 16 ) | desty ;
   cmdMem_[BLTREG(SMIDRAW_2D_Dimension)]    = (w<<16) | h ;
   cmdMem_[BLTREG(SMIDRAW_2D_Clip_TL)]      = 1 << 13 ;
   cmdMem_[BLTREG(SMIDRAW_2D_Clip_BR)]      = ( desth << 16 ) | destw ;
   cmdMem_[BLTREG(SMIDRAW_2D_Pitch)]        = ( destw << 16 ) | srcImg.stride();
   cmdMem_[BLTREG(SMIDRAW_2D_Stretch_Format)] = ( cmdMem_[BLTREG(SMIDRAW_2D_Stretch_Format)] & 0xFFFFF000 )
                                                  | h ;
   cmdMem_[BLTREG(SMIDRAW_2D_Window_Width)] = ( destw << 16 ) | srcImg.stride();
   cmdMem_[BLTREG(SMIDRAW_2D_Source_Base)]  = srcImg.ramOffset() + srcy*srcImg.stride()*2;
   cmdMem_[BLTREG(SMIDRAW_2D_Destination_Base)] = destRamOffs ;
}

fbBlt_t::~fbBlt_t( void )
{
   if( data_ )
      delete [] data_ ;
}

void const *fbBlt_t::data( void ) const 
{
   return cmdMem_ ;
}

void fbBlt_t::retarget( void *cmdMem )
{
   cmdMem_ = (unsigned long *)cmdMem ;
}

void fbBlt_t::skip( void )
{
   if( !isSkipped() )
   {
      fbJump_t jump( size()-sizeof(jump.cmdData_) );
      memcpy( skipBuf_, data(), sizeof(skipBuf_) );
      memcpy( cmdMem_, jump.cmdData_, sizeof(jump.cmdData_) );
      skip_ = true ;
   }
}

void fbBlt_t::perform( void )
{
   if( isSkipped() )
   {
      memcpy( cmdMem_, skipBuf_, sizeof(skipBuf_) );
      skip_ = false ;
   }
}

unsigned fbBlt_t::getDestX( void ) const 
{
   return cmdMem_[BLTREG(SMIDRAW_2D_Destination)] >> 16 ;
}

void fbBlt_t::setDestX( unsigned destx )
{
//   unsigned const oldDest = cmdMem_[BLTREG(SMIDRAW_2D_Destination)] >> 16 ;
   cmdMem_[BLTREG(SMIDRAW_2D_Destination)] &= 0x0000FFFF ;
   cmdMem_[BLTREG(SMIDRAW_2D_Destination)] |= (destx << 16 );
}

void fbBlt_t::moveDestY( int numRows )
{
   unsigned const w = getWidth();
   unsigned destRamOffs = cmdMem_[BLTREG(SMIDRAW_2D_Destination_Base)];
   cmdMem_[BLTREG(SMIDRAW_2D_Destination_Base)] = destRamOffs + numRows*w*2 ;
}


unsigned fbBlt_t::getWidth( void ) const 
{
   return cmdMem_[BLTREG(SMIDRAW_2D_Clip_BR)] & 0xFFFF ;
}

void fbBlt_t::swapSource( fbImage_t const &src, unsigned srcy )
{
   unsigned long regVal ;
   regVal = cmdMem_[BLTREG(SMIDRAW_2D_Dimension)] & 0x0000FFFF ;
   cmdMem_[BLTREG(SMIDRAW_2D_Dimension)]    = regVal | (src.width()<<16) ;
   regVal = cmdMem_[BLTREG(SMIDRAW_2D_Pitch)] & 0xFFFF0000 ;
   cmdMem_[BLTREG(SMIDRAW_2D_Pitch)]        = regVal | src.stride();
   regVal = cmdMem_[BLTREG(SMIDRAW_2D_Window_Width)] & 0xFFFF0000 ;
   cmdMem_[BLTREG(SMIDRAW_2D_Window_Width)] = regVal | src.stride();
   cmdMem_[BLTREG(SMIDRAW_2D_Source_Base)]  = src.ramOffset() + srcy*src.stride()*2;
}

int fbBlt( unsigned long    destRamOffs,
           unsigned         destx,
           unsigned         desty,
           unsigned         destw,
           unsigned         desth,
           fbImage_t const &srcImg,
           unsigned         srcx,
           unsigned         srcy,
           unsigned         w,
           unsigned         h )
{
   fbDevice_t &fb = getFB();

   fbCommandList_t cmdList ;
   cmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
   cmdList.push( new fbBlt_t( destRamOffs, destx, desty, destw, desth,
                              srcImg, srcx, srcy, w, h ) );
   cmdList.push( new fbFinish_t );
   fbPtr_t cmdListMem( cmdList.size() );
   cmdList.copy( cmdListMem.getPtr() );
   return ioctl( fb.getFd(), SM501_EXECCMDLIST, cmdListMem.getOffs() );
}
