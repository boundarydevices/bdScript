#ifndef __AVIHEADER_H__
#define __AVIHEADER_H__ "$Id: aviHeader.h,v 1.1 2007-08-14 12:58:33 ericn Exp $"

/*
 * aviHeader.h
 *
 * This header file declares the readAviHeader() routine,
 * which will attempt to parse the AVI Main header of a
 * file.
 *
 *    http://msdn2.microsoft.com/en-us/library/aa451195.aspx
 *
 * Change History : 
 *
 * $Log: aviHeader.h,v $
 * Revision 1.1  2007-08-14 12:58:33  ericn
 * -import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

extern bool readAviHeader( char const    *fileName,
                           unsigned long &numFrames,
                           unsigned long &numStreams,
                           unsigned long &width,
                           unsigned long &height );

#endif

