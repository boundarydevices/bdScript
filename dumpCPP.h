#ifndef __DUMPCPP_H__
#define __DUMPCPP_H__ "$Id: dumpCPP.h,v 1.1 2004-04-07 12:45:21 ericn Exp $"

/*
 * dumpCPP.h
 *
 * This header file declares the dumpCPP_t class, 
 * which is used to dump a set of data as C++ source
 * (bytes). 
 *
 *
 * Change History : 
 *
 * $Log: dumpCPP.h,v $
 * Revision 1.1  2004-04-07 12:45:21  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


class dumpCPP_t {
public:
   dumpCPP_t( void const   *data,
                unsigned long size )
      : data_( data ),
        bytesLeft_( size ){}

   //
   // returns true and fills in line if something left
   // use getLine() and getLineLength() to get the data
   //
   bool nextLine( void );

   char const *getLine( void ) const { return lineBuf_ ; }

private:
   void const     *data_ ;
   unsigned long   bytesLeft_ ;
   char            lineBuf_[ 81 ];
};


#endif

