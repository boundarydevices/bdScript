#ifndef __STRINGLIST_H__
#define __STRINGLIST_H__ "$Id: stringList.h,v 1.1 2002-12-15 20:02:37 ericn Exp $"

/*
 * stringList.h
 *
 * This header file declares the stringList_t
 * typedef.
 *
 *
 * Change History : 
 *
 * $Log: stringList.h,v $
 * Revision 1.1  2002-12-15 20:02:37  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <string>
#include <list>

typedef std::list<std::string> stringList_t ;

#endif

