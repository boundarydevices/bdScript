#ifndef __OPENFDS_H__
#define __OPENFDS_H__ "$Id: openFds.h,v 1.1 2003-08-24 15:46:43 ericn Exp $"

/*
 * openFds.h
 *
 * This header file declares the openFds_t class, 
 * which is used to retrieve the set of open file 
 * handles for the calling process.
 *
 *
 * Change History : 
 *
 * $Log: openFds.h,v $
 * Revision 1.1  2003-08-24 15:46:43  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

class openFds_t {
public:
   openFds_t( void );
   ~openFds_t( void );

   unsigned count( void ) const { return count_ ; }
   int operator[]( unsigned idx ) const { return fds_[idx]; }

private:
   unsigned count_ ;
   int     *fds_ ;
};

#endif

