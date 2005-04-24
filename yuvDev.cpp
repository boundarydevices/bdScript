/*
 * Module yuvDev.cpp
 *
 * This module defines the methods of the yuvDev_t class
 * as declared in yuvDev.h
 *
 *
 * Change History : 
 *
 * $Log: yuvDev.cpp,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "yuvDev.h"
#include <linux/sm501yuv.h>

yuvDev_t :: yuvDev_t
   ( char const *devName )
   : fd_( open( devName, O_RDWR ) )
   , mappedMem_( 0 )
   , frames_( 0 )
   , entrySize_( 0 )
   , inWidth_( 0 )
   , inHeight_( 0 )
{
   memset( &plane_, 0, sizeof( plane_ ) );
   memset( &vmm_, 0, sizeof( vmm_ ) );
}

yuvDev_t :: ~yuvDev_t( void )
{
   if( isOpen() )
      close( fd_ );
   fd_ = -1 ;
}
   
void yuvDev_t :: initHeader
   ( unsigned xPos, 
     unsigned yPos,
     unsigned inWidth,
     unsigned inHeight,
     unsigned outWidth,
     unsigned outHeight )
{
   plane_.xLeft_     = xPos ;
   plane_.yTop_      = yPos ;
   plane_.inWidth_   = inWidth ;
   plane_.inHeight_  = inHeight ;
   plane_.outWidth_  = outWidth ;
   plane_.outHeight_ = outHeight ;
   if( 0 == ioctl( fd_, SM501YUV_SETPLANE, &plane_ ) )
   {
      if( 0 == ioctl( fd_, VIDIOCGMBUF, &vmm_ ) )
      {
         entrySize_ = vmm_.offsets[1]-vmm_.offsets[0];

         void *const mem_ = mmap( 0, vmm_.size, PROT_READ|PROT_WRITE, MAP_SHARED, fd_, 0 );
         if( MAP_FAILED != mem_ )
         {
            mappedMem_ = mem_ ;
            frames_ = new void *[vmm_.frames];
            char *const base = (char *)mem_ ;
            for( unsigned i = 0 ; i < vmm_.frames ; i++ )
            {
               frames_[i] = base + vmm_.offsets[i];
            }
            inWidth_ = inWidth ;
            inHeight_ = inHeight ;
         }
         else
            perror( "yuvMap" );
      }
      else
         perror( "VIDIOCGMBUF" );
   }
   else
      perror( "SETPLANE" );
}

void *yuvDev_t :: getBuf( unsigned &index )
{
   void *returnVal = 0 ;
   int result = ioctl( fd_, SM501YUV_GETBUF, &index );
   if( 0 == result )
   {
      if( vmm_.frames > index )
      {
         returnVal = frames_[index];
      }
      else
         fprintf( stderr, "Invalid frame index %u returned\n", index );
   }
   else if( ENOBUFS != errno )
      perror( "GETBUF" );

   return returnVal ;
}


bool yuvDev_t :: write( unsigned index, long long whenMs )
{
   if( vmm_.frames > index )
   {
      sm501Buffer_t buf ;
      buf.whenMs_ = whenMs ;
      buf.index_  = index ;
      buf.data_   = (unsigned char *)frames_[index] ;
      msync( frames_[index], getEntrySize(), MS_SYNC|MS_INVALIDATE );
      if( 0 == ioctl( fd_, SM501YUV_PUTBUF, &buf ) )
         return true ;
      else
         perror( "PUTBUF" );
   }
   else
      fprintf( stderr, "write of invalid frame: %u out of %u\n", index, vmm_.frames );
   
   return false ;
}


#ifdef __MODULETEST__

#include "rawKbd.h"
#include <ctype.h>
#include <math.h>

static void rgbToYUV( unsigned long  rgb, 
                      unsigned char &y, 
                      unsigned char &u, 
                      unsigned char &v )
{
   unsigned char const R = ( rgb >> 16 ) & 255 ;
   unsigned char const G = ( rgb >> 8 ) & 255 ;
   unsigned char const B = ( rgb & 255 );
   double Y =      (0.257 * R) + (0.504 * G) + (0.098 * B) + 16 ;
   if( Y > 255.0 )
      y = 255 ;
   else
      y = (unsigned char)floor( Y );

   double U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128 ;
   if( U > 255.0 )
      u = 255 ;
   else
      u = (unsigned char)floor( U );

   double V =  (0.439 * R) - (0.368 * G) - (0.071 * B) + 128 ;
   if( V > 255.0 )
      v = 255 ;
   else
      v = (unsigned char)floor( V );
   
}

void setYUV( yuvDev_t &dev,
             void     *mem,
             unsigned char y,
             unsigned char u, 
             unsigned char v )
{
   memset( mem, 0x80808080, dev.getEntrySize() );
   unsigned char *yuv = (unsigned char *)mem ;
   unsigned const stride = dev.getRowStride();

   for( unsigned row = 0 ; row < dev.getInHeight() ; row++ )
   {
      for( unsigned col = 0 ; col < stride ; col++ )
      {
         *yuv++ = y ;
         if( 0 == ( col & 1 ) )
            *yuv++ = u ;
         else
            *yuv++ = v ;

//         yuv += 2 ;
      }
   }

   printf( "%u of %u bytes written\n", yuv-(unsigned char *)mem, dev.getEntrySize() );
}

#include "hexDump.h"

int main( void )
{
   printf( "in yuvDevMain\n" );
   rawKbd_t kbd ;
   yuvDev_t yuv ;

   while( 1 )
   {

      char inBuf[80];
      char term ;
      printf( "cmd: " );
      unsigned long len = kbd.readln( inBuf, sizeof( inBuf ), &term );

      if( ( '\x03' == term ) || ( '\x1b' == term ) )
         break;
      
      if( 0 < len )
      {
         char const cmdChar = toupper( inBuf[0] );
         switch( cmdChar )
         {
            case 'O' :
               {
                  if( !yuv.haveHeaders() )
                  {
                     unsigned x, y, w, h, ow, oh ;
                     if( 6 == sscanf( inBuf+2, "%u,%u,%u,%u,%u,%u", &x, &y, &w, &h, &ow, &oh ) )
                     {
                        printf( "open window here: %u,%u,%u,%u,%u,%u\n", x, y, w, h, ow, oh );
                        yuv.initHeader( x, y, w, h, ow, oh );
                     }
                     else
                        printf( "Error reading params\n" );
                  }
                  else
                     printf( "can't open twice: close and re-run\n" );
                  break;
               }
            case 'F' :
               {
                  if( yuv.haveHeaders() )
                  {
                     unsigned rgb ;
                     unsigned long when ;
                     if( 2 == sscanf( inBuf+2, "%x,%u", &rgb, &when ) )
                     {
                        unsigned char y, u, v ;
                        rgbToYUV( rgb, y, u, v );
                        
                        unsigned index ;
                        void *data = yuv.getBuf( index );
                        if( 0 == data )
                           printf( "waiting for buffer... hit any key to interrupt\n" );
   
                        while( 0 == data )
                        {
                           pollfd filedes ;
                           filedes.fd = yuv.getFd();
                           filedes.events = POLLOUT ;
                           int result = poll( &filedes, 1, 0 );
                           if( 0 < result )
                           {
                              data = yuv.getBuf( index );
                           }
                           else
                           {
                              char c ;
                              if( kbd.read( c ) )
                                 break ;
                           }
                        }
   
                        if( 0 != data )
                        {
                           setYUV( yuv, data, y, u, v );
                           yuv.write( index, tickMs()+when );
                           printf( "YUV: %02x,%02x,%02x at %u ms in frame %u\n", y, u, v, when, index );
                        }
                        else
                           printf( "wait aborted\n" );
                     }
                     else
                        printf( "Invalid rgb, specify in hex\n" );
                  }
                  else
                     printf( "Open first\n" );

                  break;
               }
            case 'R' :
               {
                  if( yuv.haveHeaders() )
                  {
                     unsigned index ;
                     if( ( 1 == sscanf( inBuf+2, "%u", &index ) )
                         &&
                         ( yuv.numEntries() > index ) )
                     {
                        printf( "index %u\n", index );
                        hexDumper_t dump( yuv.getEntry( index ), yuv.getEntrySize() );
                        while( dump.nextLine() )
                           printf( "%s\n", dump.getLine() );
                     }
                     else
                        printf( "Invalid index, specify in decimal: 0..%u\n", yuv.numEntries() );
                  }
                  else
                     printf( "Open first\n" );
                  break;
               }

            case 'X' :
               {
                  return 0 ;
               }
            default:
               printf( "Commands:\n"
                       "O x,y,w,h,ow,oh   (open window at x,y,inwidth,inheight,outwidth,outheight)\n"
                       "R frameNum        (hex dump of YUV frame)\n"
                       "F RGB,whenMs      (display frame w/specified RGB(hex) at specified time)\n" );
         } // switch
      }
   }

   return 0 ;
}

#endif 
