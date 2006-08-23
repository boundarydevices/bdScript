#ifndef __TRACE_H__
#define __TRACE_H__ "$Id: trace.h,v 1.2 2006-08-23 15:47:19 ericn Exp $"

/*
 * trace.h
 *
 * This header file declares a couple of routines
 * for tracing the execution of a program. 
 *
 * startTrace() - begins capture of traces
 * endTrace() - ends capture of traces, returns a 
 *              sorted buffer of PC addresses
 *
 * Change History : 
 *
 * $Log: trace.h,v $
 * Revision 1.2  2006-08-23 15:47:19  ericn
 * -use more specific name for callback
 *
 * Revision 1.1  2006/08/21 12:37:15  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


typedef void (*traceCallback_t)( void * );

void startTrace( traceCallback_t cb, void *cbParam );

struct traceEntry_t {
   unsigned long pc_ ;
   unsigned long count_ ;

   bool operator<(traceEntry_t const &rhs) const ;
};

//
// returns the number of trace entries and the count
// delete [] when done
//
traceEntry_t *endTrace( unsigned &count );

//
// prints out a set of traces
//
void dumpTraces( traceEntry_t const *traces,
                 unsigned            count );

#endif

