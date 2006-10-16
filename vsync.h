#ifndef __VSYNC_H__
#define __VSYNC_H__ "$Id: vsync.h,v 1.1 2006-10-16 22:45:52 ericn Exp $"

/*
 * vsync.h
 *
 * This header file declares the vsync_t class, which is
 * used to share a single file-handle across multiple 
 * clients, generally for use in timing output to an LCD
 * panel.
 *
 * This module is generally used in conjunction with the
 * multiSignal.h/.cpp module to provide common signal
 * handling between animations, video playback, etc.
 *
 *
 * Change History : 
 *
 * $Log: vsync.h,v $
 * Revision 1.1  2006-10-16 22:45:52  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class vsync_t{ 
public:
   static vsync_t &get( void );

   inline int getFd( void ) const { return fd_ ; }
   inline bool isOpen( void ) const { return 0 <= fd_ ; }

   inline int getSignal( void ) const { return signo_ ; }

   void installHandler( void );

private:
   vsync_t( void ); // only called by ::get()
   ~vsync_t( void );

   int const fd_ ;
   int const signo_ ;
   bool      installed_ ;
};

#endif

