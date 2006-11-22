/*
 * Program tradeShowAux.cpp
 *
 * This program is a trade-show demonstration for Boundary
 * Device and Watters & Associates. 
 *
 * It is also a sample of what you can do with the SM-501 
 * graphics components.
 *
 *
 * Change History : 
 *
 * $Log: tradeShowAux.cpp,v $
 * Revision 1.1  2006-11-22 17:27:53  ericn
 * -Initial import
 *
 * Revision 1.1  2006/11/09 17:09:01  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include <stdio.h>
#include "jsData.h"
#include "memFile.h"
#include "sm501alpha.h"
#include "imgFile.h"
#include "fbDev.h"
#include "fbCmdFinish.h"
#include "fbCmdListSignal.h"
#include "vsyncSignal.h"
#include "multiSignal.h"
#include "tickMs.h"
#include "fbCmdBlt.h"
#include "dictionary.h"
#include "fbcMoveable.h"
#include "debugPrint.h"
#include "ftObjs.h"
#include <string>
#include <map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef dictionary_t<std::string> stringDictionary_t ;
typedef std::map<std::string,freeTypeFont_t *> fontDictionary_t ;

//
// This structure is used to house both the text items for 
// element details as well as the post-win display actions,
// so some fields are not used.
//
struct textOut_t {
   freeTypeString_t *text_ ;
   int               x_ ;
   int               y_ ;
   unsigned long     time_ ;
   bool              hasElement_ ;
   unsigned          elementIdx_ ;
   unsigned short    color_ ;
};

#define MAXTEXTITEMS 16
struct elementDetails_t {
   unsigned    numItems_ ;
   textOut_t   items_[MAXTEXTITEMS];
};

struct tradeShowData_t {
   fbPtr_t              bltCmdMem_ ;
   fbPtr_t              clsCmdMem_ ;
   unsigned             numElements_ ;
   image_t             *elementImages_ ;
   fbImage_t            winningImage_ ;
   unsigned             elementHeight_ ;
   unsigned             elementWidth_ ;
   unsigned short      *elementPixels_ ;
   unsigned             centerElementX_ ;
   unsigned             centerElementY_ ;
   unsigned             numStarted_ ;
   long long            restartAt_ ;
   stringDictionary_t   elements_ ;
   unsigned             defaultElement_ ;
   unsigned volatile    winningElement_ ;
   unsigned             prevWin_ ;
   unsigned             win1X_ ;
   unsigned             win1Y_ ;
   int                  win1XIncr_ ;
   int                  win1YIncr_ ;
   elementDetails_t    *elementDetails_ ;
   long long            nextDetailAt_ ;
   unsigned             detailIndex_ ;
   unsigned             detailYPos_ ;
   textOut_t           *postWinActions_ ;
   unsigned             numPostWinActions_ ;
   unsigned             postWinIndex_ ;
   long long            postWinStart_ ;
   fontDictionary_t     fonts_ ;
   textOut_t           *wheelCandy_ ;
   unsigned             numWheelCandy_ ;
   int                  sockFd_ ;
   sockaddr_in          remote_ ;
};


static char const elementsTag_[] = {
   "elements"
};

static bool readElements( jsData_t const     &cfgData,
                          stringDictionary_t &elements )
{
   jsval rv ;
   JSObject *outerObj ;
   JSIdArray *array = 0 ;
   if( cfgData.evaluate( elementsTag_, sizeof(elementsTag_)-1, rv )
       &&
       JSVAL_IS_OBJECT(rv)
       &&
       ( 0 != ( outerObj = JSVAL_TO_OBJECT(rv) ) ) 
       &&
       ( 0 != ( array = JS_Enumerate( cfgData.cx(), outerObj ) ) ) )
   {
      unsigned numElements = array->length ;
      for( unsigned i = 0 ; i < numElements ; i++ ){
         jsval rvName ;
         JSString *sName ;
         if( JS_LookupElement( cfgData.cx(), outerObj, i, &rvName )
             &&
             ( 0 != ( sName = JS_ValueToString(cfgData.cx(), rvName ) ) ) )
         {
            std::string sElement( JS_GetStringBytes(sName), JS_GetStringLength(sName) );
            elements += sElement ;
         }
      }

      if( array )
         JS_DestroyIdArray(cfgData.cx(), array);
      return numElements == elements.size();
   }

   return false ;
}

static char const elementDetailsTag_[] = {
   "elementDetails"
};

static char const pointsTag_[] = {
   "pts"
};

static char const timeTag_[] = {
   "at"
};

static char const textTag_[] = {
   "text"
};

static char const colorTag_[] = {
   "color"
};

static char const elementTag_[] = {
   "element"
};

static char const fontTag_[] = {
   "font"
};

static char const defaultFontName_[] = {
   "normal" 
};

static void parseTextOut( jsData_t const           &jsDetails,
                          stringDictionary_t const &elements,
                          fontDictionary_t   const &fonts,
                          textOut_t                &to )
{
   memset( &to, 0, sizeof(textOut_t) );

   jsval v ;

   // points and text are needed as a pair
   int points ;
   JSString *sText ;
   if( jsDetails.evaluate( pointsTag_, sizeof(pointsTag_)-1, v )
       &&
       JSVAL_IS_INT( v )
       &&
       ( 0 < ( points = JSVAL_TO_INT( v ) ) ) 
       &&
       jsDetails.evaluate( textTag_, sizeof(textTag_)-1, v )
       &&
       JSVAL_IS_STRING( v )
       &&
       ( 0 != ( sText = JSVAL_TO_STRING( v ) ) ) )
   {
      freeTypeFont_t *font = 0 ;
      JSString *sFont ;
      char const *fontName = defaultFontName_ ;
      if( jsDetails.evaluate( fontTag_, sizeof(fontTag_)-1, v )
          &&
          JSVAL_IS_STRING(v)
          &&
          ( 0 != ( sFont = JSVAL_TO_STRING(v) ) ) )
      {
         fontName = JS_GetStringBytes(sFont);
      }
      fontDictionary_t::const_iterator it = fonts.find(fontName);
      if( it != fonts.end() ){
         font = (*it).second ;
         assert( 0 != font );
         to.text_ = new freeTypeString_t( *font, points, 
                                          JS_GetStringBytes(sText),
                                          JS_GetStringLength(sText),
                                          getFB()  .getWidth() );
         debugPrint( "[textOut]: pts: %u, text %s\n", points, JS_GetStringBytes( sText ) );
      }
      else
         printf( "Unknown font %s\n", fontName );
   }

   if( jsDetails.evaluate( "x", 1, v )
       &&
       JSVAL_IS_INT( v ) ){
      to.x_ = JSVAL_TO_INT( v );
   }

   if( jsDetails.evaluate( "y", 1, v )
       &&
       JSVAL_IS_INT( v ) ){
      to.y_ = JSVAL_TO_INT( v );
   }

   if( jsDetails.evaluate( timeTag_, sizeof(timeTag_)-1, v )
       &&
       JSVAL_IS_INT( v ) ){
      to.time_ = JSVAL_TO_INT( v );
   }

   if( jsDetails.evaluate( colorTag_, sizeof(colorTag_)-1, v )
       &&
       JSVAL_IS_INT( v ) ){
      to.color_ = JSVAL_TO_INT( v );
   }

   to.hasElement_ = false ; 
   JSString *sElement ;
   if( jsDetails.evaluate( elementTag_, sizeof(elementTag_)-1, v )
       &&
       JSVAL_IS_STRING( v )
       &&
       ( 0 != ( sElement = JSVAL_TO_STRING( v ) ) ) )
   {
      if( elements.find( JS_GetStringBytes(sElement), to.elementIdx_ ) )
         to.hasElement_ = true ;
      else
         printf( "Invalid element name %s\n", JS_GetStringBytes(sElement) );
   }
   debugPrint( "[textOut]: x:%u, y:%u, text:%p, time:%lu, index:%u, color: 0x%04x\n", 
               to.x_, to.y_, to.text_,
               to.time_, to.elementIdx_, to.color_ );
}
                               
                               
static bool oneElementDetails( jsData_t const           &jsDetails,
                               stringDictionary_t const &elements,
                               fontDictionary_t const   &fonts,
                               unsigned                  count,
                               elementDetails_t         &details )
{
   details.numItems_ = 0 ;
   for( unsigned i = 0 ; i < count ; i++ ){
      jsval     jsv ;
      JSObject *obj ;
      if( JS_LookupElement( jsDetails, jsDetails, i, &jsv )
          &&
          JSVAL_IS_OBJECT( jsv )
          &&
          ( 0 != ( obj = JSVAL_TO_OBJECT(jsv) ) ) )
      {
         textOut_t &to = details.items_[details.numItems_++];
         
         jsData_t nested( jsDetails, jsDetails, obj );
         parseTextOut( nested, elements, fonts, to );
      }
      else {
         printf( "Error parsing element %u\n", i );
         break ;
      }
   }

   return count == details.numItems_ ;
}

static bool parseElementDetails
   ( jsData_t const           &cfgData,
     fontDictionary_t   const &fonts,
     stringDictionary_t const &elements,
     elementDetails_t        *&details )
{
   bool worked = false ;
   jsval rv ;
   JSObject *outerObj ;
   JSIdArray *array = 0 ;
   if( cfgData.evaluate( elementDetailsTag_, sizeof(elementDetailsTag_)-1, rv )
       &&
       JSVAL_IS_OBJECT(rv)
       &&
       ( 0 != ( outerObj = JSVAL_TO_OBJECT(rv) ) ) 
       &&
       ( 0 != ( array = JS_Enumerate( cfgData.cx(), outerObj ) ) ) )
   {
      if( array->length == (int)elements.size() )
      {
         printf( "parsed %u element details\n", array->length );
         details = new elementDetails_t[ elements.size() ];
         worked = true ;
         for( unsigned i = 0 ; worked && ( i < elements.size() ); i++ )
         {
            jsval velem ;
            JSObject *innerObj ;
            JSIdArray *innerArray ;
            std::string const elementName = elements[i];

            if( cfgData.evaluate( elementName.c_str(), elementName.size(), velem, outerObj ) 
                &&
                JSVAL_IS_OBJECT( velem ) 
                &&
                ( 0 != ( innerObj = JSVAL_TO_OBJECT(velem) ) ) 
                &&
                ( 0 != ( innerArray = JS_Enumerate( cfgData.cx(), innerObj ) ) )
                )
            {
               jsData_t nestedData( cfgData, cfgData, innerObj );
               worked = oneElementDetails( nestedData, elements, fonts, innerArray->length, details[i] );
               if( !worked ){
                  printf( "Invalid details for element %s (%u/%u)\n", 
                          elementName.c_str(), innerArray->length, details[i].numItems_ );
               }
               JS_DestroyIdArray(cfgData.cx(), innerArray );
            }
            else
               printf( "Invalid or missing details for element %s\n", elementName.c_str() );
         }

         JS_DestroyIdArray(cfgData.cx(), array);
      }
      else
         printf( "%u details found, need %u\n", array->length, elements.size() );
   }
   else
      printf( "Error finding elementDetails object in config\n" );

   return worked ;
}

static bool parseTextOutArray
   ( jsData_t const           &cfgData,
     char const               *name,
     unsigned                  nameLen,
     fontDictionary_t   const &fonts,
     stringDictionary_t const &elements,
     textOut_t               *&entries, 
     unsigned                 &numEntries )
{
   entries = 0 ;
   numEntries = 0 ;

   bool worked = false ;
   jsval rv ;
   JSObject *outerObj ;
   JSIdArray *array = 0 ;
   if( cfgData.evaluate( name, nameLen, rv )
       &&
       JSVAL_IS_OBJECT(rv)
       &&
       ( 0 != ( outerObj = JSVAL_TO_OBJECT(rv) ) ) 
       &&
       ( 0 != ( array = JS_Enumerate( cfgData.cx(), outerObj ) ) ) 
       &&
       ( 0 <= array->length) )
   {
      entries = new textOut_t[ array->length ];
      for( unsigned i = 0 ; i < (unsigned)array->length ; i++ )
      {
         jsval     rvobj ;
         JSObject *innerObj ;

         if( JS_LookupElement( cfgData.cx(), outerObj, i, &rvobj )
             &&
             JSVAL_IS_OBJECT( rvobj )
             &&
             ( 0 != ( innerObj = JSVAL_TO_OBJECT( rvobj ) ) ) )
         {
            jsData_t nestedData( cfgData, cfgData, innerObj );
            parseTextOut( nestedData, elements, fonts, entries[i] );
            numEntries++ ;
         }
         else
         {
            printf( "invalid object %s[%u]\n", name, i );
            break ;
         }
      }
      worked = ( numEntries == (unsigned)array->length );
   }
   else
      printf( "Error finding %s array in config\n", name );

   if( !worked && ( 0 != entries ) )
      delete [] entries ;
   
   return worked ;
}

static fbCmdListSignal_t *cmdListDev_ = 0 ;
static unsigned volatile vsyncCount = 0 ;
static unsigned volatile cmdComplete = 0 ;

static void showTextOutEntry( textOut_t const       &to,
                              tradeShowData_t const &data )
{
   fbDevice_t &fb = getFB();
   if( 0 != to.text_ ){
      fb.antialias( 
         to.text_->getRow(0),
         to.text_->getWidth(), to.text_->getHeight(),
         to.x_, to.y_+to.text_->getBaseline(), 
         fb.getWidth(), fb.getHeight(),
         fb.getRed(to.color_), fb.getGreen(to.color_), fb.getBlue(to.color_) );
   }

   if( to.hasElement_ ){
      fb.render( to.x_, to.y_,
                 data.elementWidth_,
                 data.elementHeight_*data.numElements_,
                 data.elementPixels_,
                 0, to.elementIdx_*data.elementHeight_,
                 data.elementWidth_,
                 data.elementHeight_ );
   }
}

static void videoOutput( int signo, void *param )
{
   assert( cmdListDev_ );
   if( vsyncCount == cmdComplete ){
      tradeShowData_t &data = *(tradeShowData_t *)param ;
      if( data.winningElement_ != data.prevWin_ ){
         data.prevWin_ = data.winningElement_ ;
         unsigned cmdOffs ;
         if( data.winningElement_ < data.numElements_ ){
            assert( 0 != data.bltCmdMem_.size() );
            cmdOffs = data.bltCmdMem_.getOffs();
         }
         else {
            assert( 0 != data.clsCmdMem_.size() );
            cmdOffs = data.clsCmdMem_.getOffs();
         }
         int numWritten = write( cmdListDev_->getFd(), &cmdOffs, sizeof( cmdOffs ) );
         if( numWritten == sizeof( cmdOffs ) ){
            ++vsyncCount ;
         } else {
            perror( "write(cmdListDev_)" );
            exit(1);
         }
      }
   } // force alternate vsync->cmdComplete->vsync
}

static void cmdListComplete( int signo, void *param )
{
   ++cmdComplete ;

   unsigned long syncCount ;
   getFB().syncCount( syncCount );

   tradeShowData_t &data = *(tradeShowData_t *)param ;
}

static char const fontsTag_[] = {
   "fonts"
};

static char const nameTag_[] = {
   "name"
};

static char const fileTag_[] = {
   "file"
};

static bool parseFonts( jsData_t const   &cfgData,
                        fontDictionary_t &fonts )
{
   bool worked = false ;
   jsval rv ;
   JSObject *outerObj ;
   JSIdArray *array = 0 ;
   if( cfgData.evaluate( fontsTag_, sizeof(fontsTag_)-1, rv )
       &&
       JSVAL_IS_OBJECT(rv)
       &&
       ( 0 != ( outerObj = JSVAL_TO_OBJECT(rv) ) ) 
       &&
       ( 0 != ( array = JS_Enumerate( cfgData.cx(), outerObj ) ) ) 
       &&
       ( 0 < array->length ) )
   {
      printf( "%u objects in %s array\n", array->length, fontsTag_ );
      worked = true ;
      for( int i = 0 ; i < array->length ; i++ ){
         jsval rvFont ;
         JSObject *objFont ;
         if( JS_LookupElement( cfgData.cx(), outerObj, i, &rvFont )
             &&
             JSVAL_IS_OBJECT(rvFont)
             &&
             ( 0 != ( objFont = JSVAL_TO_OBJECT(rvFont ) ) ) )
         {
            jsval v ;
            JSString *sFontName ;
            JSString *sFileName ;
            if( cfgData.evaluate( nameTag_, sizeof(nameTag_)-1, v, objFont )
                &&
                JSVAL_IS_STRING(v)
                &&
                ( 0 != ( sFontName = JSVAL_TO_STRING(v) ) )
                &&
                cfgData.evaluate( fileTag_, sizeof(fileTag_)-1, v, objFont )
                &&
                JSVAL_IS_STRING(v)
                &&
                ( 0 != ( sFileName = JSVAL_TO_STRING(v) ) ) )
            {
               char const * const fontName = JS_GetStringBytes(sFontName);
               char const * const fileName = JS_GetStringBytes(sFileName);
               memFile_t fFont( fileName );
               if( fFont.worked() ){
                  freeTypeFont_t *font = new freeTypeFont_t( fFont.getData(), fFont.getLength() );
                  if( font->worked() ){
                     printf( "name: %s, file: %s\n", fontName, fileName );
                     fonts[fontName] = font ;
                  }
                  else {
                     printf( "Error parsing font [%u]%s: %s\n", i, fontName, fileName );
                     worked = false ;
                     break ;
                  }
               }
               else {
                  printf( "[%u]%s: %s: %m\n", i, fontName, fileName );
                  worked = false ;
                  break ;
               }
            }
            else {
               printf( "Font %u missing %s or %s tag\n", i, nameTag_, fileTag_ );
               worked = false ;
               break ;
            }

         }
         else {
            printf( "Invalid font[%d]\n", i );
            worked = false ;
            break ;
         }
      }
   }
   else
      printf( "Missing fonts array in config\n" );

   printf( "%u fonts loaded\n", fonts.size() );
   return worked ;
}

static char const wheelMotion[] = {
   "wheelMotion"
};

#define FONTFILENAME "/mmc/b018032l.pfm"

static char const defaultElementTag_[] = {
   "defaultElement"
};

int main( int argc, char const * const argv[] )
{
   if( 3 < argc )
   {
//      sm501alpha_t &alphaLayer = sm501alpha_t::get(sm501alpha_t::rgba4444);

      char const *configFileName = argv[1];
      memFile_t fConfig( configFileName );
      if( fConfig.worked() ){
         jsval     rv ;
         JSObject *motionObj ;

         jsData_t cfgData( (char const *)fConfig.getData(), fConfig.getLength(), configFileName );
         if( cfgData.initialized() 
             &&
             cfgData.evaluate( wheelMotion, sizeof(wheelMotion)-1, rv )
             &&
             JSVAL_IS_OBJECT(rv)
             &&
             ( 0 != ( motionObj = JSVAL_TO_OBJECT(rv) ) ) ){
            image_t image ;
            char const *imgFileName = argv[2];
            if( imageFromFile( imgFileName, image ) )
            {
               fbDevice_t &fb = getFB();
               fb.clear( 0, 0, 0 );

               memFile_t fFont( argv[3] );
               if( !fFont.worked() ){
                  perror( argv[3] );
                  return -1 ;
               }

               freeTypeFont_t font( fFont.getData(), fFont.getLength() );
               if( !font.worked() ){
                  printf( "Error parsing font %s\n", argv[3] );
                  return -1 ;
               }

               vsyncSignal_t &vsync = vsyncSignal_t::get();
               if( !vsync.isOpen() ){
                  perror( "vsyncSignal" );
                  return -1 ;
               }

               fbCmdListSignal_t &cmdListDev = fbCmdListSignal_t::get();
               if( !cmdListDev.isOpen() ){
                  perror( "cmdListDev" );
                  return -1 ;
               }

               fbImage_t fbi( image, fbi.rgb565 );
debugPrint( "image %ux%u\n", fbi.width(), fbi.height() );
               tradeShowData_t data ;

               if( !readElements( cfgData, data.elements_ ) ){
                  printf( "Error parsing elements array in %s\n", configFileName );
                  return -1 ;
               }

               data.numElements_ = fbi.height()/fbi.width(); // element boxes are square
               data.elementHeight_ = fbi.height()/data.numElements_ ;
               data.elementWidth_  = fbi.width();
               data.elementPixels_ = (unsigned short *)fbi.pixels();
               data.winningElement_ = data.numElements_ ;
               data.prevWin_        = 0 ;

               if( data.numElements_ != data.elements_.size() ){
                  printf( "Invalid element count: image: %u, elements[] %u\n",
                          data.numElements_, data.elements_.size() );
                  return -1 ;
               }

               if( !parseFonts( cfgData, data.fonts_ ) ){
                  printf( "Error parsing fonts\n" );
                  return -1 ;
               }

               if( !parseElementDetails( cfgData, data.fonts_, data.elements_, data.elementDetails_ ) ){
                  printf( "Error parsing element details\n" );
                  return -1 ;
               }

               data.elementImages_ = new image_t [data.numElements_];

               for( unsigned i = 0 ; i < data.numElements_ ; i++ ){
                  std::string imgFileName = data.elements_[i];
                  imgFileName += ".png" ;
                  if( imageFromFile( imgFileName.c_str(), data.elementImages_[i] ) ){
                     printf( "%s\n", imgFileName.c_str() );
                  }
                  else {
                     perror( imgFileName.c_str() );
                     return -1 ;
                  }
               }


               jsval jsv ;
               if( !cfgData.evaluate( defaultElementTag_, sizeof(defaultElementTag_)-1, jsv )
                   ||
                   !JSVAL_IS_INT( jsv ) ){
                  printf( "Missing or invalid %s\n", defaultElementTag_ );
                  if( cfgData.evaluate( defaultElementTag_, sizeof(defaultElementTag_), jsv ) )
                     printf( "data type is %d\n", JS_TypeOfValue(cfgData, jsv) );
                  else
                     printf( "not found\n" );
                  return -1 ;
               }
               data.defaultElement_ = JSVAL_TO_INT(jsv);
               if( data.defaultElement_ >= data.numElements_ ){
                  printf( "Invalid %s\n", defaultElementTag_ );
                  return -1 ;
               }

               data.sockFd_ = socket( AF_INET, SOCK_DGRAM, 0 );
               int doit = 1 ;
               setsockopt( data.sockFd_, SOL_SOCKET, SO_BROADCAST, &doit, sizeof( doit ) );
               sockaddr_in local ;
               local.sin_family      = AF_INET ;
               local.sin_addr.s_addr = INADDR_ANY ;
               local.sin_port        = 0xBDBD ;
               bind( data.sockFd_, (struct sockaddr *)&local, sizeof( local ) );

               unsigned xPos = (fb.getWidth()-fb.getHeight())/2 ;
               fbCommandList_t clsCmdList ; // clear screen
               clsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               clsCmdList.push( 
                  new fbCmdClear_t(
                     0, 0, 0, fb.getWidth(), fb.getHeight(), 0xFFFF )
               );
               clsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               clsCmdList.push( 
                  new fbBlt_t(
                     0, // graphics ram
                     xPos, 0, fb.getWidth(), fb.getHeight(),
                     data.winningImage_, 0, 0, data.winningImage_.width(), data.winningImage_.height() ) 
               );
               clsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               clsCmdList.push( new fbFinish_t );
               data.clsCmdMem_ = fbPtr_t( clsCmdList.size() );
               clsCmdList.copy( data.clsCmdMem_.getPtr() );

               fbCommandList_t bltCmdList ; // clear screen
               data.winningImage_ = fbImage_t( data.elementImages_[0], fbImage_t::rgb565 );
               bltCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               bltCmdList.push( 
                  new fbBlt_t(
                     0, // graphics ram
                     xPos, 0, fb.getWidth(), fb.getHeight(),
                     data.winningImage_, 0, 0, data.winningImage_.width(), data.winningImage_.height() ) 
               );
               bltCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               bltCmdList.push( new fbFinish_t );
               data.bltCmdMem_ = fbPtr_t( bltCmdList.size() );
               bltCmdList.copy( data.bltCmdMem_.getPtr() );

               sigset_t blockThese ;
               sigemptyset( &blockThese );

               sigaddset( &blockThese, cmdListDev.getSignal() );
               sigaddset( &blockThese, vsync.getSignal() );

               setSignalHandler( vsync.getSignal(), blockThese, videoOutput, &data );
               setSignalHandler( cmdListDev.getSignal(), blockThese, cmdListComplete, &data );

               cmdListDev_ = &cmdListDev ;

               vsync.enable();
               cmdListDev.enable();

               do {
                  char inBuf[512];
                  int numRead ;
                  if( 0 < ( numRead = recv( data.sockFd_, inBuf, sizeof(inBuf), 0 ) ) ){
                     inBuf[numRead] = 0 ;
                     printf( "rx: <%s>\n", inBuf );
                     std::string sIn(inBuf,numRead);
                     data.winningImage_ = fbImage_t();
                     unsigned idx ;
                     if( !data.elements_.find(sIn,idx) ){
                        printf( "element %s not found\n", inBuf );
                        idx = data.defaultElement_ ;
                     }
                     image_t const &img = data.elementImages_[idx];
                     data.winningImage_ = fbImage_t( img, fbImage_t::rgb565 );
                     data.winningElement_ = idx ;
                     printf( "element %u: %s, offset %u\n", idx, inBuf, data.winningImage_.ramOffset() );
                  }
               } while( 1 );
               
               vsync.disable();
               cmdListDev.disable();
            }
            else
               perror( imgFileName );
         }
         else
            fprintf( stderr, "%s: %s\n", configFileName, cfgData.errorMsg() );
      }
      else
         perror( configFileName );
   }
   else
      fprintf( stderr, "Usage: slotWheel dataFile imgFile font [x y h]\n" );

   return 0 ;
}

