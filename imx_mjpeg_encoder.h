#ifndef __IMX_MJPEG_ENCODER_H__
#define __IMX_MJPEG_ENCODER_H__ "$Id$"

/*
 * imx_mjpeg_encoder.h
 *
 * This header file declares the mjpeg_encoder_t class for
 * use in encoding YUV data as MJPEG using the i.MX51's 
 * hardware encoder.
 *
 * The simplest use-case is:
 *
 *	initialize
 *	while (more frames) {
 *		get_bufs();
 *		fill in YUV data
 *		encode()
 *		process output data
 *	}
 * 
 * but this class also supports queueing and asynchronous completion 
 * of decoding. Using this interface, the simplest use case is 
 * something like this:
 *
 *	while (more frames) {
 *		if( get_bufs() ) {
 *		   fill in YUV data
 *		   start_encode()
 *		}
 *		if( encode_complete() ) {
 *		   process output data
 *		}
 *		... poll other devices
 *	}
 *
 * This queued interface allows tagging of each encode operation with
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

class mjpeg_encoder_t {
public:
	mjpeg_encoder_t(vpu_t &vpu, unsigned width, unsigned height, unsigned fourcc);
	
	bool initialized( void ) const { return initialized_ ; }

	inline unsigned fourcc(void) const { return fourcc_ ; }
	inline unsigned width(void) const { return w_ ; }
	inline unsigned height(void) const { return h_ ; }
	inline unsigned yuvSize(void) const { return imgSize_ ; }

	bool get_bufs( unsigned char *&y, unsigned char *&u, unsigned char *&v );

	// synchronous encode
	bool encode( void const *&outData, unsigned &outLength);

	// asynchronous encode
	bool start_encode( void *y, void *u, void *v, void *opaque );
	bool encode_complete(void const *&outData, unsigned &outLength, void *&opaque);


	inline unsigned numQueued(void) const { return numQueued_ ; }

	inline unsigned numStarted(void) const { return numStarted_ ; }
	inline unsigned numCompleted(void) const { return numCompleted_ ; }

	~mjpeg_encoder_t(void);
private:

	struct frame_buf {
		int addrY;
		int addrCb;
		int addrCr;
		int mvColBuf;
		vpu_mem_desc desc;
	};

	bool 	  	initialized_ ;
	unsigned	fourcc_ ;
	unsigned  	w_ ;
	unsigned  	h_ ;
	unsigned  	imgSize_ ;
        EncHandle 	handle_ ;
	PhysicalAddress phy_bsbuf_addr; /* Physical bitstream buffer */
	unsigned	virt_bsbuf_addr;		/* Virtual bitstream buffer */
	int 		picwidth;	/* Picture width */
	int 		picheight;	/* Picture height */
	int 		fbcount;	/* Total number of framebuffers allocated */
	int 		src_fbid;	/* Index of frame buffer that contains YUV image */
	FrameBuffer	*fb; /* frame buffer base given to encoder */
	struct frame_buf **pfbpool; /* allocated fb pointers are stored here */

	unsigned 	numQueued_ ;
	unsigned	nextAlloc_ ;
	unsigned	nextComplete_ ;
	void	       *app_context_[2];
	unsigned	numStarted_ ;
	unsigned 	numCompleted_ ;
};

#endif

