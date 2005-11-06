/*
 * Module sniffWLAN.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: sniffWLAN.cpp,v $
 * Revision 1.4  2005-11-06 00:49:52  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.3  2003/09/07 22:51:17  ericn
 * -modified to allow empty SSID
 *
 * Revision 1.2  2003/08/13 00:49:04  ericn
 * -fixed cancellation process
 *
 * Revision 1.1  2003/08/12 01:20:38  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "sniffWLAN.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if_arp.h>
#include <asm/types.h>
#include <time.h>
#include "tickMs.h"
#include <poll.h>

/*
 * Packet Types
 */
#define MGT_PROBE      		0x0040  	/* Client ProbeRequest    */
#define MGT_PROBE_RESP		0x0050 		/* AP ProbeReponse        */
#define MGT_ASSOC_REQ 		0x0000 		/* Client Assocrequest    */
#define MGT_BEACON  		0x0080 		/* Management - Beacon frame  */
#define DATA_PURE			0x0008		/* Data Packet */

#define FLAG_TO_DS				0x01
#define FLAG_FROM_DS			0x02
#define FLAG_MORE_FRAGMENTS		0x04
#define FLAG_RETRY				0x08
#define FLAG_POWER_MGT			0x10
#define FLAG_MORE_DATA			0x20
#define FLAG_WEP				0x40
#define FLAG_ORDER				0x80

#define FLAG_WEP_REQUIRED		0x10


#define COOK_FLAGS(x)          (((x) & 0xFF00) >> 8)
#define DATA_ADDR_T4         ((FLAG_TO_DS|FLAG_FROM_DS) << 8)
#define COOK_ADDR_SELECTOR(x) ((x) & 0x300)

#define DATA_SHORT_HDR_LEN     24
#define DATA_LONG_HDR_LEN      30
#define MGT_FRAME_HDR_LEN      24	/* Length of Managment frame-headers */

#define IP_PROTO_UDP		17		/* user datagram protocol */
#define IP_PROTO_TCP		6		/* tcp */
#define IP_PROTO_ICMP		1		/* icmp */

#define IP_OFFSET_PROTO	9

#define IS_TO_DS(x)            ((x) & FLAG_TO_DS)
#define IS_FROM_DS(x)          ((x) & FLAG_FROM_DS)
#define IS_WEP(x)              ((x) & FLAG_WEP)
#define IS_WEP_REQUIRED(x)     ((x) & FLAG_WEP_REQUIRED)

#define AP_NONE 	0
#define AP_ONE		1
#define AP_MORE     2
#define AP_PROBE	3

static const uint8_t DHCPD_SIGNATURE[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x43, 0x00, 0x44};

#define  DHCPD_OFFSET 24


/* ************************************************************************* */
/*        Logical field codes (IEEE 802.11 encoding of tags)                 */
/* ************************************************************************* */
#define TAG_SSID           0x00
#define TAG_SUPP_RATES     0x01
#define TAG_FH_PARAMETER   0x02
#define TAG_DS_PARAMETER   0x03
#define TAG_CF_PARAMETER   0x04
#define TAG_TIM            0x05
#define TAG_IBSS_PARAMETER 0x06
#define TAG_CHALLENGE_TEXT 0x10


#define DEVNAME_LEN 16

/*
 * The basic information types in the sniffed packed from prism
 */
typedef struct {
  __u32 did __attribute__ ((packed));
  __u16 status __attribute__ ((packed));
  __u16 len __attribute__ ((packed));
  __u32 data __attribute__ ((packed));
} p80211item_t;

/*
 * Just before the payload, the prismcard
 * puts in a lot of other info
 * like signal quality an stuff
 */
typedef struct {
  __u32 msgcode __attribute__ ((packed));
  __u32 msglen __attribute__ ((packed));
  __u8 devname[DEVNAME_LEN] __attribute__ ((packed));
  p80211item_t hosttime __attribute__ ((packed));
  p80211item_t mactime __attribute__ ((packed));
  p80211item_t channel __attribute__ ((packed));
  p80211item_t rssi __attribute__ ((packed));
  p80211item_t sq __attribute__ ((packed));
  p80211item_t signal __attribute__ ((packed));
  p80211item_t noise __attribute__ ((packed));
  p80211item_t rate __attribute__ ((packed));
  p80211item_t istx __attribute__ ((packed));
  p80211item_t frmlen __attribute__ ((packed));
} AdmInfo_t;

/*
 * Overlay to directly index varius types of data
 * in Mgmt Frames
 */
typedef struct {
	unsigned short frametype;
	unsigned short duration;
	unsigned char DestAddr[6];
	unsigned char SrcAddr[6];
	unsigned char BssId[6];
	unsigned short FragSeq;
    unsigned short TimeStamp[4];
	unsigned short BeaconInterval;
	unsigned short Capabilities;
}FixedMgmt_t;

typedef struct p80211msg_lnxreq_wlansniff
{
	unsigned long	msgcode	__attribute__ ((packed));
	unsigned long   msglen	__attribute__ ((packed));
	unsigned char	devname[DEVNAME_LEN]	__attribute__ ((packed));
	p80211item_t	enable	__attribute__ ((packed));
	p80211item_t	channel	__attribute__ ((packed));
	p80211item_t	prismheader	__attribute__ ((packed));
	p80211item_t	wlanheader	__attribute__ ((packed));
	p80211item_t	keepwepflags	__attribute__ ((packed));
	p80211item_t	stripfcs	__attribute__ ((packed));
	p80211item_t	packet_trunc	__attribute__ ((packed));
	p80211item_t	resultcode	__attribute__ ((packed));
} __attribute__ ((packed)) p80211msg_lnxreq_wlansniff_t;

typedef struct p80211ioctl_req
{
	char 	        name[DEVNAME_LEN]	__attribute__ ((packed)) ;
	void	       *data			__attribute__ ((packed)) ;
	unsigned long	magic			__attribute__ ((packed)) ;
	unsigned short	len			__attribute__ ((packed)) ;
	unsigned long	result			__attribute__ ((packed)) ;
} __attribute__ ((packed)) p80211ioctl_req_t;

static char const * const wepFlags[] = {
   "clear", "WEP"
};

#define P80211_IOCTL_MAGIC	(0x4a2d464dUL)
#define P80211_IFREQ		(SIOCDEVPRIVATE + 1)

typedef struct p80211msgd
{
	unsigned long	msgcode			__attribute__ ((packed));
	unsigned long	msglen			__attribute__ ((packed));
	unsigned char	devname[DEVNAME_LEN]	__attribute__ ((packed));
	unsigned char	args[0]			__attribute__ ((packed));
} __attribute__ ((packed)) p80211msgd_t;


sniffWLAN_t :: sniffWLAN_t( void )
   : cancel_( false )
   , firstAP_( 0 )
   , fd_( socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL)) )
{
   if( 0 <= fd_ )
   {
      struct ifreq ifr ;
      memset(&ifr, 0, sizeof(ifr));
      strcpy(ifr.ifr_name, "wlan0" );
      
      if (ioctl(fd_, SIOCGIFFLAGS, &ifr) != -1) 
      {
         ifr.ifr_flags |= IFF_PROMISC;
         if (ioctl(fd_, SIOCSIFFLAGS, &ifr) != -1) 
         {
            if(ioctl(fd_, SIOCGIFHWADDR, &ifr) != -1) 
            {
               if( -1 != ioctl(fd_, SIOCGIFINDEX, &ifr) )
               {
                  struct sockaddr_ll   sll;

                  memset(&sll, 0, sizeof(sll));
                  sll.sll_family    = AF_PACKET;
                  sll.sll_ifindex   = ifr.ifr_ifindex ;
                  sll.sll_protocol  = htons(ETH_P_ALL);

	          if( -1 != bind(fd_, (struct sockaddr *) &sll, sizeof(sll))) 
                  {
                     return ; // setup complete
                  }
                  else
                     perror( "bind" );
               }
               else
                  perror( "GIFINDEX" );
            }
            else
               perror( "GIFHWADDR" );
            
            if (ioctl(fd_, SIOCGIFFLAGS, &ifr) != -1) 
            {
               //
               // get out of promiscuous mode
               //
               ifr.ifr_flags &= ~IFF_PROMISC ;
               ioctl(fd_, SIOCSIFFLAGS, &ifr);
            }
         }
         else
            perror( "IFF_PROMISC" );
      }
      else
         perror( "GIFFLAGS" );
      ::close( fd_ );
      fd_ = -1 ;
   }
   else
      perror( "socket" );
}

sniffWLAN_t :: ~sniffWLAN_t( void )
{
   close();
   clearAPs();
}
   
void sniffWLAN_t :: clearAPs( void )
{
   while( firstAP_ )
   {
      ap_t *temp = firstAP_ ;
      firstAP_ = temp->next_ ;
      delete temp ;
   }
}

void sniffWLAN_t :: close( void )
{
   if( sniffing() )
   {
      struct ifreq ifr ;
      memset(&ifr, 0, sizeof(ifr));
      strcpy(ifr.ifr_name, "wlan0" );
      
      if (ioctl(fd_, SIOCGIFFLAGS, &ifr) != -1) 
      {
         //
         // get out of promiscuous mode
         //
         ifr.ifr_flags &= ~IFF_PROMISC ;
         ioctl(fd_, SIOCSIFFLAGS, &ifr);
      }
   }
}

void sniffWLAN_t :: sniff( unsigned channel, unsigned seconds )
{
   p80211msg_lnxreq_wlansniff_t sniff ;
   memset( &sniff, 0, sizeof( sniff ) );

   sniff.msgcode = 0x83 ; // wlan sniff
   sniff.msglen  = sizeof( sniff );   // 120 == 96 body + 4 + 4 + 16
   strcpy( (char *)sniff.devname, "wlan0" );

   sniff.enable.did     = 0x00001083 ;
   sniff.enable.status  = 0x0000 ;
   sniff.enable.len     = 0x0004 ;
   sniff.enable.data    = 0x00000001 ;

   sniff.channel.did     = 0x00002083 ;
   sniff.channel.status  = 0x0000 ;
   sniff.channel.len     = 0x0004 ;
   sniff.channel.data    = 0 ; // channel # goes here

   sniff.prismheader.did     = 0x00003083 ;
   sniff.prismheader.status  = 0x0000 ;
   sniff.prismheader.len     = 0x0004 ;
   sniff.prismheader.data    = 0x00000001 ;

   sniff.wlanheader.did     = 0x00004083 ;
   sniff.wlanheader.status  = 0x0001 ;
   sniff.wlanheader.len     = 0x0004 ;
   sniff.wlanheader.data    = 0x00000000 ;

   sniff.keepwepflags.did     = 0x00005083 ;
   sniff.keepwepflags.status  = 0x0000 ;
   sniff.keepwepflags.len     = 0x0004 ;
   sniff.keepwepflags.data    = 0x00000000 ;

   sniff.stripfcs.did     = 0x00006083 ;
   sniff.stripfcs.status  = 0x0001 ;
   sniff.stripfcs.len     = 0x0004 ;
   sniff.stripfcs.data    = 0x00000000 ;
   
   sniff.packet_trunc.did     = 0x00007083 ;
   sniff.packet_trunc.status  = 0x0001 ;
   sniff.packet_trunc.len     = 0x0004 ;
   sniff.packet_trunc.data    = 0x00000000 ;

   sniff.resultcode.did     = 0x00008083 ;
   sniff.resultcode.status  = 0x0000 ;
   sniff.resultcode.len     = 0x0004 ;
   sniff.resultcode.data    = 0x00000000 ; // output
                        
   sniff.channel.data = channel ;
   p80211ioctl_req ioctlReq ;
   strcpy( ioctlReq.name, "wlan0" );
   ioctlReq.data = &sniff ;
   ioctlReq.magic = P80211_IOCTL_MAGIC ;
   ioctlReq.len  = sizeof( sniff );
   ioctlReq.result = 0 ;
   int ioctlRes = ioctl( fd_, P80211_IFREQ, &ioctlReq);
   if( 0 == ioctlRes )
   {
      long long const end = tickMs() + ( seconds * 1000 );

      while( !cancel_ && ( tickMs() < end ) )
      {
         pollfd filedes ;
         filedes.fd = fd_ ;
         filedes.events = POLLIN ;
         if( 0 < poll( &filedes, 1, 100 ) && ( 0 != ( filedes.revents & POLLIN ) ) )
         {
            unsigned char buf[2048];
            struct sockaddr_ll from;
            socklen_t fromLen = sizeof( from );
            unsigned const dataBytes = recvfrom( fd_, buf, sizeof( buf ), MSG_TRUNC, (struct sockaddr *)&from, &fromLen );
            if( 0 <= dataBytes )
            {
               AdmInfo_t const   *A = (AdmInfo_t *) buf; /* First the frame from the card */
               FixedMgmt_t       *M = (FixedMgmt_t *)(A+1); /*Then the actual data*/

               if( MGT_BEACON == M->frametype )
               {
                  unsigned char *ap = M->SrcAddr ;
                  ap[0] = '\0' ;
                  char ssid[32];
                  ssid[0] = '\0' ; // flag not present
                  unsigned frameChannel = 0 ;
                  bool     hasWep  = IS_WEP_REQUIRED(M->Capabilities);
                  unsigned char const *varBits = (unsigned char *)(M+1);
                  unsigned char const *end = buf + dataBytes ;
                  while( varBits < end )
                  {
                     unsigned const tagType = varBits[0];
                     unsigned const tagLen  = varBits[1];
                     varBits+=2;

                     switch(tagType)
                     {
                        case TAG_SSID:
                           {
                              unsigned const ssidLen = ( tagLen >= sizeof( ssid ) )
                                                       ? sizeof( ssid ) - 1 
                                                       : tagLen ;
                              memcpy( ssid, varBits, ssidLen );
                              ssid[ssidLen] = '\0' ;
                              break;
                           }
                        case TAG_DS_PARAMETER:
                           {
                              if( 1 == tagLen )
                                 frameChannel = *varBits ;
                              break;
                           }
                        /* Skip all other values for the moment */
                        case TAG_SUPP_RATES:
                           {
                              break;
                           }
                        case TAG_TIM: // ?? beacon interval
                           {
                              break;
                           }
                        case TAG_FH_PARAMETER:
                        case TAG_CF_PARAMETER:
                        case TAG_IBSS_PARAMETER :
                        case TAG_CHALLENGE_TEXT:
                           {
                              break;
                           }
                     }	
                     varBits+=tagLen;
                  }

                  if( channel == frameChannel )
                  {
                     ap_t *ap = firstAP_ ;
                     while( ap )
                     {
                        if( 0 == memcmp( ap->apMac_, M->SrcAddr, sizeof( ap->apMac_ ) ) )
                        {
                           break;
                        }
                        else
                           ap = ap->next_ ;
                     }

                     if( 0 == ap )
                     {
                        ap = new ap_t ;
                        memcpy( ap->bssid_, M->BssId, sizeof( ap->bssid_ ) );
                        memcpy( ap->apMac_, M->SrcAddr, sizeof( ap->apMac_ ) );
                        ap->requiresWEP_ = hasWep ;
                        strcpy( ap->ssid_, ssid );
                        ap->channel_ = frameChannel ;
                        ap->signal_ = A->signal.data ;
                        ap->noise_  = A->noise.data ;
                        ap->next_ = firstAP_ ;
                        firstAP_ = ap ;
                        onNew( *ap );
                     }
                  }
               } // beacon
            }
            else
            {
               perror( "recv" );
               break;
            }
         } // something to read
      } // until timeout
      
      sniff.enable.data    = 0x00000000 ;
      p80211ioctl_req ioctlReq ;
      strcpy( ioctlReq.name, "wlan0" );
      ioctlReq.data = &sniff ;
      ioctlReq.magic = P80211_IOCTL_MAGIC ;
      ioctlReq.len  = sizeof( sniff );
      ioctlReq.result = 0 ;
      int ioctlRes = ioctl( fd_, P80211_IFREQ, &ioctlReq);
      if( 0 != ioctlRes )
         perror( "Turning off sniff" );
   }
   else
      perror( "P80211_IFREQ" );

}

void sniffWLAN_t :: onNew( ap_t &newAP )
{
}

// #define __MODULETEST__

#ifdef __MODULETEST__

class mySniffer_t : public sniffWLAN_t {
public:
   void onNew( ap_t &newOne )
   {
      printf( "BSS %.2X:%.2X:%.2X:%.2X:%.2X:%.2X"
              " AP %.2X:%.2X:%.2X:%.2X:%.2X:%.2X"
              " %s SSID %s, channel %u, signal %u, noise %u\n", 
              newOne.bssid_[0], newOne.bssid_[1], newOne.bssid_[2],
              newOne.bssid_[3], newOne.bssid_[4], newOne.bssid_[5],
              newOne.apMac_[0], newOne.apMac_[1], newOne.apMac_[2],
              newOne.apMac_[3], newOne.apMac_[4], newOne.apMac_[5],
              wepFlags[newOne.requiresWEP_],
              newOne.ssid_,
              newOne.channel_, newOne.signal_, newOne.noise_ );
   }
};

int main( void )
{
   mySniffer_t sniffer ;
   if( sniffer.sniffing() )
   {   
      for( unsigned ch = 1 ; ch <= 11 ; ch++ )
      {
         printf( "---> channel %u\n", ch );
         sniffer.sniff( ch, 1 );
         sniffer.clearAPs();
      }
   }
   else
      fprintf( stderr, "Error opening sniff channel\n" );

   //
   // worker interface
   //

   
}
#endif
