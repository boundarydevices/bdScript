#ifndef __CURLTHREAD_H__
#define __CURLTHREAD_H__ "$Id: curlThread.h,v 1.1 2002-10-31 02:13:08 ericn Exp $"

/*
 * curlThread.h
 *
 * This header file declares the interface to multi-threaded 
 * curl requests. The fundamentals of this are as follows:
 *
 *    Script creates a loadable object (image, sound, font...)
 *       with an un-typed object with at least a url property.
 *       
 *       var myImage = new image( { url:"images/myImage.jpg" } );
 *
 *    The left-hand side object (image()) constructor passes the 
 *    right and left-hand objects to queueCurlRequest. 
 *
 *    Before queueing the request, the constructor should set a 
 *    property to indicate whether the image has been loaded. 
 *
 *    By convention this property is a bool called 'isLoaded', 
 *    so that the Script can check completion by something like:
 *
 *       if( myImage.isLoaded )
 *          myImage.draw( 0, 0 );
 *
 *    Note for script'ers : Because the execution context is locked
 *    during main-line code, you can't simply wait for the object
 *    to be loaded by something like:
 *
 *       while( !myImage.isLoaded )       // BBBBBAAAAADDDDD!!!!
 *          ;                             // never gives handler a chance to complete
 *
 *    When one of the curl worker threads finishes a request, it
 *    locks the execution context and executes either the onComplete()
 *    or onError() routine to finish the object's construction. 
 *
 *    This code is typically used for conversion to a JavaScript 
 *    native object type (image, sound, font...) and for sanity
 *    checking (is image initialized with HTML code?).
 *
 *    By convention, each of these will also execute any handler 
 *    code specified by the right-hand object using the "onLoad" 
 *    and "onLoadError" methods. These script-level handlers should 
 *    be copied from the right-hand object into the left-hand object 
 *    before the request is queued.
 *
 *    In the case of load errors, the curl worker thread will set 
 *    the 'loadErrorMsg' property in the left hand object with 
 *    a description of the error prior to calling the onLoadError()
 *    method.
 *
 * ---------------------- Quick reference ------------------------
 *
 * Loadable object constructor/class should
 *
 *       define isLoaded property to false
 *       save a reference to the rhObj as property 'initializer' in lhObj.
 *          This prevents the garbage collector from 'collectin' the 
 *          rhObj and ensures that any non-curl parameters are available
 *          to the call-back function.
 *       copy onLoad and onLoadError script to lhObj from rhObj if present
 *       try to queue the request
 *       if unsuccessful, perform any needed cleanup, and 
 *          call the curlLoadErrorMsg() routine to set the loadErrorMsg
 *             property of lhObj (useful in onLoadError script)
 *          execute the onLoadError script if provided
 * 
 * queueCurlRequest() will :
 *       parse the rhObj, looking for the following properties:
 *
 *          url:        - if this is not found, queueCurlRequest will
 *                        return false
 *
 *          useCache:   - if present, used as flag for download
 *
 *          urlParams   - if present, used to build an HTTP get or
 *                        post request. This should be a nested untyped
 *                        object in which each property name is used
 *                        as a get or post parameter name, and the values
 *                        are stringified and url-escaped for transmission.
 *
 *                        Note that if a value contains a '@' as the first
 *                        character, the remainder of the value is treated
 *                        as a file name for an HTTP post upload
 *
 *
 * the curl queue worker threads will :
 *
 *    grab the next item off of the request queue and process the request
 *    when complete:
 *       check the completion status and call either 
 *       request::onComplete()
 *       request::onError()
 *
 * Change History : 
 *
 * $Log: curlThread.h,v $
 * Revision 1.1  2002-10-31 02:13:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <string>

#ifndef __CURLCACHE_H__
#include "curlCache.h"
#endif

#include "js/jsapi.h"

#ifndef __SEMAPHORE_H__
#include "semaphore.h"
#endif


struct jsCurlRequest_t {
   //
   // all of these fields must be filled in by the caller of queueCurlRequest
   //
   JSObject            *lhObj_ ;    // generally used for the destination object
   JSObject            *rhObj_ ;    // generally used to specify the request: 
                                    //    must be object with at least url property
                                    //    optional useCache property
                                    //    optionally urlParams[] array property
   JSContext           *cx_ ;       // context in which to run
   mutex_t             *mutex_ ;    // mutex to grant access to context

   //
   // one of these called when transfer terminates. 
   //
   void (*onComplete)( jsCurlRequest_t &, curlFile_t const &f );
   void (*onError)( jsCurlRequest_t &, curlFile_t const &f );
};

//
// In order to execute code specified by onLoad and onLoadError
// properties of rhObj in the context of the lhObj, derived classes
// can call these methods.
//
void jsCurlOnComplete( jsCurlRequest_t &, curlFile_t const &f );
void jsCurlOnError( jsCurlRequest_t &, curlFile_t const &f );

bool queueCurlRequest( jsCurlRequest_t const &request );

void startCurlThreads( void );
void stopCurlThreads( void );

#endif

