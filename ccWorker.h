#ifndef __CCWORKER_H__
#define __CCWORKER_H__ "$Id: ccWorker.h,v 1.2 2002-11-29 16:42:44 ericn Exp $"

/*
 * ccWorker.h
 *
 * This header file declares the curlCacheWorker_t
 * thread class, which is used to retrieve content
 * via curl for placement in the curl cache.
 *
 * Refer to curlCache.txt for a description of the
 * placement of this module and code in the overall
 * curl cache design.
 *
 * Change History : 
 *
 * $Log: ccWorker.h,v $
 * Revision 1.2  2002-11-29 16:42:44  ericn
 * -changed function typedefs
 *
 * Revision 1.1  2002/11/27 18:35:41  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include <curl/curl.h>
#include <string>
#include "mtQueue.h"

struct curlTransferRequest_t {
   void              *opaque_ ;     // app-specific data
   std::string        url_ ;        // url to request, should be absolute
   struct HttpPost   *postHead_ ;   // post with parameters or NULL. Deallocated by curl thread.
   bool volatile     *cancel_ ;     // used to tell curl thread to abort
};

typedef mtQueue_t<curlTransferRequest_t> curlQueue_t ;

curlQueue_t &getCurlRequestQueue( void );


//
// The callback routines are called during and at the end
// of a curl transfer.
//
// Every curl request should result in a callback to one of
// the following 'finalizer' routines:
//
//       onComplete
//       onCancel
//       onFailure
//
// The informational routines may or may not be called, depending
// on whether the information is known prior to completion:
//
//       onSize
//       onProgress
//


//
// Called when transfer is complete. The curl thread will 
// deallocate the associated memory when the call returns, so
// the callback should make a copy if necessary.
//
typedef void (*onCurlComplete_t)( curlTransferRequest_t &request,
                                  void const            *data,
                                  unsigned long          numRead );

//
// called if transfer fails
//
typedef void (*onCurlFailure_t)( curlTransferRequest_t &request,
                                 std::string const     &errorMsg );

//
// called if transfer cancelled
//
typedef void (*onCurlCancel_t)( curlTransferRequest_t &request );


//
// called when size is known (if known before completion)
//
// If size isn't known until completion, this routine may never
// be called.
//
typedef void (*onCurlSize_t)( curlTransferRequest_t &request,
                              unsigned long          size );
//
// called during transfer to indicate progress
//
typedef void (*onCurlProgress_t)( curlTransferRequest_t &request,
                                  unsigned long          totalReadSoFar );


void initializeCurlWorkers
   (  onCurlComplete_t     onComplete,
      onCurlFailure_t      onFailure,
      onCurlCancel_t       onCancel,
      onCurlSize_t         onSize,
      onCurlProgress_t     onProgress );

void shutdownCurlWorkers(void);

#endif
