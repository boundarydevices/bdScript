/*
 * Module odomCommand.cpp
 *
 * This module defines the odomCmdInterp_t class
 * as declared in odomCmdInterp_t
 *
 *
 * Change History : 
 *
 * $Log: odomCommand.cpp,v $
 * Revision 1.2  2006-09-04 15:16:28  ericn
 * -add volume command
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomCommand.h"
#include <string.h>
#include <alloca.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "screenImage.h"
#include "imgToPNG.h"
#include "imgFile.h"
#include "odomPlaylist.h"
#include "fbDev.h"
#include <sys/ioctl.h>
#include <linux/sm501-int.h>
#include "memFile.h"
#include "ftObjs.h"
#include <map>
#include <string>
#include "sm501alpha.h"
#include "odomMode.h"

static odometerMode_e odometerMode_ = graphicsLayer ;

//
// command handlers are passed an array of parameters starting
// with the command used (a.la. main()).
//
typedef bool (*commandHandler_t)( odomCmdInterp_t   &interp,
                                  char const * const params[],
                                  unsigned           numParams,
                                  char              *errorMsg,
                                  unsigned           errorMsgLen );

typedef struct commandEntry_t {
   char const      *name_ ;
   commandHandler_t handler_ ;
};

static bool args( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   printf( "--> %u parameters\n", numParams );
   for( unsigned i = 0 ; i < numParams ; i++ ){
      printf( "[%u] == %p/%s\n", i, params[i], params[i] );
   }
   return true ;
}

static bool mode( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 2 == numParams ){
      odometerMode_ = ('4' == *params[1])
                      ? alphaLayer
                      : graphicsLayer ;
      return true ;
   }
   else {
      snprintf( errorMsg, errorMsgLen, 
                "Usage: mode 4444|565\n" 
                "current mode is %s\n",
                (alphaLayer==odometerMode_) 
                ? "4444"
                : "565" );
   }
   
   return false ;
}

static bool graphics( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 3 == numParams ){
      char const *const graphName = params[1];
      char const *const dir = params[2];
      odomGraphics_t *graphics = new odomGraphics_t(dir,odometerMode_);
      if( graphics->worked() ){
         odomGraphicsByName_t::get().add( graphName, graphics );
         return true ;
      }
      else
         snprintf( errorMsg, errorMsgLen, "Error loading graphics from %s", dir );

      delete graphics ;
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: graphics name path\n" );
   
   return false ;
}

static bool add( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 6 <= numParams ){
      char const *graphName = params[1];
      odomGraphics_t const *graphics = odomGraphicsByName_t::get().get(graphName);
      if( graphics ){
         char const *odomName = params[2];
         unsigned const x = strtoul(params[3], 0, 0);
         unsigned const y = strtoul(params[4], 0, 0);
         unsigned const maxV = strtoul(params[5], 0, 0 );
         
         if( 0 == maxV ){
            snprintf( errorMsg, errorMsgLen, "Invalid maxV value %s", params[5] );
            return false ;
         }

         unsigned const initVal = ( 6 < numParams ) 
                                  ? strtoul(params[6], 0, 0 )
                                  : 0 ;
         odometer_t *newOdom = new odometer_t( *graphics,
                                               initVal, x, y, maxV,
                                               odometerMode_ );
         if( 7 < numParams ){
            unsigned const target = strtoul(params[7], 0, 0 );
            if( target < initVal ){
               snprintf( errorMsg, errorMsgLen, "Invalid target value %s. must be >= %u", params[7], initVal );
               delete newOdom ;
               return false ;
            }

            newOdom->setTarget(target);
         }
         
         odometerSet_t &odoms = odometerSet_t::get();
         odoms.add( odomName, newOdom );
         return true ;
      }
      else {
         snprintf( errorMsg, errorMsgLen, "graphics %s not defined\n", graphName );
      }
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: add graphName odomName x y maxV [initval [target]]" );

   return false ;
}

static bool setTarget( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 3 == numParams ){
      odometerSet_t &odoms = odometerSet_t::get();
      char const *odomName = params[1];
      odometer_t *odom = odoms.get(odomName);
      if( odom ){
         unsigned target = strtoul(params[2], 0, 0 );
         unsigned curVal = odom->value().value();
         if( target < curVal ){
            snprintf( errorMsg, errorMsgLen, "Invalid target value %s. must be >= %u", params[2], curVal );
            return false ;
         }
   
         odom->setTarget(target);
         return true ;
      }
      else {
         snprintf( errorMsg, errorMsgLen, "odometer %s not defined\n", odomName );
      }
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: setTarget odomName targetValue" );

   return false ;
}

static bool setValue( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 3 == numParams ){
      odometerSet_t &odoms = odometerSet_t::get();
      char const *odomName = params[1];
      odometer_t *odom = odoms.get(odomName);
      if( odom ){
         unsigned value = strtoul(params[2], 0, 0 );
         unsigned target = odom->target();
         if( target < value ){
            snprintf( errorMsg, errorMsgLen, "Invalid value %s. must be <= %u", params[2], target );
            return false ;
         }
   
         odom->setValue(value);
         return true ;
      }
      else {
         snprintf( errorMsg, errorMsgLen, "odometer %s not defined\n", odomName );
      }
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: setValue odomName value" );

   return false ;
}

static bool dump( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   odometerSet_t &odoms = odometerSet_t::get();
   odoms.dump();
   if( 2 == numParams ){
      char const *odomName = params[1];
      odometer_t *odom = odoms.get(odomName);
      if( odom ){
         printf( "odom <%s>, value %u, target %u, velocity %u\n",
                 odomName, odom->value().value(), odom->target(), odom->velocity() );
         rectangle_t r = odom->value().getRect();
         printf( "   x:y w:h == %u:%u %u:%u\n", r.xLeft_, r.yTop_, r.width_, r.height_ );
         return true ;
      }
      else {
         snprintf( errorMsg, errorMsgLen, "odometer %s not defined\n", odomName );
      }
   }
   else
      return true ;

   return false ;
}

static bool run( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 1 == numParams ){
      odometerSet_t &odoms = odometerSet_t::get();
      odoms.run();
      return true ;
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: run" );

   return false ;
}

static bool stop( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 1 == numParams ){
      odometerSet_t &odoms = odometerSet_t::get();
      odoms.stop();
      return true ;
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: stop" );

   return false ;
}

static bool cls( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   unsigned long rgb = ( 1 < numParams )
                     ? strtoul( params[1], 0, 0 )
                     : 0 ;
   getFB().clear( (unsigned char)(rgb>>16),
                  (unsigned char)(rgb>>8),
                  (unsigned char)rgb );
   return true ;
}

static bool source( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 2 == numParams ){
      FILE *fIn = fopen( params[1], "rt" );
      if( fIn ){
         bool worked = true ;
         char inBuf[512];

         while( worked && fgets( inBuf, sizeof(inBuf), fIn ) ){
            unsigned const len = strlen(inBuf);
            if( 1 < len ){
               inBuf[len-1] = '\0' ;
               worked = interp.dispatch( inBuf );
            }
         }
         fclose( fIn );
         return worked ;
      }
      else
         snprintf( errorMsg, errorMsgLen, "%s: %m", params[1] );
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: source path" );

   return false ;
}

static bool screenShot( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 2 == numParams ){
      odometerSet_t &odoms = odometerSet_t::get();
      bool wasRunning = odoms.isRunning();
      if( wasRunning )
         odoms.stop();
      
      fbDevice_t &fb = getFB();
      image_t screenImg ;
      rectangle_t r ;
      r.xLeft_ = r.yTop_ = 0 ;
      r.width_ = fb.getWidth();
      r.height_ = fb.getHeight();

      screenImageRect( fb, r, screenImg );

      bool worked = false ;
      void const *pngData ;
      unsigned    pngSize ;
      if( imageToPNG( screenImg, pngData, pngSize ) ){
         char const *outFileName = params[1];
         FILE *fOut = fopen( outFileName, "wb" );
         if( fOut )
         {
            int numWritten = fwrite( pngData, 1, pngSize, fOut );
            fclose( fOut );
            worked = (numWritten == (int)pngSize);
         }
         else
            snprintf( errorMsg, errorMsgLen, "%s:%m", outFileName );
         
         free((void *)pngData);
      }
      else
         snprintf( errorMsg, errorMsgLen, "error converting to PNG" );

      if( wasRunning )
         odoms.run();

      return worked ;
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: screenShot path" );

   return false ;
}


static bool image( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 2 <= numParams ){
      image_t img ;
      if( imageFromFile( params[1], img ) ){
         unsigned x = 0 ;
         if( 3 <= numParams )
            x = strtoul( params[2], 0, 0 );

         unsigned y = 0 ;
         if( 4 <= numParams )
            y = strtoul( params[3], 0, 0 );
         getFB().render( x, y, 
                         img.width_, img.height_, 
                         (unsigned short *)img.pixData_ );
         return true ;
      }
      else
         snprintf( errorMsg, errorMsgLen, "%s: %m", params[1] );
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: image path [x [y]]" );

   return false ;
}

static bool playlist( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 2 <= numParams ){
      return interp.playlist().dispatch( interp, params+1, numParams, errorMsg, errorMsgLen );
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: playlist subcommand [more params...]" );

   return false ;
}

static bool sm501reg( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 2 <= numParams ){
      unsigned long regAddr = strtoul( params[1], 0, 0 );
      fbDevice_t &fb = getFB();
	   
      unsigned long reg = regAddr ;
      int res = ioctl( fb.getFd(), SM501_READREG, &reg );
      if( 0 == res ){
         printf( "sm501reg[0x%08lx] == 0x%08lx\n", regAddr, reg );
         if( 2 < numParams )
         {
            reg_and_value rv ;
            rv.reg_ = regAddr ;
            rv.value_ = strtoul(params[2], 0, 0 );
            printf( "writing value 0x%lx to register 0x%lx\n", rv.value_, rv.reg_ );
            res = ioctl( fb.getFd(), SM501_WRITEREG, &rv );
            if( 0 == res ){
               printf( "done\n" );
               return true ;
            }
            else
               perror( "SM501_WRITEREG" );
         }
         else
            return true ;
      }
      else
         perror( "SM501_READREG" );
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: sm501reg register [value]" );

   return false ;
}

typedef struct fontEntry_t {
   void           *ram_ ;
   freeTypeFont_t *font_ ;
};

typedef std::map<std::string,fontEntry_t *> fontByName_t ;
static fontByName_t fontsByName_ ;

static bool caption( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 6 < numParams ){
      fontEntry_t *entry = fontsByName_[params[1]];
      freeTypeFont_t *font ;
      if( entry && ( 0 != ( font = entry->font_ ) ) ){
         char captionText[512];
         char *next = captionText ;
         char *end = next + sizeof(captionText)-1 ;

         for( unsigned p = 6 ; p < numParams ; p++ ){
            next += snprintf( next, end-next, "%s ", params[p] );
         }
         *next = '\0' ;

         fbDevice_t &fb = getFB();
         unsigned pointSize = strtoul( params[2], 0, 0 );
         unsigned x = strtoul( params[3], 0, 0 );
         unsigned y = strtoul( params[4], 0, 0 );
         unsigned colorIdx = strtoul(params[5], 0, 0 );

         if( ( x < fb.getWidth() )
             &&
             ( y < fb.getHeight() ) ){
            if( colorIdx < 16 ){
               sm501alpha_t &alphaLayer = sm501alpha_t::get(sm501alpha_t::rgba44);
               freeTypeString_t fts( *font, pointSize, captionText, next-captionText, fb.getWidth() );

               printf( "caption at %u:%u, %ux%u, color %u\n",
                       x, y,
                       fts.getWidth(), fts.getHeight(),
                       colorIdx );
               alphaLayer.drawText(                // highlight
                  fts.getRow(0),
                  fts.getWidth(), fts.getHeight(),
                  x-2, y-2, 15 );
               alphaLayer.drawText(                // shadow
                  fts.getRow(0),
                  fts.getWidth(), fts.getHeight(),
                  x+2, y+2, 0 );
               alphaLayer.drawText(                // actual color
                  fts.getRow(0),
                  fts.getWidth(), fts.getHeight(),
                  x, y, colorIdx );
               return true ;
            }
            else
               snprintf( errorMsg, errorMsgLen, "invalid position %u:%u", x, y );
         }
         else
            snprintf( errorMsg, errorMsgLen, "invalid position %u:%u", x, y );
      }
      else
         snprintf( errorMsg, errorMsgLen, "Unknown font %s", params[1] );
   }
   else                                      //   0       1        2      3 4    5        6      ...
      snprintf( errorMsg, errorMsgLen, "Usage: caption fontName pointSize x y colorIdx [param [param...]]\n" );

   return false ;
}

static bool font( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 3 == numParams ){
      memFile_t fFont( params[2] );
      if( fFont.worked() ){
         fontEntry_t *entry = new fontEntry_t ; 
         entry->ram_ = malloc( fFont.getLength() );
         memcpy( entry->ram_, fFont.getData(), fFont.getLength() );
         entry->font_ = new freeTypeFont_t( entry->ram_, fFont.getLength() );
         if( entry->font_->worked() ){
            fontsByName_[params[1]] = entry ;
            return true ;
         }
         else
            snprintf( errorMsg, errorMsgLen, "error parsing font file" );

         free( entry->ram_ );
         delete entry->font_ ;
         delete entry ;
      }
      else
         snprintf( errorMsg, errorMsgLen, "%s:%m", params[2] );
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: font fontName fontFile\n" );

   return false ;
}


static bool clrAlpha( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 5 == numParams ){
      rectangle_t r ;
      r.xLeft_ = strtoul(params[1],0,0);
      r.yTop_  = strtoul(params[2],0,0);
      r.width_ = strtoul(params[3],0,0);
      r.height_ = strtoul(params[4],0,0);
      sm501alpha_t::get(sm501alpha_t::rgba44).clear( r );
      return true ;
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: clrAlpha x y w h\n" );

   return false ;
}

static bool volume( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   if( 2 == numParams ){
      unsigned volume ;
      if( ( 1 == sscanf( params[1], "%u", &volume ) )
          &&
          ( 99 >= volume ) ){
         interp.playlist().setVolume( volume );
         return true ;
      }
      else
         snprintf( errorMsg, errorMsgLen, "Invalid volume %s, should be 0..99\n", params[1] );
   }
   else
      snprintf( errorMsg, errorMsgLen, "Usage: volume 0-99\n" );

   return false ;
}


static commandEntry_t commands_[] = {
   { name_: "args"
   , handler_: args }
,  { name_: "add"
   , handler_: add }
,  { name_: "caption"
   , handler_: caption }
,  { name_: "clrAlpha"
   , handler_: clrAlpha }
,  { name_: "cls"
   , handler_: cls }
,  { name_: "font"
   , handler_: font }
,  { name_: "graphics"
   , handler_: graphics }
,  { name_: "image"
   , handler_: image }
,  { name_: "dump"
   , handler_: dump }
,  { name_: "mode"
   , handler_: mode }
,  { name_: "playlist"
   , handler_: playlist }
,  { name_: "run"
   , handler_: run }
,  { name_: "screenShot"
   , handler_: screenShot }
,  { name_: "setTarget"
   , handler_: setTarget }
,  { name_: "setValue"
   , handler_: setValue }
,  { name_: "sm501reg"
   , handler_: sm501reg }
,  { name_: "source"
   , handler_: source }
,  { name_: "stop"
   , handler_: stop }
,  { name_: "volume"
   , handler_: volume }
};

static unsigned const numCommands_ = sizeof(commands_)/sizeof(commands_[0]);

odomCmdInterp_t::odomCmdInterp_t( odomPlaylist_t &playlist )
   : exit_( false )
   , playlist_( playlist )
{
   errorMsg_[0] = '\0' ;
}
   
odomCmdInterp_t::~odomCmdInterp_t( void )
{
}

#define MAXPARAMS 32

bool odomCmdInterp_t::dispatch( char const *cmdline )
{
   errorMsg_[0] = '\0' ;
   strncpy( inBuf_, cmdline, sizeof(inBuf_)-1 );
   inBuf_[sizeof(inBuf_)-1] = '\0' ;

   char ** const parts = (char **)alloca(MAXPARAMS*sizeof(char *));
   unsigned numParts = 0 ;
   char *nextIn = inBuf_ ;
   char c ;
   while( (MAXPARAMS > numParts) && ( 0 != ( c = *nextIn++ )) ){
      // skip whitespace
      while( isspace(c) )
         c = *nextIn++ ;

      if( '\0' == c )
         break ;

      parts[numParts++] = nextIn-1 ;

      while( c && !isspace(c) )
         c = *nextIn++ ;

      nextIn[-1] = '\0' ;
   }

   if( 0 < numParts ){
      char const *cmd = parts[0];
      for( unsigned i = 0 ; i < numCommands_ ; i++ )
      {
         if( 0 == strcasecmp( cmd, commands_[i].name_ ) )
            return commands_[i].handler_( *this, parts, numParts, errorMsg_, sizeof(errorMsg_) );
      }

      if( ( 0 == strcasecmp( cmd, "die" ) )
          ||
          ( 0 == strcasecmp( cmd, "exit" ) ) ){
         exit_ = true ;
         return true ;
      }
      else {
         snprintf( errorMsg_, sizeof(errorMsg_), "command %s not implemented\n", cmd );
         return false ;
      }
   }
   else {
      snprintf( errorMsg_, sizeof(errorMsg_), "No data parsing cmdline\n" );
      return false ;
   }
}




#ifdef MODULETEST
#include <stdio.h>

int main( int argc, char const * const argv[] )
{
   odomPlaylist_t  playlist ;
   odomCmdInterp_t interp(playlist);

   for( int arg = 1 ; arg < argc ; arg++ ){
      char const *cmd = argv[arg];
      printf( "%s: ", cmd );
      if( interp.dispatch( cmd ) )
         printf( "success\n" );
      else
         printf( "error %s\n", interp.getErrorMsg() );
   }
   
   odometerSet_t::get().run();

   char cmd[512];
   while( !interp.exitRequested() 
          && 
          ( 0 != fgets( cmd, sizeof(cmd), stdin ) ) ){
      cmd[strlen(cmd)-1] = '\0' ; // trim <CR>
      printf( "%s: ", cmd );
      if( interp.dispatch( cmd ) )
         printf( "success\n" );
      else
         printf( "error %s\n", interp.getErrorMsg() );
   }

   return 0 ;
}

#endif
