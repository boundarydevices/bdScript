#ifndef __DDTOUL_H__
#define __DDTOUL_H__ "$Id: ddtoul.h,v 1.1 2002-11-11 04:30:45 ericn Exp $"

/*
 * ddtoul.h
 *
 * This header file declares the ddtoul_t class, which 
 * takes a dotted decimal IP address and turns it into
 * an unsigned long in either network or host byte order.
 *
 *
 * Change History : 
 *
 * $Log: ddtoul.h,v $
 * Revision 1.1  2002-11-11 04:30:45  ericn
 * -moved from boundary1
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


class ddtoul_t {
public:
   ddtoul_t( char const *dotted );

   bool worked( void ) const { return worked_ ; }
   unsigned long networkOrder( void ) const { return netOrder_ ; }
   unsigned long hostOrder( void ) const { return hostOrder_ ; }

private:
   unsigned long netOrder_ ;
   unsigned long hostOrder_ ;
   bool          worked_ ;
};

#endif

