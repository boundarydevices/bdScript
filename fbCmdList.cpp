/*
 * Module fbCmdList.cpp
 *
 * This module defines the methods of the fbCommandList_t
 * class as declared in fbCmdList.h
 *
 *
 * Change History : 
 *
 * $Log: fbCmdList.cpp,v $
 * Revision 1.2  2006-10-16 22:36:37  ericn
 * -make fbJump_t a full-fledged fbCommand_t
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbCmdList.h"
#include <string.h>
#include "fbDev.h"
#include <linux/sm501-int.h>
#include <stdio.h>
#include <sys/ioctl.h>

static bool initialized_ = false ;
static void initialize()
{
   reg_and_value rv ;
   rv.reg_   = SMICMD_CONDITION ;
   rv.value_ = 1 ; // condition bit for skip commands

   int res = ioctl( getFB().getFd(), SM501_WRITEREG, &rv );
   if( res )
      perror( "SM501_WRITEREG" );
   initialized_ = true ;
}

void fbCommand_t::retarget( void *data )
{
}

fbCommandList_t::fbCommandList_t( void )
   : size_( 0 )
   , commands_()
{
   if( !initialized_ )
      initialize();
}

fbCommandList_t::~fbCommandList_t( void )
{
   for( unsigned i = 0 ; i < commands_.size(); i++ )
      delete commands_[i];
}

void fbCommandList_t::push( fbCommand_t *cmd )
{
   size_ += cmd->size();
   commands_.push_back( cmd );
}

void fbCommandList_t::copy( void *where )
{
   char *outBytes = (char *)where ;
   for( unsigned i = 0 ; i < commands_.size(); i++ )
   {
      fbCommand_t *const cmd = commands_[i];
      unsigned const size = cmd->size();
      memcpy( outBytes, cmd->data(), size );

      // point command at it's new buffer
      cmd->retarget( outBytes );
      outBytes += size ;
   }
}

fbJump_t::fbJump_t( unsigned numBytes )
   : fbCommand_t( sizeof( cmdData_ ) )
   , cmdPtr_( cmdData_ )
{
   cmdData_[0] = 0xC0000000 | (numBytes & 0x0FFFFFF8);
   cmdData_[1] = 1 ;
}

void const *fbJump_t::data( void ) const 
{
   return cmdPtr_ ;
}

void fbJump_t::retarget( void *data )
{
   cmdPtr_ = (unsigned long *)data ;
}

void fbJump_t::setLength( unsigned bytes )
{
   cmdPtr_[0] = 0xC0000000 | (bytes & 0x0FFFFFF8);
}

void fbCommandList_t::dump()
{
   printf( "---- %u bytes, %u commands ----\n", size_, commands_.size() );
   for( unsigned i = 0 ; i < commands_.size(); i++ )
   {
      fbCommand_t &cmd = *(commands_[i]);
      printf( "--- command %u, %u bytes\n", i, cmd.size() );
      unsigned numLongs = cmd.size()/4 ;
      unsigned long const *longs = (unsigned long const *)cmd.data();
      for( unsigned l = 0 ; l < numLongs ; l++ )
         printf( "%08lx\n", *longs++ );
   }
}

#ifdef MODULETEST

#include <stdio.h>
#include "fbCmdBlt.h"
#include "fbCmdFinish.h"
#include "fbCmdWait.h"
#include "imgFile.h"
#include "fbImage.h"
#include "fbDev.h"
#include "hexDump.h"
#include <sys/ioctl.h>
#include <linux/sm501-int.h>
#include "tickMs.h"

int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
      fbDevice_t &fb = getFB();
      
      reg_and_value rv ;
      rv.reg_   = SMICMD_CONDITION ;
      rv.value_ = 1 ; // condition bit for skip commands

      int res = ioctl( fb.getFd(), SM501_WRITEREG, &rv );
      if( res )
         perror( "SM501_WRITEREG" );

      fbCommandList_t cmdList ;

      unsigned const imgCount = argc-1 ;
      fbImage_t ** const images = new fbImage_t *[imgCount];
      fbBlt_t   ** const blts = new fbBlt_t *[imgCount];
      unsigned   * const offsets = new unsigned [imgCount];

      unsigned x = 0 ;
      unsigned fbRAM = 0 ; 

      for( int arg = 1 ; arg < argc ; arg++ )
      {
         image_t image ;
         if( !imageFromFile( argv[arg], image ) ){
            perror( argv[arg] );
            return -1 ;
         }

         cmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );

         fbImage_t *const newImg = new fbImage_t( image, fbImage_t::rgb565 );
//         memset( newImg->pixels(), 0x80, newImg->stride()*newImg->height()*2 );
         
         printf( "%u x %u pixels, stride %u\n", image.width_, image.height_, newImg->stride() );
         unsigned const i = arg-1 ;
         images[i] = newImg ;
         blts[i] = new fbBlt_t( fbRAM,
                                x, 0, 
                                fb.getWidth(), fb.getHeight(),
                                *newImg,
                                0, 0, 
                                newImg->width(), newImg->height() );
         offsets[i] = cmdList.size();
         cmdList.push( blts[i] );
         x += newImg->width();
      }
      
      printf( "loaded %u images\n", argc-1 );
      cmdList.push( new fbFinish_t );
      printf( "%u bytes of cmdlist\n", cmdList.size() );

      fbPtr_t cmdListMem( cmdList.size() );
      cmdList.copy( cmdListMem.getPtr() );
      hexDumper_t dump( cmdListMem.getPtr(), cmdListMem.size(), cmdListMem.getOffs() );
      while( dump.nextLine() )
         printf( "%s\n", dump.getLine() );
/*
      for( unsigned i = 0 ; i < imgCount ; i++ ){
         printf( "---> blts[%u]\n", i );
         unsigned long const *longs = (unsigned long const *)( (char *)cmdListMem.getPtr() + offsets[i] );
         for( unsigned j = 0 ; j < blts[i]->size() ; j += 4 )
            printf( "%08lx\n", *longs++ );
      }
*/

      unsigned const imgWidth = x ;
      unsigned y = imgWidth ;
      unsigned numCmds = 0 ;
      long long start = tickMs();
      while( ( x < fb.getWidth() ) && ( y < fb.getHeight() ) )
      {
         int rval = ioctl( fb.getFd(), SM501_EXECCMDLIST, cmdListMem.getOffs() );
         if( rval ){
            printf( "result: %d\n", rval );
            break ;
         }

         ++numCmds ;
         for( unsigned i = 0 ; i < imgCount ; i++ ){
            blts[i]->set( fbRAM,
                          x, y, 
                          fb.getWidth(), fb.getHeight(),
                          *images[i],
                          0, 0, 
                          images[i]->width(), images[i]->height() );

/*
*/
            if( numCmds & 1 ){
               printf( "skip cmd %u\n", numCmds );
               blts[i]->skip();
            }
            else {
               printf( "perform cmd %u\n", numCmds );
               blts[i]->perform();
            }

            x += images[i]->width();
         }
         y += imgWidth ;
//         if( x < fb.getWidth() )
//            cmdList.copy( cmdListMem.getPtr() );
      }
      long long end = tickMs();
      blts[0]->skip();
      for( unsigned i = 0 ; i < imgCount ; i++ ){
         printf( "---> blts[%u]\n", i );
         unsigned long const *longs = (unsigned long const *)( (char *)cmdListMem.getPtr() + offsets[i] );
         for( unsigned j = 0 ; j < blts[i]->size() ; j += 4 )
            printf( "%08lx\n", *longs++ );
      }

      printf( "%u cmd lists, %u blts in %lu ms\n",
              numCmds, numCmds*imgCount, (unsigned long)(end-start) );
   }
   else
      fprintf( stderr, "Usage: %s imgFile [imgFile...]\n", argv[0] );

   return 0 ;
}

#endif
