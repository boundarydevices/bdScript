/*
 * Module jsInput.cpp
 *
 * This module defines the initialization routine
 * for the input event Javascript bindings as declared
 * in jsInput.h
 *
 * Change History : 
 *
 * $Log: jsInput.cpp,v $
 * Revision 1.1  2007-07-07 00:25:52  ericn
 * -[bdScript] added mouse cursor and input handling
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include "jsInput.h"
#include "inputPoll.h"
#include "jsGlobals.h"
#include "js/jscntxt.h"

class jsInputPoll_t : public inputPoll_t {
public:
	jsInputPoll_t( char const *fileName );
	~jsInputPoll_t( void );

	virtual void onData( struct input_event const &event );
	jsval	handlerObj_ ;
	jsval	handlerCode_ ;
};

jsInputPoll_t::jsInputPoll_t( char const *fileName )
	: inputPoll_t( pollHandlers_, fileName )
	, handlerObj_( JSVAL_NULL )
	, handlerCode_( JSVAL_NULL )
{
	JS_AddRoot( execContext_, &handlerObj_ );
	JS_AddRoot( execContext_, &handlerCode_ );
}

jsInputPoll_t::~jsInputPoll_t( void )
{
	JS_RemoveRoot( execContext_, &handlerObj_ );
	JS_RemoveRoot( execContext_, &handlerCode_ );
}

void jsInputPoll_t::onData( struct input_event const &event )
{
	JSFunction *function = JS_ValueToFunction( execContext_, handlerCode_ );
	JSObject   *obj = JSVAL_TO_OBJECT(handlerObj_);
	if( function && obj ){
		jsval args[3] = {
                        INT_TO_JSVAL( event.type )
		,	INT_TO_JSVAL( event.code )
		,	INT_TO_JSVAL( event.value )
		};
		jsval rval ;
                JS_CallFunction(execContext_, obj, function, 3, args, &rval );
	}
}

static char const * const eventTypeNames[] = {
	"EV_SYN"
,	"EV_KEY"
,	"EV_REL"
,	"EV_ABS"
,	"EV_MSC"
,	"EV_SW"
};

static unsigned const numEventTypes = sizeof(eventTypeNames)/sizeof(eventTypeNames[0]);

JSClass jsInputClass_ = {
  "input",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSObject *inputProto = 0 ;

//
// constructor for the input object
//
static JSBool jsInput( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
	*rval = JSVAL_FALSE ;
	JSString *sDevice ;
	JSFunction *function ;
	if (( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) )
	    && 
	    (2 == argc )
	    &&
	    (0 != (sDevice=JSVAL_TO_STRING(argv[0])))
	    &&
	    (0 != (function=JS_ValueToFunction(cx,argv[1])))){
		char const *fileName = JS_GetStringBytes(sDevice);
		jsInputPoll_t *poller = new jsInputPoll_t(fileName);
		if( poller && poller->isOpen() ){
			JSObject *retObj = JS_NewObject( cx, &jsInputClass_, inputProto, obj );
			*rval = OBJECT_TO_JSVAL(retObj);
			JS_SetPrivate( cx, retObj, poller );
			poller->handlerObj_ = OBJECT_TO_JSVAL(obj);
			poller->handlerCode_ = OBJECT_TO_JSVAL(argv[1]);
		}
		else
			JS_ReportError(cx, "Error opening %s\n", fileName );

	}
	else
		JS_ReportError(cx, "Usage: new input( '/dev/input/event0' )\n" );

	return JS_TRUE;
}

bool initJSInput( JSContext *cx, JSObject *glob )
{
	bool worked = true ;
	for( unsigned i = 0 ; worked && ( i < numEventTypes ) ; i++ ){
	   worked = worked 
		    && 
		    JS_DefineProperty( cx, glob, eventTypeNames[i], 
					INT_TO_JSVAL( i ),
					0, 0, 
					JSPROP_ENUMERATE
                                       |JSPROP_PERMANENT
				       |JSPROP_READONLY );
	   if( worked ){
		   inputProto = JS_InitClass( cx, glob, NULL, &jsInputClass_,
						jsInput, 1,
						0, 0, 0, 0 );
		   worked = (0 != inputProto);
	   }
	}
	return worked ;
}

