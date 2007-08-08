/*
 * Module odometer.cpp
 *
 * This module defines the methods of the odometer_t
 * and odometerSet_t classes as declared in odometer.h
 *
 * Change History : 
 *
 * $Log: odometer.cpp,v $
 * Revision 1.12  2007-08-08 17:08:26  ericn
 * -[sm501] Use class names from sm501-int.h
 *
 * Revision 1.11  2006/10/10 20:51:10  ericn
 * -remove deprecated test program
 *
 * Revision 1.10  2006/09/24 16:26:15  ericn
 * -remove vsync propagation (use multiSignal instead)
 *
 * Revision 1.9  2006/09/23 15:30:01  ericn
 * -Use multiSignal module
 *
 * Revision 1.8  2006/09/17 15:55:23  ericn
 * -no log by default
 *
 * Revision 1.7  2006/09/10 01:15:19  ericn
 * -trace events
 *
 * Revision 1.6  2006/09/01 17:36:33  ericn
 * -blt video earlier to reduce tearing
 *
 * Revision 1.5  2006/08/16 02:31:35  ericn
 * -rewrite based on command lists
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "odometer.h"
#include <map>
#include <string>
#include "fbCmdFinish.h"
#include <sys/ioctl.h>
#include <signal.h>
#include "fbDev.h"
#include <fcntl.h>
#include "rtSignal.h"
//#define LOGTRACES
#include "logTraces.h"
#include "multiSignal.h"

static traceSource_t traceVsync( "vsync" );
static traceSource_t traceCmdList( "cmdList" );

odometer_t::odometer_t
   ( odomGraphics_t const &graphics,
     unsigned              initValue,
     unsigned              x,
     unsigned              y,
     unsigned              maxVelocity,
     odometerMode_e        mode )
   : cmdList_()
   , cmdListMem_()
   , target_( initValue )
   , value_( cmdList_, graphics, x, y, mode )
   , velocity_( 1 )
   , maxVelocity_( maxVelocity )
{
   cmdList_.push( new fbFinish_t );

   cmdListMem_ = fbPtr_t( cmdList_.size() );
   cmdList_.copy( cmdListMem_.getPtr() );

   value_.set( initValue );
}

odometer_t::~odometer_t( void )
{
}

void odometer_t::setValue( unsigned newValue )
{
   value_.set( newValue );
}

void odometer_t::setTarget( unsigned newValue )
{
   target_ = newValue ;
}


static unsigned log2(unsigned long v)
{
   unsigned bits = 31 ;
   unsigned long mask = 0x80000000 ;
   while( 0 == ( v & mask ) ){
      bits-- ;
      mask >>= 1 ;
   }
   return bits ;
}

static unsigned calcVelocity( unsigned target, unsigned value, unsigned maxV )
{
   unsigned v ;
/*
   log2 of difference used as 32nd multiplier against maxV
   
   unsigned l2 = log2( target-value );
   v = (l2*maxV)/32 ;
*/

   /*
    * maxV unless we're within 1024
    */
   unsigned const diff = target-value ;
   if( diff & 0xfffffc00 )
      v = maxV ;
   else
      v = (diff*maxV)/1024 ;

   if( 0 == v )
      v = 1 ;

   return v ;
}

void odometer_t::advance( unsigned numTicks )
{
   while( ( target_ > value_.value() ) && numTicks-- ){
      velocity_ = calcVelocity(target_, value_.value(), maxVelocity_ );
      value_.advance(velocity_);
   }
}

static odometerSet_t *inst_ = 0 ;

odometerSet_t &odometerSet_t::get(){
   if( 0 == inst_ )
      inst_ = new odometerSet_t ;

   return *inst_ ;
}

typedef std::map<std::string,odometer_t *> odomsByName_t ;
static odomsByName_t odomsByName_ ;

void odometerSet_t::add( char const *name, odometer_t *odom )
{
   unsigned const prevSize = odomsByName_.size();
   odomsByName_[name] = odom ;
   if( odomsByName_.size() != prevSize ){
      if( cmdList_ )
         delete [] cmdList_ ;
      cmdList_ = new unsigned long [odomsByName_.size()];
      unsigned next = 0 ;
      cmdListBytes_ = 0 ;
      for( odomsByName_t::const_iterator it = odomsByName_.begin()
           ; it != odomsByName_.end()
           ; it++ ){
         odometer_t *odom = (*it).second ;
         if( odom ){
            cmdList_[next++] = odom->cmdListOffs();
            cmdListBytes_ += sizeof(*cmdList_);
         }
      }
   }
}

odometer_t *odometerSet_t::get( char const *name )
{
   return odomsByName_[name];
}

void odometerSet_t::setValue( char const *name, unsigned pennies )
{
   odometer_t *odom = odomsByName_[name];
   if( odom )
      odom->setValue( pennies );
}

void odometerSet_t::setTarget( char const *name, unsigned pennies )
{
   odometer_t *odom = odomsByName_[name];
   if( odom )
      odom->setTarget( pennies );
}

unsigned long odometerSet_t::syncCount(void) const {
   return prevSync_ ;
}

unsigned maxSigDepth = 0 ;
unsigned sigDepth = 0 ;

void odometerSet_t::sigio(void){
   fprintf( stderr, "sigio (overflow). pending signals follow:\n" );
   sigset_t sigs ;
   sigfillset( &sigs );
   siginfo_t info ;
   struct timespec ts ;
   ts.tv_sec = 0 ;
   ts.tv_nsec = 0 ;
   int signum ;
   while( 0 <= ( signum = sigtimedwait( &sigs, &info, &ts ) ) ){
      printf( "   signal %d/%d, errno %d, fd %d\n",
              info.si_signo, signum,
              info.si_errno,
              info.si_fd );
   }

   sigpending( &sigs );
   for( signum = minRtSignal(); signum <= maxRtSignal(); signum++ ){
      if( 0 == sigismember( &sigs, signum ) )
         printf( "~" );
      printf( "%d\n", signum );
   }
//   exit(1);
}

void odometerSet_t::sigCmdList(void){
   if( ++sigDepth > maxSigDepth )
      maxSigDepth = sigDepth ;

   ++completionCount_ ;
   if( !stopping_ ){
      unsigned long curSync ;
      getFB().syncCount( curSync );

      unsigned long elapsedTicks = curSync-prevSync_ ;
      prevSync_ = curSync ;

      for( odomsByName_t::const_iterator it = odomsByName_.begin()
           ; it != odomsByName_.end()
           ; it++ ){
         odometer_t *odom = (*it).second ;
         if( odom ){
            odom->advance(elapsedTicks);
         }
      }
   }

   --sigDepth ;
}

void odometerSet_t::sigVsync(void){
   
   if( ++sigDepth > maxSigDepth )
      maxSigDepth = sigDepth ;

   if( !stopping_ 
       && 
       ( 0 < cmdListBytes_ ) 
       &&
       ( issueCount_ == completionCount_ ) )
   {
      TRACEINCR( traceCmdList );
      int numWritten = write( fdCmd_, cmdList_, cmdListBytes_ );
      if( cmdListBytes_ == (unsigned)numWritten )
         ++issueCount_ ;
   }

   --sigDepth ;
}

static void cmdListHandler( int signo, void *param )
{
   odometerSet_t *odoms = (odometerSet_t *)param ;
   if( odoms )
      odoms->sigCmdList();
   TRACEDECR( traceCmdList );
}

static void vsyncHandler( int signo, void *param )
{
   TRACE_T( traceVsync, trace );
   odometerSet_t *odoms = (odometerSet_t *)param ;
   if( odoms )
      odoms->sigVsync();
}

void odometerSet_t::stop( void )
{
   stopping_ = true ;
   isRunning_ = false ;

   while( completionCount_ != issueCount_ ){
      pause();
   }

   sigset_t signals ;
   sigemptyset( &signals );
   sigaddset( &signals, cmdListSignal_ );
   sigaddset( &signals, vsyncSignal_ );
   sigprocmask( SIG_BLOCK, &signals, 0 );
   stopping_ = false ;
}

void odometerSet_t::run( void )
{
   sigset_t signals ;
   sigemptyset( &signals );
   sigaddset( &signals, cmdListSignal_ );
   sigaddset( &signals, vsyncSignal_ );
   sigprocmask( SIG_UNBLOCK, &signals, 0 );
   isRunning_ = true ;
}

odometerSet_t::odometerSet_t( void )
   : cmdListSignal_( nextRtSignal() )
   , vsyncSignal_( nextRtSignal() )
   , pid_( getpid() )
   , fdCmd_( open("/dev/" SM501CMD_CLASS, O_RDWR ) )
   , fdSync_( open("/dev/" SM501INT_CLASS, O_RDONLY ) )
   , cmdList_( 0 )
   , cmdListBytes_( 0 )
   , stopping_( false )
   , issueCount_( 0 )
   , completionCount_( 0 )
   , isRunning_(false)
{
   if( isOpen() )
   {
      printf( "signals: cmdList(%d), vsync(%d)\n", cmdListSignal_, vsyncSignal_ );
      getFB().syncCount( prevSync_ );

      stop();
   
      fcntl(fdCmd_, F_SETOWN, pid_ );
      fcntl(fdCmd_, F_SETSIG, cmdListSignal_ );
      
      sigset_t blockThese ;
      sigaddset( &blockThese, cmdListSignal_ );
      sigaddset( &blockThese, vsyncSignal_ );

      setSignalHandler( cmdListSignal_, blockThese, cmdListHandler, this );
   
      fcntl(fdSync_, F_SETOWN, pid_);
      fcntl(fdSync_, F_SETSIG, vsyncSignal_ );
      
      setSignalHandler( vsyncSignal_, blockThese, vsyncHandler, this );

      int flags = fcntl( fdCmd_, F_GETFL, 0 );
      fcntl( fdCmd_, F_SETFL, flags | O_NONBLOCK | FASYNC );
      flags = fcntl( fdSync_, F_GETFL, 0 );
      fcntl( fdSync_, F_SETFL, flags | O_NONBLOCK | FASYNC );
   }
}

void odometerSet_t::dump( void )
{
   printf( "max signal depth: %u\n", maxSigDepth );
   
   odomsByName_t::const_iterator it = odomsByName_.begin();
   for( ; it != odomsByName_.end(); it++ ){
      printf( "%s\n", (*it).first.c_str() );
   }
}
