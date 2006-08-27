#ifndef __MPEGSTREAM_H__
#define __MPEGSTREAM_H__ "$Id: mpegStream.h,v 1.4 2006-08-27 19:13:19 ericn Exp $"

/*
 * mpegStream.h
 *
 * This header file declares the mpegStreamFile_t class, which
 * reads and demuxes an mpeg stream a chunk at a time.
 *
 * Change History : 
 *
 * $Log: mpegStream.h,v $
 * Revision 1.4  2006-08-27 19:13:19  ericn
 * -add mpegFileStream_t class, deprecate mpegStream_t
 *
 * Revision 1.3  2006/08/24 23:59:11  ericn
 * -gutted in favor of mpegPS.h/.cpp
 *
 * Revision 1.2  2006/08/16 02:32:50  ericn
 * -add convenience function
 *
 * Revision 1.1  2005/04/24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */

#include <stdio.h>

#include "mpegPS.h"

//
// This class makes using the one above easier with data from 
// a file-system. It handles buffering to ensure that an entire
// frame is available at one time.
//
// returns false on end-of-file
//
class mpegStreamFile_t {
public:
   mpegStreamFile_t( FILE *fIn );
   ~mpegStreamFile_t( void );

   bool getFrame( unsigned char const       *&frameData,      // output: pointer to frame data
                  unsigned                   &frameLen,       // output: bytes in frame
                  long long                  &pts,            // output: when to play, ms relative to start
                  unsigned char              &streamId,       // output: which stream if video or audio, frame type if other
                  CodecType                  &codecType,      // output: VIDEO or AUDIO
                  CodecID                    &codecId );      // output: See list in mpegPS.h


   // for debug purposes. position in file 
   unsigned offsetInFile(void) const { return fileOffs_ ; }

private:
   FILE   *const fIn_ ;
   unsigned char inBuf_[8192];
   unsigned      offset_ ;
   unsigned      numLeft_ ;
   unsigned      fileOffs_ ;
};


#endif

