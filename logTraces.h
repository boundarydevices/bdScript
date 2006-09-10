#ifndef __LOGTRACES_H__
#define __LOGTRACES_H__ "$Id: logTraces.h,v 1.1 2006-09-10 01:18:13 ericn Exp $"

/*
 * logTraces.h
 *
 * This header file declares the interface routines
 * for a subsystem to trace execution of routines and
 * code blocks. 
 * 
 * Tracing consists of two primary things:
 *
 *    trace output: a TCP socket is created to
 *       accept connection requests from a single
 *       client. When connected, the output class will
 *       send a description of all trace sources to the
 *       client application (normally telnet or the like).
 *
 *       The logTraces_t singleton is used to set up this
 *       machinery. Note that the 'enable' flag can be used
 *       to turn off logging entirely.
 *
 *    application loggers: Any module for which tracing
 *       is desired uses the 'traceSource_t' and 'trace_t'
 *       classes to identify the activity and state of
 *       each trace event. The traceSource_t class defines
 *       the timeline for output, and the trace_t
 *
 *       traceSource_t objects should always be static
 *       in scope. 
 *
 *       trace_t objects should always be dynamic. They
 *       will increment the value of the traceSource_t
 *       in their constructor and decrement the value
 *       in their destructor.
 *
 * In order to remove the overhead involved in tracing, the trace_t 
 * class is normally referred to through the TRACE_T macro.
 *
 * When tracing events that do not have their duration on a stack 
 * frame, the traceSource_t::increment() and decrement() routines
 * can be used directly. Use the TRACEINCR() and TRACEDECR() macros
 * to allow a single-setting control over whether these log entries
 * are generated.
 *
 * Change History : 
 *
 * $Log: logTraces.h,v $
 * Revision 1.1  2006-09-10 01:18:13  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class logTraces_t {
public:
   static logTraces_t &get( void );
   static logTraces_t &get( bool enable );      // call once in main() to enable

   // returns source id
   unsigned newSource( char const *sourceName );

   void     log( unsigned source, unsigned value );

   inline bool enabled( void ) const { return 0 <= listenFd_ ; }
   inline bool connected( void ) const { return 0 <= connectFd_ ; }

   void connect();
   void disconnect();

private:
   logTraces_t( void );
   ~logTraces_t( void );
   void enable( void );

   int   memFd_ ;
   void *mem_ ;
   void *oscr_ ;
   int   listenFd_ ;
   int   connectFd_ ;
};


class traceSource_t {
public:
   traceSource_t( char const *name );
   ~traceSource_t( void );

   void increment();
   void decrement();
private:
   logTraces_t &logger_ ;
   unsigned     sourceId_ ;
   unsigned     value_ ;
};

class trace_t {
public:
   trace_t( traceSource_t &source )
      : src_( source ){ src_.increment(); }
   ~trace_t( void ){ src_.decrement(); }
private:
   traceSource_t &src_ ;
};

#ifdef LOGTRACES
   #define TRACE_T( __src, __name )  trace_t __name( __src );
   #define TRACEINCR( __src ) __src.increment()
   #define TRACEDECR( __src ) __src.decrement()
#else
   #define TRACE_T( __src, __name ) /**/
   #define TRACEINCR( __src ) /**/
   #define TRACEDECR( __src ) /**/
#endif

#endif

