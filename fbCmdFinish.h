#ifndef __FBCMDFINISH_H__
#define __FBCMDFINISH_H__ "$Id: fbCmdFinish.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * fbCmdFinish.h
 *
 * This header file declares the fbFinish_t command
 * list class, which represents the
 *
 *
 * Change History : 
 *
 * $Log: fbCmdFinish.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbCmdList.h"

class fbFinish_t : public fbCommand_t {
public:
   fbFinish_t();
   virtual ~fbFinish_t( void );

   virtual void const *data( void ) const ;

private:
};


#endif

