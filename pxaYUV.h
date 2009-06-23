#ifndef __PXAYUV_H__
#define __PXAYUV_H__ "$Id$"

/*
 * pxaYUV.h
 *
 * This header file declares the pxaYUV_t class, which 
 * will open and mmap() the PXA YUV frame buffer(s).
 *
 * Change History : 
 *
 * $Log$
 *
 *
 * Copyright Boundary Devices, Inc. 2009
 */


class pxaYUV_t {
public:
   pxaYUV_t( unsigned x, unsigned y, unsigned w, unsigned h );
   ~pxaYUV_t( void );
   
   inline bool isOpen(void) const {return 0 <= fd_y ;}

   unsigned getWidth(void) const { return width ; }
   unsigned getHeight(void) const { return height ; }

   void writeInterleaved( unsigned char const *data );
private:
   int      fd_y ;
   int      fd_u ;
   int      fd_v ;
   void     *map_y ;
   void     *map_u ;
   void     *map_v ;
   unsigned width ;
   unsigned height ;
   int      planar_y_bytes ;
   int      planar_uv_bytes ;
};

#endif

