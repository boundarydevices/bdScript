#include "jsSHA1.h"
/*
 * Module jsSHA1.cpp
 *
 * This module defines sha1sum function that can be used to calculate
 * sha1sum for a given string.
 *
 *
 * Chnage History :
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "jsSHA1.h"
#include <string>
#include <stdio.h>
#include <openssl/sha.h>

/*
 * takes a string as an argument and returns the sha1sum
 * of the string.
 */
static JSBool
jsSHA1Sum( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   *rval = JSVAL_VOID;
   if((1 == argc) && JSVAL_IS_STRING(argv[0]))
   {
      JSString *str = JSVAL_TO_STRING(argv[0]);
      SHA_CTX ctx;
      unsigned char md[SHA_DIGEST_LENGTH];
      char hashStr[SHA_DIGEST_LENGTH*2+1] = "";
      int idx = 0;
      SHA1_Init(&ctx);
      SHA1_Update(&ctx, JS_GetStringBytes(str), JS_GetStringLength(str));
      SHA1_Final(md, &ctx);
      for(idx = 0; idx < SHA_DIGEST_LENGTH; idx++) {
         sprintf((hashStr + (idx * 2)), "%02X", md[idx]);
      }
      *rval = STRING_TO_JSVAL(JS_NewStringCopyN(cx, hashStr, strlen(hashStr)));
   }
   else
      JS_ReportError( cx, "usage: sha1sum(string)" );

   return JS_TRUE;
}

static JSFunctionSpec sha1functions[] = {
   {"sha1sum",      jsSHA1Sum,         1},
   {0}
};

bool initJSSHA1(JSContext *cx, JSObject *glob)
{
   return JS_DefineFunctions( cx, glob, sha1functions);
}
