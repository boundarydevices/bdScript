#ifndef __IMX_MJPEG_DECODER_H__
#define __IMX_MJPEG_DECODER_H__ "$Id$"

/*
 * imx_mjpeg_decoder.h
 *
 * This header file declares the mjpeg_decoder_t class for
 * use in decoding MJPEG data to YUV using the i.MX51's hardware 
 * decoder (VPU).
 *
 * This class supports queueing and asynchronous completion of decoding. 
 * The simplest use case is something like this:
 *
 *	while (more frames) {
 *		if( fill_buff(mjpeg_data) ) {
 *		   start_decode()
 *		   advance input frame
 *		}
 *		if( decode_complete() ) {
 *		   process YUV output data
 *		}
 *		... poll other devices
 *	}
 *
 * This queued interface allows tagging of each decode operation with
 * an application-defined opaque parameter. It's envisioned that the
 * parameter will provide at least timing information about when the
 * frame of data was received (from a camera).
 * 
 * Copyright Boundary Devices, Inc. 2010
 */
extern "C" {
#include <vpu_lib.h>
#include <vpu_io.h>
};

#include "imx_vpu.h"

class mjpeg_decoder_t {
public:
	mjpeg_decoder_t(vpu_t &vpu);
	
	bool initialized( void ) const { return ERR < state_ ; }

	// these routines are only valid after a call to decode_complete() returns true
	inline unsigned width(void) const { return w_ ; }
	inline unsigned height(void) const { return h_ ; }
	inline unsigned yStride(void) const { return ystride_ ; }
	inline unsigned uvStride(void) const { return uvstride_ ; }
	inline unsigned yuvSize(void) const { return imgSize_ ; }
	inline unsigned ySize(void) const { return ystride_*h_ ; }
	inline unsigned uvSize(void) const { return uvstride_*h_; }

	//
	// asynchronous decode
	//
	bool fill_buf( void const *data, unsigned length, void *opaque );
	bool decode_complete(void const *&y, void const *&u, void const *&v, void *&opaque );

	inline unsigned numQueued(void) const { return numQueued_ ; }

	inline unsigned numStarted(void) const { return numStarted_ ; }
	inline unsigned numCompleted(void) const { return numCompleted_ ; }

	~mjpeg_decoder_t(void);
private:
	enum state {
		ERR,
		INIT,
		DECODING
	};
	struct frame_buf {
		int addrY;
		int addrCb;
		int addrCr;
		int mvColBuf;
		vpu_mem_desc desc;
	};

	unsigned  	w_ ;
	unsigned  	h_ ;
	unsigned  	ystride_ ;
	unsigned  	uvstride_ ;
	unsigned  	imgSize_ ;
        DecHandle 	handle_ ;
	PhysicalAddress phy_bsbuf_addr;
	PhysicalAddress phy_ps_buf;
	PhysicalAddress phy_slice_buf;
	int 		phy_slicebuf_size;
	unsigned long	virt_bsbuf_addr;
	int 		fbcount;
	FrameBuffer	*fb;
	struct frame_buf *pfbpool;
	vpu_mem_desc 	*mvcol_memdesc;

	unsigned 	numQueued_ ;
	unsigned	nextAlloc_ ;
	unsigned	nextComplete_ ;
	void	       *app_context_[2];
	unsigned	numStarted_ ;
	unsigned 	numCompleted_ ;
	enum state	state_ ;
};

#endif

