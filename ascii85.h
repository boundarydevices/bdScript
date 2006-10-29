#ifndef __ASCII85_H__
#define __ASCII85_H__ "$Id: ascii85.h,v 1.1 2006-10-29 21:59:10 ericn Exp $"

/*
 * ascii85.h
 *
 * This header file declares the ascii85_t class,
 * which is a filter used to produce printable ASCII 
 * from binary data. 
 *
 * This class doesn't perform any output, but hands each
 * character of output data to the application-supplied
 * handler.
 * 
 *
 * Change History : 
 *
 * $Log: ascii85.h,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


//
// App-supplied output routine. Return true to keep
// going, false to stop.
//
typedef bool (*ascii85_output_t)( char  outchar,
                                  void *opaque );

class ascii85_t {
public:
   ascii85_t( ascii85_output_t outHandler,
              void            *outParam );

   bool convert( char const *inData,
                 unsigned    inLen );
   bool flush( void );

private:
   ascii85_output_t handler_ ;
   void            *hParam_ ;
   char             leftover_[4];
   unsigned         numLeft_ ;
};


#endif

