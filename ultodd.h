#ifndef __ULTODD_H__
#define __ULTODD_H__ "$Id: ultodd.h,v 1.1 2002-11-11 04:30:46 ericn Exp $"

/*
 * ultodd.h
 *
 * This header file declares the ultodd_t class, which 
 * takes and unsigned long IP address in network byte
 * order, and turns it into a string.
 *
 *
 * Change History : 
 *
 * $Log: ultodd.h,v $
 * Revision 1.1  2002-11-11 04:30:46  ericn
 * -moved from boundary1
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

class ultodd_t {
public:
   ultodd_t( unsigned long ip ); // in network order
   
   char const *getValue( void ) const { return dd_ ; }

private:
   char dd_[16];
};

#define IPSTRING( __add ) ultodd_t(__add).getValue()

#endif

