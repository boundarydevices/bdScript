#ifndef __CURLGET_H__
#define __CURLGET_H__ "$Id: curlGet.h,v 1.1 2002-10-06 16:52:32 ericn Exp $"

/*
 * curlGet.h
 *
 * This header file declares the curlGet() routine, which
 * simply retrieves a file and saves it into the specified
 * output file.
 *
 *
 * Change History : 
 *
 * $Log: curlGet.h,v $
 * Revision 1.1  2002-10-06 16:52:32  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

bool curlGet( char const *url,
              char const *saveAs = 0 ); // 0 means current working directory, same file name

#endif

