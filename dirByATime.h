#ifndef __DIRBYATIME_H__
#define __DIRBYATIME_H__ "$Id: dirByATime.h,v 1.1 2002-09-28 16:50:46 ericn Exp $"

/*
 * dirByATime.h
 *
 * This header file declares the dirByATime_t class,
 * which is used to read a directory and sort the file
 * entries by access time (stat.st_atime).
 *
 * Note that this class does not include entries for 
 * subdirectories or device files (only reg'lar files)
 *
 * Change History : 
 *
 * $Log: dirByATime.h,v $
 * Revision 1.1  2002-09-28 16:50:46  ericn
 * Initial revision
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */
#include <dirent.h>
#include <sys/stat.h>

class dirByATime_t {
public:
   dirByATime_t( char const *dirName );
   ~dirByATime_t( void );

   struct details_t {
      struct stat   stat_ ;
      struct dirent dirent_ ;
   };

   unsigned long totalSize( void ) const { return totalSize_ ; }
   unsigned numEntries( void ) const { return numEntries_ ; } 
   struct stat const   &getStat( unsigned idx ) const { return entries_[idx].stat_ ; }
   struct dirent const &getDirent( unsigned idx ) const { return entries_[idx].dirent_ ; }
   details_t const     &getEntry( unsigned idx ) const { return entries_[idx]; }

private:
   char            dir_[256];
   unsigned        dirLen_ ;
   unsigned        numEntries_ ;
   unsigned long   totalSize_ ;
   details_t      *entries_ ;
};


#endif

