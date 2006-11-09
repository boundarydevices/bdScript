/*
 * Program tradeShow.cpp
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
 * $Log: tradeShow.cpp,v $
 * Revision 1.1  2006-11-09 17:09:01  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include <stdio.h>
#include "slotWheel.h"
#include "memFile.h"
#include "sm501alpha.h"
#include "imgFile.h"
#include "fbDev.h"
#include "fbCmdFinish.h"
#include "fbCmdListSignal.h"
#include "vsyncSignal.h"
#include "multiSignal.h"
#include "cylinderShadow.h"
#include "tickMs.h"
#include "fbCmdBlt.h"
#include "dictionary.h"
#include "fbcMoveable.h"
#include "debugPrint.h"
#include "ftObjs.h"
#include <string>
#include <map>

typedef dictionary_t<std::string> stringDictionary_t ;
typedef std::map<std::string,freeTypeFont_t *> fontDictionary_t ;

#define NUMSLOTWHEELS 3

enum state_e {
   initialize,
   initComplete,
   startWheels,
   wheelsRunning,
   stopWheels,
   wheelsComplete,
   win0clear,          // clear left and right
   win1slide,          // slide element to upper left
   win2details,        // display element details
   win3hold,           // pause after all details
   win4clear,          // clear screen
   postWin,
   winEndClear,
   lose,
   numStates
};

struct outcome_t {
   unsigned elements_[3];
};

static char const * const stateNames_[numStates] = {
   "initialize",
   "initComplete",
   "startWheels",
   "wheelsRunning",
   "stopWheels",
   "wheelsComplete",
   "win0clear",
   "win1slide",
   "win2details",
   "win3hold",
   "win4clear",
   "postWin",
   "winEndClear",
   "lose"
};

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
   fbPtr_t              initCompleteCmdMem_ ;
   fbPtr_t              wheelCmdMem_ ;
   fbPtr_t              stopWheelsCmdMem_ ;
   fbPtr_t              win0CmdMem_ ;
   fbcMoveable_t       *win1Moveable_ ;
   fbPtr_t              win1CmdMem_ ;
   fbPtr_t              clsCmdMem_ ;
   slotWheel_t         *wheels_[NUMSLOTWHEELS];
   state_e volatile     state_ ;
   unsigned             numElements_ ;
   unsigned             elementHeight_ ;
   unsigned             elementWidth_ ;
   unsigned short      *elementPixels_ ;
   unsigned             centerElementX_ ;
   unsigned             centerElementY_ ;
   unsigned             numStarted_ ;
   long long            startTicks_[NUMSLOTWHEELS];
   long long            restartAt_ ;
   stringDictionary_t   elements_ ;
   unsigned             numOutcomes_ ;
   outcome_t           *outcomes_ ;
   unsigned             outcomeIndex_ ;
   unsigned             winningElement_ ;
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

static char const outcomeTag_[] = {
   "outcomes"
};

static bool readOutcomes( jsData_t const           &cfgData,
                          stringDictionary_t const &elements,
                          unsigned                 &numOutcomes,
                          outcome_t               *&outcomes )
{
   jsval rv ;
   JSObject *outerObj ;
   JSIdArray *array = 0 ;
   if( cfgData.evaluate( outcomeTag_, sizeof(outcomeTag_)-1, rv )
       &&
       JSVAL_IS_OBJECT(rv)
       &&
       ( 0 != ( outerObj = JSVAL_TO_OBJECT(rv) ) ) 
       &&
       ( 0 != ( array = JS_Enumerate( cfgData.cx(), outerObj ) ) ) )
   {
      numOutcomes = array->length ;
      outcomes = new outcome_t[numOutcomes];
debugPrint( "%u outcomes\n", numOutcomes );
      bool worked = true ;

      for( unsigned i = 0 ; worked && ( i < numOutcomes ); i++ ){
         jsval rvOutcome ;
         JSObject *objOutcome ;
         JSIdArray *innerArray ;
         if( JS_LookupElement( cfgData.cx(), outerObj, i, &rvOutcome )
             &&
             JSVAL_IS_OBJECT(rvOutcome)
             &&
             ( 0 != ( objOutcome = JSVAL_TO_OBJECT(rvOutcome ) ) ) 
             &&
             ( 0 != ( innerArray = JS_Enumerate( cfgData.cx(), objOutcome ) ) ) 
             &&
             ( NUMSLOTWHEELS == innerArray->length ) )
         {
            for( unsigned j = 0 ; j < NUMSLOTWHEELS ; j++ ){
               jsval rvName ;
               JSString *sName ;

               if( JS_LookupElement( cfgData.cx(), objOutcome, j, &rvName )
                   &&
                   JSVAL_IS_STRING(rvName)
                   &&
                   ( 0 != ( sName = JSVAL_TO_STRING(rvName ) ) ) )
               {
                  std::string element( JS_GetStringBytes(sName), JS_GetStringLength(sName) );
                  unsigned index ;
                  if( elements.find(element,index) ){
                     outcomes[i].elements_[j] = index ;
                  }
                  else
                  {
                     printf( "Unknown element %s in outcome object [%u][%u]\n", element.c_str(), i, j );
                     worked = false ;
                     break ;
                  }
               }
               else 
               {
                  printf( "Invalid outcome object [%u][%u]\n", i, j );
                  worked = false ;
                  break ;
               }
            }
         }
         else 
         {
            printf( "Invalid outcome object %u\n", i );
            if( JS_IdToValue( cfgData.cx(), array->vector[i], &rvOutcome ) )
            {
               printf( "parsed array element\n" );
               if( JSVAL_IS_OBJECT(rvOutcome) )
               {
                  printf( "object\n" );
                  if( 0 != ( objOutcome = JSVAL_TO_OBJECT(rvOutcome ) ) )
                  {
                     printf( "converted\n" );
                     if( 0 != ( innerArray = JS_Enumerate( cfgData.cx(), objOutcome ) ) ) 
                     {
                        printf( "enumerated\n" );
                        printf( "length: %u\n", innerArray->length );
                     }
                     else
                        printf( "can't enumerate\n" );
                  }
                  else
                     printf( "can't convert\n" );
               }
               else {
                  printf( "not object: type %u\n", JS_TypeOfValue(cfgData.cx(), rvOutcome) );
                  JSString *str ;
                  str = JS_ValueToString(cfgData.cx(), rvOutcome );
                  printf( "value<%s>\n", JS_GetStringBytes(str) );
               }
            }
            else
               printf( "no array element\n" );
            worked = false ;
            break ;
         }
      }

      if( array )
         JS_DestroyIdArray(cfgData.cx(), array);

      if( worked )
         return true ;

      delete outcomes ;
      outcomes = 0 ;
      numOutcomes = 0 ;
   }
   else
      printf( "Error finding outcomes array\n" );

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

static char const postWinTag_[] = {
   "postWin"
};

static bool parsePostWin( jsData_t const           &cfgData,
                          fontDictionary_t   const &fonts,
                          stringDictionary_t const &elements,
                          textOut_t               *&entries, 
                          unsigned                 &numEntries )
{
   return parseTextOutArray( cfgData, postWinTag_, sizeof(postWinTag_)-1, fonts, elements, entries, numEntries );
}

static char const wheelCandyTag_[] = {
   "wheelCandy"
};

static bool parseWheelCandy( 
   jsData_t const           &cfgData,
   fontDictionary_t   const &fonts,
   stringDictionary_t const &elements,
   textOut_t               *&entries, 
   unsigned                 &numEntries )
{
   return parseTextOutArray( cfgData, wheelCandyTag_, sizeof(wheelCandyTag_)-1, fonts, elements, entries, numEntries );
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
      switch( data.state_ ){
         case initComplete:
         {
            for( unsigned i = 0 ; i < data.numWheelCandy_ ; i++ )
            {
               showTextOutEntry( data.wheelCandy_[i], data );
            }

            fbPtr_t &mem = data.initCompleteCmdMem_ ;
            assert( 0 != mem.size() );
            ++vsyncCount ;
            unsigned offset = mem.getOffs();
            int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
            if( numWritten != sizeof( offset ) ){
               perror( "write(cmdListDev_)" );
               exit(1);
            }
            break ;
         }
         case startWheels:
         case wheelsRunning:
         {
            fbPtr_t &mem = data.wheelCmdMem_ ;
            assert( 0 != mem.size() );
            ++vsyncCount ;
            unsigned offset = mem.getOffs();
            int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
            if( numWritten != sizeof( offset ) ){
               perror( "write(cmdListDev_)" );
               exit(1);
            }
            break ;
         }
         case stopWheels:
         {
            fbPtr_t &mem = data.stopWheelsCmdMem_ ;
            assert( 0 != mem.size() );
            ++vsyncCount ;
            unsigned offset = mem.getOffs();
            int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
            if( numWritten != sizeof( offset ) ){
               perror( "write(cmdListDev_)" );
               exit(1);
            }
            break ;
         }
         case wheelsComplete:
         {
            long long now = tickMs();
            if( 0 < (now-data.restartAt_) ){
               // done running, advance state
               assert( data.outcomeIndex_ < data.numOutcomes_ );
               outcome_t const &outcome = data.outcomes_[data.outcomeIndex_];
               if( ( outcome.elements_[0] == outcome.elements_[1] )
                   &&
                   ( outcome.elements_[1] == outcome.elements_[2] ) )
               {
                  data.winningElement_ = outcome.elements_[0];
                  data.state_ = win0clear ;
               }
               else {
                  data.restartAt_ = tickMs() + 1500 ;
                  data.state_ = lose ;
               }
printf( "%s/%s/%s == %s\n", 
        data.elements_[outcome.elements_[0]].c_str(),
        data.elements_[outcome.elements_[1]].c_str(),
        data.elements_[outcome.elements_[2]].c_str(),
        stateNames_[data.state_] );
               data.outcomeIndex_ = data.outcomeIndex_ + 1 ;
               if( data.numOutcomes_ <= data.outcomeIndex_ )
                  data.outcomeIndex_ = 0 ;
            }
            break ;
         }
         case win0clear:
         {
            fbPtr_t &mem = data.win0CmdMem_ ;
            assert( 0 != mem.size() );
            ++vsyncCount ;
            unsigned offset = mem.getOffs();
            int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
            if( numWritten != sizeof( offset ) ){
               perror( "write(cmdListDev_)" );
               exit(1);
            }
            break ;
         }
         case win1slide: {
            debugPrint( "%s...%u:%u\n", stateNames_[data.state_], data.win1X_, data.win1Y_ );
            fbPtr_t &mem = data.win1CmdMem_ ;
            assert( 0 != mem.size() );
            ++vsyncCount ;
            unsigned offset = mem.getOffs();
            int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
            if( numWritten != sizeof( offset ) ){
               perror( "write(cmdListDev_)" );
               exit(1);
            }
            debugPrint( "%u:%u\n", data.win1X_/256, data.win1Y_/256 );
            break ;
         }
         case win2details: {
            long long now = tickMs();
            if( 0 <= (now-data.restartAt_) ){
               elementDetails_t &details = data.elementDetails_[data.winningElement_];
               if( data.detailIndex_ < details.numItems_ ){
                  textOut_t &to = details.items_[data.detailIndex_++];
                  fbDevice_t &fb = getFB();
                  fb.antialias( 
                     to.text_->getRow(0),
                     to.text_->getWidth(),to.text_->getHeight(),
                     fb.getWidth()/3, data.detailYPos_+to.text_->getBaseline(), 
                     fb.getWidth(), fb.getHeight(),
                     0xFF, 0xFF, 0xFF );
                  data.restartAt_ = now + 2000 ;
                  data.detailYPos_ += 2*to.text_->getFontHeight(); // double-space
               }
               else {
                  data.restartAt_ = tickMs() + 3500 ;
                  data.state_ = win3hold ;
               }
            }
            break ;
         }

         case win3hold: {
            long long now = tickMs();
            if( 0 <= (now-data.restartAt_) ){
               data.state_ = win4clear ;
            }
            break ;
         }
         
         case win4clear:
         {
            fbPtr_t &mem = data.clsCmdMem_ ;
            assert( 0 != mem.size() );
            ++vsyncCount ;
            unsigned offset = mem.getOffs();
            int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
            if( numWritten != sizeof( offset ) ){
               perror( "write(cmdListDev_)" );
               exit(1);
            }
            break ;
         }

         case postWin: {
            long long const now = tickMs();
            while( data.postWinIndex_ < data.numPostWinActions_ )
            {
               textOut_t const &to = data.postWinActions_[data.postWinIndex_];
               if( 0 < (now-(data.postWinStart_+to.time_)) ){
                  showTextOutEntry( to, data );
printf( "display post-win action %u\n", data.postWinIndex_ );
printf( "[textOut]: x:%u, y:%u, text:%p, time:%lu, index:%u, color: 0x%04x\n", 
        to.x_, to.y_, to.text_,
        to.time_, to.elementIdx_, to.color_ );
                  data.postWinStart_ += to.time_ ;
                  data.postWinIndex_++ ;
               }
               else
                  break ;
            }

            if( data.postWinIndex_ == data.numPostWinActions_ ){
               data.state_ = winEndClear ;
            }

            break ;
         }

         case winEndClear: {
            fbPtr_t &mem = data.clsCmdMem_ ;
            assert( 0 != mem.size() );
            ++vsyncCount ;
            unsigned offset = mem.getOffs();
            int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
            if( numWritten != sizeof( offset ) ){
               perror( "write(cmdListDev_)" );
               exit(1);
            }
            break ;
         }
         case lose:
         {
            long long now = tickMs();
            if( 0 < (now-data.restartAt_) )
               data.state_ = initComplete ;
             break ;
         }
         default:
            break ;
      }
   } // force alternate vsync->cmdComplete->vsync
}

static void cmdListComplete( int signo, void *param )
{
   ++cmdComplete ;

   unsigned long syncCount ;
   getFB().syncCount( syncCount );

   tradeShowData_t &data = *(tradeShowData_t *)param ;
   switch( data.state_ ){
      case initComplete:
      {
         data.numStarted_ = 0 ;
         long long now = tickMs();
         data.startTicks_[0] = now ;
         data.startTicks_[1] = data.startTicks_[0] + 740 ;
         data.startTicks_[2] = data.startTicks_[1] + 480 ;
         data.state_ = startWheels ;

         break ;
      }
      case startWheels:
      {
         assert( data.numStarted_ < NUMSLOTWHEELS );
         long long now = tickMs();
         long long diff = 0 ;
         while( ( NUMSLOTWHEELS > data.numStarted_ )
                &&
                ( 0 <= (diff = (now-data.startTicks_[data.numStarted_]) )) )
         {
            assert( data.outcomeIndex_ < data.numOutcomes_ );
            outcome_t const &outcome = data.outcomes_[data.outcomeIndex_];
            unsigned elementIdx = outcome.elements_[data.numStarted_];
            int outPos = elementIdx*data.elementHeight_
                       - data.elementHeight_/2 ;
            if( 0 > outPos )
               outPos += data.elementHeight_ *data.numElements_ ;
            data.wheels_[data.numStarted_]->start(outPos);
            data.numStarted_++ ;
         }

         if( NUMSLOTWHEELS == data.numStarted_ )
            data.state_ = wheelsRunning ;
         // intentional fall-through
      }

      case wheelsRunning:
      {
         bool stillRunning = false ;
         for( unsigned w = 0 ; w < NUMSLOTWHEELS ; w++ ){
            slotWheel_t *wheel = data.wheels_[w];
            wheel->tick(syncCount);
            if( wheel->isRunning() ){
               stillRunning = true ;
            }
         }

         if( ( wheelsRunning == data.state_ ) && !stillRunning )
            data.state_ = stopWheels ;
         
         break ;
      }
      case stopWheels:
      {
         data.restartAt_ = tickMs() + 1500 ;
         data.state_ = wheelsComplete ;
         break ;
      }
      
      case win0clear:
      {
         data.restartAt_ = tickMs() + 3000 ;
         data.state_ = win1slide ;
         data.win1X_ = data.centerElementX_*256 ;
         data.win1Y_ = data.centerElementY_*256 ;
         data.win1Moveable_->jumpTo( data.win1X_/256, data.win1Y_/256 );
         data.win1Moveable_->setSourceOrigin( 0, data.winningElement_*data.elementHeight_ );
         break ;
      }

      case win1slide:
      {
         debugPrint( "%s...%u:%u\n", stateNames_[data.state_], data.win1X_, data.win1Y_ );
         data.win1Moveable_->executed();
         if( ( 0 != data.win1X_ ) 
             ||
             ( 0 != data.win1Y_ ) ){
            if( 0 != data.win1X_ ){
               int _256ths = data.win1X_ ;
               _256ths += data.win1XIncr_ ;
               if( 0 > _256ths )
                  _256ths = 0 ;
               data.win1X_ = _256ths ;
               data.win1Moveable_->setX( data.win1X_/256 );
            }
            if( 0 != data.win1Y_ ){
               int _256ths = data.win1Y_ ;
               _256ths += data.win1YIncr_ ;
               if( 0 > _256ths )
                  _256ths = 0 ;
               data.win1Y_ = _256ths ;
               data.win1Moveable_->setY( _256ths/256 );
            }
            data.win1Moveable_->updateCommandList();
         } // still movin'
         else {
            data.state_ = win2details ;
            data.detailIndex_ = 0 ;
            data.detailYPos_ = 0 ;
            data.nextDetailAt_ = tickMs();
         }
         break ;
      }

      case win4clear:
      {
         data.state_ = postWin ;
         data.postWinIndex_ = 0 ;
         data.postWinStart_ = tickMs();
         break ;
      }

      case winEndClear:
         data.state_ = initComplete ;
         break ;

      default:
         break ;
   }
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

               if( data.numElements_ != data.elements_.size() ){
                  printf( "Invalid element count: image: %u, elements[] %u\n",
                          data.numElements_, data.elements_.size() );
                  return -1 ;
               }

               if( !parseFonts( cfgData, data.fonts_ ) ){
                  printf( "Error parsing fonts\n" );
                  return -1 ;
               }

               if( !readOutcomes( cfgData, data.elements_, data.numOutcomes_, data.outcomes_ ) ){
                  printf( "Error parsing outcome array in %s\n", configFileName );
                  return -1 ;
               }
               data.outcomeIndex_ = 0 ;

               if( !parseElementDetails( cfgData, data.fonts_, data.elements_, data.elementDetails_ ) ){
                  printf( "Error parsing element details\n" );
                  return -1 ;
               }

               if( !parsePostWin( cfgData, data.fonts_, data.elements_, 
                                  data.postWinActions_, data.numPostWinActions_ ) )
               {
                  printf( "Error parsing post-win actions\n" );
                  return -1 ;
               }

               if( !parseWheelCandy( cfgData, data.fonts_, data.elements_, 
                                     data.wheelCandy_, data.numWheelCandy_ ) )
               {
                  printf( "Error parsing wheel candy actions\n" );
                  return -1 ;
               }
               printf( "%u wheel candy actions\n", data.numWheelCandy_ );

               fbCommandList_t wheelCmdList ;
               data.state_ = initialize ;

               unsigned const startX = fb.getWidth()/2 - ((3*fbi.width())/2) ;
               unsigned const height = 2*fbi.width();
               unsigned xPos = startX ;
               unsigned yPos = fbi.width()/2 ;
               unsigned const wheelWidth = fbi.width();
               for( unsigned w = 0 ; w < NUMSLOTWHEELS ; w++ ){
                  data.wheels_[w] = new slotWheel_t( jsData_t(cfgData,cfgData,motionObj), 
                                                       wheelCmdList, 0, xPos, yPos, 
                                                       fb.getWidth(), fb.getHeight(),
                                                       fbi, height );
                  xPos += wheelWidth;
               }
               wheelCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               wheelCmdList.push( new fbFinish_t );

               data.wheelCmdMem_ = fbPtr_t( wheelCmdList.size() );
               wheelCmdList.copy( data.wheelCmdMem_.getPtr() );

               unsigned const wheelWidth3 = xPos-startX ;

               fbPtr_t shadow = cylinderShadow( wheelWidth3, height, 2.0, 1.0, height*7/8, height/16 );
               fbImage_t shadowImg( shadow, wheelWidth3, height );

               sm501alpha_t &alphaLayer = sm501alpha_t::get( sm501alpha_t::rgba4444 );
               fbCommandList_t shadowCmdList ;
               shadowCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               shadowCmdList.push( 
                  new fbBlt_t(
                     alphaLayer.fbRamOffset(),
                     startX, yPos, fb.getWidth(), fb.getHeight(),
                     shadowImg, 0, 0, wheelWidth3, height ) 
               );
               shadowCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               shadowCmdList.push( new fbFinish_t );
               data.initCompleteCmdMem_ = fbPtr_t( shadowCmdList.size() );
               shadowCmdList.copy( data.initCompleteCmdMem_.getPtr() );

               fbCommandList_t stopWheelsCmdList ;

/*
               stopWheelsCmdList.push( 
                  new fbCmdClear_t(
                     0, // graphics RAM
                     0, 0, fb.getWidth(), yPos )
               );
*/               
               // clear alpha
               stopWheelsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               stopWheelsCmdList.push( 
                  new fbCmdClear_t(
                     alphaLayer.fbRamOffset(),
                     startX, yPos, wheelWidth3, height )
               );
               
               // clear above and below in graphics space (wheel candy)
               // and top of center wheels
               stopWheelsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               stopWheelsCmdList.push( 
                  new fbCmdClear_t(
                     0,    // graphics RAM
                     0, yPos, fb.getWidth(), data.elementHeight_/2 )
               );
               // and bottom
               stopWheelsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               stopWheelsCmdList.push( 
                  new fbCmdClear_t(
                     0,    // graphics RAM
                     0, yPos+height-(data.elementHeight_/2),
                     fb.getWidth(), fb.getHeight()-yPos-height+(data.elementHeight_/2) )
               );

               stopWheelsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               stopWheelsCmdList.push( new fbFinish_t );
               
               data.stopWheelsCmdMem_ = fbPtr_t( stopWheelsCmdList.size() );
               stopWheelsCmdList.copy( data.stopWheelsCmdMem_.getPtr() );

               //
               // win0clear command list: clear left and right element
               //
               fbCommandList_t win0CmdList ;
               data.centerElementY_ = yPos+(data.elementHeight_/2);
               win0CmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               win0CmdList.push( 
                  new fbCmdClear_t(
                     0, startX,
                     data.centerElementY_,
                     wheelWidth, data.elementHeight_
                  )
               );

               win0CmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               win0CmdList.push( 
                  new fbCmdClear_t(
                     0, startX+(2*wheelWidth),
                     yPos+(data.elementHeight_/2),
                     wheelWidth, data.elementHeight_
                  )
               );

               win0CmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               win0CmdList.push( new fbFinish_t );
               data.win0CmdMem_ = fbPtr_t( win0CmdList.size() );
               win0CmdList.copy( data.win0CmdMem_.getPtr() );

               data.centerElementX_ = startX+wheelWidth ;
               fbCommandList_t win1CmdList ;
               data.win1X_ = data.centerElementX_*256 ;
               data.win1Y_ = data.centerElementY_*256 ;
               unsigned const win1Ticks = 40 ;

               data.win1XIncr_ = 0-(data.centerElementX_*256)/win1Ticks ;
               data.win1YIncr_ = 0-(data.centerElementY_*256)/win1Ticks ;
printf( "moveable %u:%u, %d,%d\n", data.win1X_/256, data.win1Y_/256, data.win1XIncr_, data.win1YIncr_ );
               data.win1Moveable_ = new fbcMoveable_t(
                  win1CmdList,
                  0,       // graphics layer
                  data.centerElementX_, data.centerElementY_,
                  fb.getWidth(), fb.getHeight(),
                  fbi,
                  0, 256,
                  wheelWidth, data.elementHeight_,
                  0x0000      // black
               );
               win0CmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               win1CmdList.push( new fbFinish_t );
               data.win1CmdMem_ = fbPtr_t( win1CmdList.size() );
               win1CmdList.copy( data.win1CmdMem_.getPtr() );

               fbCommandList_t clsCmdList ; // clear screen
               clsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               clsCmdList.push( 
                  new fbCmdClear_t(
                     0, 0, 0, fb.getWidth(), fb.getHeight() )
               );
               clsCmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
               clsCmdList.push( new fbFinish_t );
               data.clsCmdMem_ = fbPtr_t( clsCmdList.size() );
               clsCmdList.copy( data.clsCmdMem_.getPtr() );

               sigset_t blockThese ;
               sigemptyset( &blockThese );

               sigaddset( &blockThese, cmdListDev.getSignal() );
               sigaddset( &blockThese, vsync.getSignal() );

               setSignalHandler( vsync.getSignal(), blockThese, videoOutput, &data );
               setSignalHandler( cmdListDev.getSignal(), blockThese, cmdListComplete, &data );

               cmdListDev_ = &cmdListDev ;

               vsync.enable();
               cmdListDev.enable();

               data.state_ = initComplete ;
               do {
                  pause();
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

