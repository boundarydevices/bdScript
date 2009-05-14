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
 * Revision 1.4  2009-05-14 16:25:41  ericn
 * [trivial] Add include file to match latest glibc
 *
 * Revision 1.3  2006-12-13 21:22:07  ericn
 * -add text messages
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
#include <string>
#include <assert.h>

typedef dictionary_t<std::string> stringDictionary_t ;

#define NUMSLOTWHEELS 3

enum state_e {
   initialize,
   initComplete,
   startWheels,
   wheelsRunning,
   stopWheels,
   wheelsComplete,
   win0,          // clear left and right
   win1,          // slide element to upper left
   win,
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
   "win0",
   "win1",
   "win",
   "lose"
};

struct tradeShowData_t {
   fbPtr_t              initCompleteCmdMem_ ;
   fbPtr_t              wheelCmdMem_ ;
   fbPtr_t              stopWheelsCmdMem_ ;
   fbPtr_t              win0CmdMem_ ;
   slotWheel_t         *wheels_[NUMSLOTWHEELS];
   state_e volatile     state_ ;
   unsigned             numElements_ ;
   unsigned             elementHeight_ ;
   unsigned             numStarted_ ;
   long long            startTicks_[NUMSLOTWHEELS];
   long long            restartAt_ ;
   stringDictionary_t   elements_ ;
   unsigned             numOutcomes_ ;
   outcome_t           *outcomes_ ;
   unsigned             outcomeIndex_ ;
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
printf( "%u outcomes\n", numOutcomes );
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

static fbCmdListSignal_t *cmdListDev_ = 0 ;
static unsigned volatile vsyncCount = 0 ;
static unsigned volatile cmdComplete = 0 ;

static void videoOutput( int signo, void *param )
{
   assert( cmdListDev_ );

   if( vsyncCount == cmdComplete ){
      tradeShowData_t &data = *(tradeShowData_t *)param ;
      switch( data.state_ ){
         case initComplete:
         {
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
                  data.state_ = win0 ;
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
         case win0:
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
         case win1: {
            long long now = tickMs();
            if( 0 < (now-data.restartAt_) ){
               printf( "win1\n" );
               data.state_ = win ;
            }
            break ;
         }
         case win:
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
      
      case win0:
      {
         data.restartAt_ = tickMs() + 3000 ;
         data.state_ = win1 ;
      }

      default:
         break ;
   }
}

static char const wheelMotion[] = {
   "wheelMotion"
};

int main( int argc, char const * const argv[] )
{
   if( 2 < argc )
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
printf( "image %ux%u\n", fbi.width(), fbi.height() );
               tradeShowData_t data ;

               if( !readElements( cfgData, data.elements_ ) ){
                  printf( "Error parsing elements array in %s\n", configFileName );
                  return -1 ;
               }

               data.numElements_ = fbi.height()/fbi.width(); // element boxes are square
               data.elementHeight_ = fbi.height()/data.numElements_ ;

               if( data.numElements_ != data.elements_.size() ){
                  printf( "Invalid element count: image: %u, elements[] %u\n",
                          data.numElements_, data.elements_.size() );
                  return -1 ;
               }

               if( !readOutcomes( cfgData, data.elements_, data.numOutcomes_, data.outcomes_ ) ){
                  printf( "Error parsing outcome array in %s\n", configFileName );
                  return -1 ;
               }
               data.outcomeIndex_ = 0 ;

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
               wheelCmdList.push( new fbFinish_t );
               data.wheelCmdMem_ = fbPtr_t( wheelCmdList.size() );
               wheelCmdList.copy( data.wheelCmdMem_.getPtr() );

               unsigned const wheelWidth3 = xPos-startX ;

               fbPtr_t shadow = cylinderShadow( wheelWidth3, height, 2.0, 1.0, height*7/8, height/16 );
               fbImage_t shadowImg( shadow, wheelWidth3, height );

               sm501alpha_t &alphaLayer = sm501alpha_t::get( sm501alpha_t::rgba4444 );
               fbCommandList_t shadowCmdList ;
               shadowCmdList.push( 
                  new fbBlt_t(
                     alphaLayer.fbRamOffset(),
                     startX, yPos, fb.getWidth(), fb.getHeight(),
                     shadowImg, 0, 0, wheelWidth3, height ) 
               );
               shadowCmdList.push( new fbFinish_t );
               data.initCompleteCmdMem_ = fbPtr_t( shadowCmdList.size() );
               shadowCmdList.copy( data.initCompleteCmdMem_.getPtr() );

               fbCommandList_t stopWheelsCmdList ;

               // clear alpha
               stopWheelsCmdList.push( 
                  new fbCmdClear_t(
                     alphaLayer.fbRamOffset(),
                     startX, yPos, wheelWidth3, height )
               );

               // clear top of center wheels
               stopWheelsCmdList.push( 
                  new fbCmdClear_t(
                     0, startX, yPos, wheelWidth3, data.elementHeight_/2 )
               );
               // and bottom
               stopWheelsCmdList.push( 
                  new fbCmdClear_t(
                     0, startX, yPos+(3*data.elementHeight_)/2, 
                     wheelWidth3, data.elementHeight_/2 )
               );
               stopWheelsCmdList.push( new fbFinish_t );
               data.stopWheelsCmdMem_ = fbPtr_t( stopWheelsCmdList.size() );
               stopWheelsCmdList.copy( data.stopWheelsCmdMem_.getPtr() );

               //
               // win0 command list: clear left and right element
               //
               fbCommandList_t win0CmdList ;
               win0CmdList.push( 
                  new fbCmdClear_t(
                     0, startX,
                     yPos+(data.elementHeight_/2),
                     wheelWidth, data.elementHeight_
                  )
               );

               win0CmdList.push( 
                  new fbCmdClear_t(
                     0, startX+(2*wheelWidth),
                     yPos+(data.elementHeight_/2),
                     wheelWidth, data.elementHeight_
                  )
               );

               win0CmdList.push( new fbFinish_t );
               data.win0CmdMem_ = fbPtr_t( win0CmdList.size() );
               win0CmdList.copy( data.win0CmdMem_.getPtr() );

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
      fprintf( stderr, "Usage: slotWheel dataFile imgFile [x y h]\n" );

   return 0 ;
}

