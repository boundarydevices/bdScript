/*
 * Module trace.cpp
 *
 * This module defines the startTrace() and endTrace()
 * routines as declared in trace.h
 *
 *
 * Change History : 
 *
 * $Log: trace.cpp,v $
 * Revision 1.2  2006-08-23 15:47:21  ericn
 * -use more specific name for callback
 *
 * Revision 1.1  2006/08/21 12:37:15  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "trace.h"
#include <signal.h>
#include <map>
#include <set>
#include <execinfo.h>
#include "hexDump.h"

typedef std::map<unsigned long,unsigned long> longByLong_t ;
static longByLong_t traceAddrs_ ;
static unsigned long volatile numAlarms_ = 0 ;
static unsigned long volatile numAddrs_ = 0 ;
static traceCallback_t callback_ = 0 ;
static void           *callbackParam_ = 0 ; 

bool traceEntry_t::operator<(traceEntry_t const &rhs) const {
   return this->count_ < rhs.count_ ;
};

typedef std::multiset<traceEntry_t> addressesByCount_t ;

bool volatile done_ = false ;
bool first = false ; // set true to capture first frame

static void handler(int signo, siginfo_t *info, void *context )
{
   if( 0 == traceAddrs_.size() ){
      hexDumper_t dumpStack( &signo, 512, (unsigned long)&signo );
      while( dumpStack.nextLine() )
         printf( "%s\n", dumpStack.getLine() );
   }

   void *btArray[128];
   int const btSize = backtrace(btArray, sizeof(btArray) / sizeof(btArray[0]) );
   if (btSize > 0)
   {
      for (int i = 0 ; i < btSize ; i++ )
         traceAddrs_[(unsigned long)btArray[i]]++ ;
      ++numAlarms_ ;
      numAddrs_ += btSize ;
   }
   
   struct itimerval     itimer;
   itimer . it_interval . tv_sec = 0;
   itimer . it_interval . tv_usec = 0;
   itimer . it_value . tv_sec = 0 ;
   itimer . it_value . tv_usec = 10000 ;

   if( !done_ )
   {
      if( setitimer (ITIMER_REAL, &itimer, NULL) < 0)
      {
         perror ("StartTimer could not setitimer");
         exit (0);
      }
   }

   if( callback_ )
      callback_(callbackParam_);
}

void dumpTraces( traceEntry_t const *traces,
                 unsigned            count )
{
   while( 0 < count-- ){
      traceEntry_t const &t = *traces++ ;
      char **symbolName = backtrace_symbols( (void **)&t.pc_, 1 );
      printf( "%10ld\t%p\t%s\n", t.count_, (void *)t.pc_, symbolName[0] );
   }
}


void startTrace(callback_t cb, void *cbParam){
   callback_ = cb ;
   callbackParam_ = cbParam ; 

   struct itimerval     itimer;

   /*
    * Set up a signal handler for the SIGALRM signal which is what the
    * expiring timer will set.
    */

   struct sigaction sa ;
   sa.sa_flags = SA_SIGINFO|SA_RESTART ;
   sa.sa_restorer = 0 ;
   sigemptyset( &sa.sa_mask );
   sa.sa_sigaction = handler ;
   sigaction(SIGALRM, &sa, 0 );

   /*
    * Set the timer up to be non repeating, so that once it expires, it
    * doesn't start another cycle.  What you do depends on what you need
    * in a particular application.
    */

   itimer . it_interval . tv_sec = 0;
   itimer . it_interval . tv_usec = 0;
 
   /*
    * Set the time to expiration to interval seconds.
    * The timer resolution is milliseconds.
    */
   
   itimer . it_value . tv_sec = 0 ;
   itimer . it_value . tv_usec = 100000 ;


   if (setitimer (ITIMER_REAL, &itimer, NULL) < 0)
   {
      perror ("StartTimer could not setitimer");
      exit (0);
   }

   traceAddrs_.clear();
   numAlarms_ = 0 ;
   numAddrs_ = 0 ;
}


traceEntry_t *endTrace( unsigned &count )
{
   unsigned long const start = numAlarms_ ;
   done_ = true ;
   while( numAlarms_ == start )
      pause();

   printf( "------------- trace long ---------------\n"
           "%lu address hits\n"
           "iterations\taddress\n", numAddrs_ );

   addressesByCount_t byCount ;
   longByLong_t :: const_iterator it = traceAddrs_.begin();
   for( ; it != traceAddrs_.end(); it++ )
   {
      traceEntry_t t ;
      t.pc_    = (*it).first ;
      t.count_ = (*it).second ;
      byCount.insert( t );
   }

   traceEntry_t *const returnVal = new traceEntry_t[traceAddrs_.size()];
   traceEntry_t *nextOut = returnVal ;

   unsigned long sum = 0 ;
   addressesByCount_t :: reverse_iterator rit = byCount.rbegin();
   for( ; rit != byCount.rend(); rit++ )
   {
      *nextOut++ = (*rit);
      sum += (*rit).count_ ;
   }
   printf( "sum == %lu\n", sum );

   traceAddrs_.clear();
   numAlarms_ = numAddrs_ = 0 ;

   count = nextOut - returnVal ;
   return returnVal ;
}

