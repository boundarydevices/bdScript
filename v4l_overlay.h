#ifndef __V4L_OVERLAY_H__
#define __V4L_OVERLAY_H__ "$Id$"

/*
 * v4l_overlay.h
 *
 * This header file declares the v4l_overlay_t class, which is
 * used to display a YUV overlay on an i.MX51 processor. 
 *
 * When constructed with a specified input widtn and height
 * and output rectangle, this class will attempt to open the 
 * YUV device and configure it as specified.
 *
 * Usage generally involves checking for success (isOpen()),
 * then repeatedly calling getBuf() and putBuf().
 *
 * Change History : 
 *
 * $Log$
 *
 *
 * Copyright Boundary Devices, Inc. 2010
 */

class v4l_overlay_t {
public:
    v4l_overlay_t( unsigned inw, unsigned inh,
                   unsigned outx, unsigned outy,
                   unsigned outw, unsigned outh,
                   unsigned transparency,       // 0 == opaque, 255 == fully transparent
                   unsigned fourcc );           // V4L_PIX_blah
    ~v4l_overlay_t( void );
    
    bool isOpen( void ) const { return 0 <= fd_ ; }
    int getFd( void ) const { return fd_ ; }

    unsigned imgSize(void)const { return ySize()+2*uvSize(); }

    unsigned ySize(void) const { return ysize_ ; }
    unsigned yStride(void) const { return ystride_ ; }

    unsigned uvSize(void) const { return uvsize_ ; }
    unsigned uvStride(void) const { return uvstride_ ; }

    bool getBuf( unsigned       &idx, 
                 unsigned char *&y, 
                 unsigned char *&u, 
                 unsigned char *&v );

    void putBuf( unsigned idx );

    unsigned lastIndex(void) const { return lastIndex_ ; }
    unsigned numQueued(void) const { return numQueued_ ; }

private:
    enum {
        NUM_BUFFERS = 4
    };

    void close(void);
    void pollBufs(void);

    v4l_overlay_t( v4l_overlay_t const & ); // no copies
    int         fd_ ;
    unsigned    ysize_ ;
    unsigned    uvsize_ ;
    unsigned    ystride_ ;
    unsigned    uvstride_ ;
    unsigned    numQueued_ ;
    void const *buffers_[NUM_BUFFERS];
    bool        streaming_ ;
    bool        buffer_avail[NUM_BUFFERS];
    unsigned    next_buf_ ;
    unsigned    lastIndex_ ;
};

#endif

