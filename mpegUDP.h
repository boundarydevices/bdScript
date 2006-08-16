#ifndef __MPEGUDP_H__
#define __MPEGUDP_H__ "$Id: mpegUDP.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * mpegUDP.h
 *
 * Declares the data structure and constants for sending MPEG
 * data over UDP for streaming.
 *
 */

enum mpegUDP_packet_e {
   MPEGUDP_HEADER = 0
,  MPEGUDP_DATA   = 1
};


enum mpegFrameType_e {
   MPEGUDP_VIDEO = 1
,  MPEGUDP_AUDIO = 2
};



#define MPEGUDP_MAXDATA    4096

struct mpegFrame_t {
   mpegFrameType_e type_ ;
   unsigned short  frameLen_ ;
   unsigned short  streamId_ ;
   long long       dts_ ;
   long long       pts_ ;
   unsigned char   data_[MPEGUDP_MAXDATA];
} __attribute__ ((aligned (2)));


#define MPEGUDP_MAXFILENAME 256

struct mpegHeader_t {
   unsigned short nameLength_ ;
   char           name_[MPEGUDP_MAXFILENAME];
} __attribute__ ((aligned (2)));



struct mpegUDP_t {
   mpegUDP_packet_e  type_ ;
   unsigned          identifier_ ;     // identifies file
   unsigned          sequence_ ;       // packet number within the file

   union {
      struct mpegHeader_t header_ ;
      struct mpegFrame_t  frame_ ;
   } d ;
} __attribute__ ((aligned (2)));
 

#endif
