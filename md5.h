#ifndef __MD5_H__
#define __MD5_H__ "$Id: md5.h,v 1.2 2004-09-27 04:35:25 ericn Exp $"

/*
 * md5.h
 *
 * This header file declares the getMD5() routine,
 * which will produce a 16-byte hash of the input
 * buffer.
 *
 *
 * Change History : 
 *
 * $Log: md5.h,v $
 * Revision 1.2  2004-09-27 04:35:25  ericn
 * -allow md5 of file and cramfs
 *
 * Revision 1.1  2003/09/06 19:51:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

struct md5_t {
   unsigned char md5bytes_[16];
};

void getMD5( void const   *data,
             unsigned long bytes,
             md5_t        &result );

//
// returns true if the file is opened
// and read successfully
//
bool getMD5( char const *fileName,
             md5_t      &result );

//
// returns true if the file is opened, looks like a 
// cramfs filesystem, and is read successfully
//
bool md5Cramfs( char const *fileName,
                md5_t      &result );


#endif

