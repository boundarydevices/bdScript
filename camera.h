#ifndef __CAMERA_H__
#define __CAMERA_H__ "$Id$"

/*
 * camera.h
 *
 * This header file declares the camera_t class, which is
 * a thin wrapper around an mmap'd V4L2 device that returns
 * frames to the caller for processing.
 * 
 * Note that this class sets the camera file descriptor to
 * non-blocking. Use poll() or select() to wait for a frame.
 *
 * Change History : 
 *
 * $Log$
 *
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include <linux/videodev2.h>
#include <sys/poll.h>

class camera_t {
public:
	camera_t( char const *devName,
		  unsigned    width,
		  unsigned    height,
		  unsigned    fps,
		  unsigned    pixelformat );
	~camera_t(void);

	bool isOpen(void) const { return 0 <= fd_ ; }
	int getFd(void) const { return fd_ ; }

	unsigned getWidth(void) const { return w_ ; }
	unsigned getHeight(void) const { return h_ ; }
	unsigned stride(void) const { return fmt_.fmt.pix.bytesperline ; }
	unsigned imgSize(void) const { return fmt_.fmt.pix.sizeimage ; }

	// capture interface
	bool startCapture(void);

	// pull frames with this method
	bool grabFrame(void const *&data,int &index);

	// return them with this method
	void returnFrame(void const *data, int index);

	bool stopCapture(void);
	
	unsigned numDropped(void) const { return frame_drops_ ; }
private:
	struct buffer {
		void *		  start;
		size_t		  length;
	};

	int			fd_ ;
	unsigned const      	w_ ;
	unsigned const      	h_ ;
	struct pollfd		pfd_ ;
	struct v4l2_format	fmt_ ;
	struct buffer		*buffers_ ;
	unsigned		n_buffers_ ;
	unsigned		frame_drops_ ;
};

#endif

