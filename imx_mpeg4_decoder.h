#ifndef __IMX_MPEG4_DECODER_H__
#define __IMX_MPEG4_DECODER_H__ "$Id$"

/*
 * imx_mpeg4_decoder.h
 *
 * This header file declares the mpeg4_decoder_t class for use 
 * in decoding mpeg4 data to YUV using the i.MX51's hardware 
 * decoder (VPU).
 *
 * This class supports queueing and asynchronous completion of decoding. 
 * The simplest use case is something like this:
 *
 *	while (more frames) {
 *		if( space_avai(how_much) ){
 *			if( fill_buff(mpeg4_data) ) {
 *		   		advance input pointer
 *			}
 *		}
 *		if( decode_complete() ) {
 *		   process YUV output data
 *			... time may elapse here
 *		   releaseBuffer
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

class mpeg4_decoder_t {
public:
	enum {
		PICTYPE_IFRAME
	,	PICTYPE_PFRAME
	,	PICTYPE_BIFRAME
	,	PICTYPE_BFRAME
	,	PICTYPE_SKIPPED
	};
	mpeg4_decoder_t(vpu_t &vpu);
	
	bool initialized( void ) const { return ERR < state_ ; }

	// these routines are only valid after a call to decode_complete() returns true
	inline unsigned width(void) const { return w_ ; }
	inline unsigned height(void) const { return h_ ; }
	inline unsigned yStride(void) const { return ystride_ ; }
	inline unsigned uvStride(void) const { return uvstride_ ; }
	inline unsigned yuvSize(void) const { return imgSize_ ; }
	inline unsigned ySize(void) const { return ystride_*h_ ; }
	inline unsigned uvSize(void) const { return uvstride_*h_/2; }

	//
	// asynchronous decode
	//
	bool space_avail( unsigned &length );
	bool fill_buf( void const *data, unsigned length, void *opaque );
	bool decode_complete(void const *&y, void const *&u, void const *&v, void *&opaque, unsigned &displayIdx, int &picType, bool &errors );
	void releaseBuffer(unsigned displayIdx);

	~mpeg4_decoder_t(void);
private:
	void startDecode(void);
	enum state {
		ERR,
		INIT,		// open, no display info
		DECODING	// have display info
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
	bool		startedDecode_ ;
	unsigned	app_fbs ;
	unsigned	decoder_fbs ;
	enum state	state_ ;
};

#endif

