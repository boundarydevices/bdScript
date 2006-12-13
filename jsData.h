#ifndef __JSDATA_H__
#define __JSDATA_H__ "$Id: jsData.h,v 1.4 2006-12-13 21:27:09 ericn Exp $"

/*
 * jsData.h
 *
 * This header file declares a set of routines to
 * get and process Javascript data in isolation. 
 *
 * Typical usage is to instantiate a jsData_t object,
 * throw a file at it, then evaluate expressions to 
 * retrieve data from the runtime.
 *
 * This
 *
 *
 * Change History : 
 *
 * $Log: jsData.h,v $
 * Revision 1.4  2006-12-13 21:27:09  ericn
 * -added operator jsObject *
 *
 * Revision 1.2  2006/11/05 18:19:18  ericn
 * -allow nested jsData_t's
 *
 * Revision 1.1  2006/10/16 22:45:42  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "js/jsstddef.h"
#include "js/jsapi.h"

class jsData_t {
public:
   jsData_t( char const *scriptlet,
             unsigned    length,
             char const *fileName = 0 );
   jsData_t( JSRuntime  *rt,
             JSContext  *cx,
             JSObject   *rootObj,
             char const *fileName = 0 );
   ~jsData_t( void );

   inline bool initialized( void ) const { return 0 != obj_ ; }
   
   bool evaluate( char const *scriptlet, 
                  unsigned    length, 
                  jsval      &rval,
                  JSObject   *obj = 0 ) const ;  // zero means global scope

   inline JSContext *cx( void ) const { return cx_ ; }

   inline char const *errorMsg() const { return errorMsg_ ; }

   void setErrorMsg( char const *msg, char const *fileName, unsigned line );

   bool getInt( char const *name, int &value, JSObject *obj=0 ) const ;
   bool getDouble( char const *name, double &value, JSObject *obj=0 ) const ;
   bool getBool( char const *name, bool &value, JSObject *obj=0 ) const ;
   bool getString( char const *name, char *value, unsigned maxLen, unsigned &length, JSObject *obj=0 ) const ;

   operator JSRuntime *(void) const { return rt_ ; }
   operator JSContext *(void) const { return cx_ ; }

private:
   char const * const fileName_ ;
   bool         const ownIt_ ;
   JSRuntime         *rt_ ;
   JSContext         *cx_ ; 
   JSObject          *obj_ ;
   char               errorMsg_[256];
};

#endif

