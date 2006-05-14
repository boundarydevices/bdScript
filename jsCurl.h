#ifndef __JSCURL_H__
#define __JSCURL_H__ "$Id: jsCurl.h,v 1.11 2006-05-14 14:33:08 ericn Exp $"

/*
 * jsCurl.h
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
 *       if successful
 *          set the isLoaded flag to true 
 *          request::onComplete()
 *       else
 *          request::onError()
 *
 *                                                                                     static              static
 *    Object            constructor             methods           properties           methods           properties
 *    curlFile          curlFile( url )                           isOpen
 *                                                                size
 *                                                                data
 *                                                                url
 *                                                                httpCode
 *                                                                fileTime
 *                                                                mimeType 
 *
 * and each returns an object (array) with the following
 * members:
 *    worked   - boolean : meaning is obvious
 *    status   - HTTP status
 *    mimeType - mime type of retrieved file
 *    content  - String with the content of the URL
 *
 * Change History : 
 *
 * $Log: jsCurl.h,v $
 * Revision 1.11  2006-05-14 14:33:08  ericn
 * -use constant for initializer
 *
 * Revision 1.10  2003/11/28 14:09:54  ericn
 * -lock lhObj_ via jsval
 *
 * Revision 1.9  2002/12/03 13:36:13  ericn
 * -collapsed code and objects for curl transfers
 *
 * Revision 1.8  2002/12/02 15:09:23  ericn
 * -added bookkeeping for callbacks
 *
 * Revision 1.7  2002/11/30 16:29:14  ericn
 * -fixed locking and allocation of requests
 *
 * Revision 1.6  2002/11/30 05:27:32  ericn
 * -moved async into request structure
 *
 * Revision 1.5  2002/11/30 00:32:05  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.4  2002/11/03 17:55:51  ericn
 * -modified to support synchronous gets and posts
 *
 * Revision 1.3  2002/10/13 14:36:11  ericn
 * -removed curlPost(), fleshed out variable handling
 *
 * Revision 1.2  2002/10/13 13:50:55  ericn
 * -merged curlGet() and curlPost() with curlFile object
 *
 * Revision 1.1  2002/09/29 17:36:23  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"
#include <string>

struct jsCurlRequest_t {
   typedef void (*onComplete_t)( jsCurlRequest_t &,
                                 void const     *data,
                                 unsigned long   numRead );
   enum status_e {
      inTransit_,
      completed_,
      failed_,
      cancelled_
   };

   //
   // all of these fields must be filled in by the caller of queueCurlRequest
   //
   JSObject            *lhObj_ ;    // generally used for the destination object
   jsval                lhVal_ ;
   JSContext           *cx_ ;       // context in which to run handlers (generally lhObj context)
   bool                 async_ ;
   bool                 isComplete_ ;
   status_e             status_ ;
   
   //
   // only valid if status_ == completed_ 
   //
   void const          *data_ ;
   unsigned long        length_ ;
   unsigned long        handle_ ;

   //
   // only valid if status_ == failed_
   //
   std::string          errorMsg_ ;

   unsigned long        expectedSize_ ;
   unsigned long        bytesSoFar_ ;

   pthread_t            callingThread_ ;

   //
   // This is called when(if) transfer terminates to process data
   //
   onComplete_t onComplete_ ;
   
   ~jsCurlRequest_t( void );

private:
   jsCurlRequest_t( JSObject    *lhObj,         // input : newly constructed object
                    JSContext   *cx,            // input : JS execution context
                    bool         async,         // input : asynchronous call?  
                    onComplete_t onComplete );  // input : post-processor for completion

   //
   // This routine saves initializer values into lhObj->initializer and queues
   // the file for transfer (possibly immediate return).
   //
   // When (if) transfer completes, the onComplete call is made to perform type-specific
   // parsing of the data. 
   //
   friend bool queueCurlRequest( JSObject    *lhObj,        // input : newly constructed object
                                 jsval        initializer,  // input : right-hand size of constructor or function
                                 JSContext   *cx,           // input : JS execution context
                                 onComplete_t onComplete = 0 ); // input : post-processor for completion
};

bool initJSCurl( JSContext *cx, JSObject *glob );

extern char const * const initializerProp_ ;

#endif

