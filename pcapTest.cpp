/*
 * Program pcapTest.cpp
 *
 * This program tests the sniffing capabilities of the
 * Prism WLAN driver. This code was taken from the
 * prismstumbler project:
 *
 *    http://sourceforge.net/projects/prismstumbler/
 *
 * Change History : 
 *
 * $Log: pcapTest.cpp,v $
 * Revision 1.5  2003-08-10 21:21:29  ericn
 * -modified to walk channels
 *
 * Revision 1.4  2003/08/10 19:06:39  ericn
 * -added AP to output
 *
 * Revision 1.3  2003/08/10 17:25:16  ericn
 * -modified to set/clear sniff mode
 *
 * Revision 1.2  2003/08/10 15:38:41  ericn
 * -added WEP flag to output
 *
 * Revision 1.1  2003/08/10 15:15:58  ericn
 * -minimal WLAN sniffer
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


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
#include "hexDump.h"
#include <asm/types.h>
#include <time.h>

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

typedef struct {
	unsigned short frametype;
	unsigned short duration;
	unsigned char DestAddr[6];
	unsigned char SrcAddr[6];
	unsigned char BssId[6];
	unsigned short FragSeq;
}ProbeFixedMgmt_t;

typedef struct {
	char DestMac[20];
	char SrcMac[20];
	char BssId[20];
	char SSID[80];
	int hasWep;
	int isAp;
	int  Channel;
	int Signal;
    int Noise;
    int FrameType;
	float longitude, latitude;
	int isData;
	int hasIntIV;
	int dhcp;
	unsigned char subnet[5];
}ScanResult_t;

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

static void dump_msg(void *msg)
{
	p80211msgd_t	*msgp = (p80211msgd_t *)msg;
	int 		i;
	int		bodylen;

	fprintf(stderr, "  msgcode=0x%08lx  msglen=%lu  devname=%s\n",
			msgp->msgcode, msgp->msglen, msgp->devname);
	fprintf(stderr, "body: ");
	bodylen=msgp->msglen - 
		(sizeof(msgp->msgcode) +
		 sizeof(msgp->msglen) +
		 sizeof(msgp->devname));
	for ( i = 0; i < bodylen; i+=4) {
		fprintf(stderr, "%02x%02x%02x%02x ", 
			msgp->args[i], msgp->args[i+1], 
			msgp->args[i+2], msgp->args[i+3]);
	}
	fprintf(stderr,"\n");
}

int main( void )
{
   int sock_fd = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
   if( 0 <= sock_fd )
   {
printf( "sizeof( FixedMgmt_t ) == %u\n", sizeof( FixedMgmt_t ) );
      printf( "PF_PACKET socket opened\n" );
      struct ifreq ifr ;
      memset(&ifr, 0, sizeof(ifr));
      strcpy(ifr.ifr_name, "wlan0" );
      
      if (ioctl(sock_fd, SIOCGIFFLAGS, &ifr) != -1) 
      {
         printf( "flags %d\n", ifr.ifr_flags );
      
         ifr.ifr_flags |= IFF_PROMISC;
         if (ioctl(sock_fd, SIOCSIFFLAGS, &ifr) != -1) 
         {
            printf( "IFF_PROMISC...\n" );
            if(ioctl(sock_fd, SIOCGIFHWADDR, &ifr) != -1) 
            {
// 1 == ARPHRD_ETHER, pcap uses DLT_EN10MB

               printf( "hwFamily %d\n", ifr.ifr_hwaddr.sa_family );
               if( -1 != ioctl(sock_fd, SIOCGIFINDEX, &ifr) )
               {
                  printf( "index %d\n", ifr.ifr_ifindex );
                  struct sockaddr_ll   sll;

                  memset(&sll, 0, sizeof(sll));
                  sll.sll_family    = AF_PACKET;
                  sll.sll_ifindex   = ifr.ifr_ifindex ;
                  sll.sll_protocol  = htons(ETH_P_ALL);

                  int oldFlags = fcntl(0, F_GETFL);
                  fcntl( 0, F_SETFL, oldFlags | O_NONBLOCK);

	          if( -1 != bind(sock_fd, (struct sockaddr *) &sll, sizeof(sll))) 
                  {
                     printf( "bound\n" );

                     p80211msg_lnxreq_wlansniff_t sniffBase ;
                     memset( &sniffBase, 0, sizeof( sniffBase ) );

                     sniffBase.msgcode = 0x83 ; // wlan sniffBase
                     sniffBase.msglen  = sizeof( sniffBase );   // 120 == 96 body + 4 + 4 + 16
                     strcpy( (char *)sniffBase.devname, "wlan0" );

                     sniffBase.enable.did     = 0x00001083 ;
                     sniffBase.enable.status  = 0x0000 ;
                     sniffBase.enable.len     = 0x0004 ;
                     sniffBase.enable.data    = 0x00000001 ;

                     sniffBase.channel.did     = 0x00002083 ;
                     sniffBase.channel.status  = 0x0000 ;
                     sniffBase.channel.len     = 0x0004 ;
                     sniffBase.channel.data    = 0 ; // channel # goes here

                     sniffBase.prismheader.did     = 0x00003083 ;
                     sniffBase.prismheader.status  = 0x0000 ;
                     sniffBase.prismheader.len     = 0x0004 ;
                     sniffBase.prismheader.data    = 0x00000001 ;

                     sniffBase.wlanheader.did     = 0x00004083 ;
                     sniffBase.wlanheader.status  = 0x0001 ;
                     sniffBase.wlanheader.len     = 0x0004 ;
                     sniffBase.wlanheader.data    = 0x00000000 ;

                     sniffBase.keepwepflags.did     = 0x00005083 ;
                     sniffBase.keepwepflags.status  = 0x0000 ;
                     sniffBase.keepwepflags.len     = 0x0004 ;
                     sniffBase.keepwepflags.data    = 0x00000000 ;

                     sniffBase.stripfcs.did     = 0x00006083 ;
                     sniffBase.stripfcs.status  = 0x0001 ;
                     sniffBase.stripfcs.len     = 0x0004 ;
                     sniffBase.stripfcs.data    = 0x00000000 ;
                     
                     sniffBase.packet_trunc.did     = 0x00007083 ;
                     sniffBase.packet_trunc.status  = 0x0001 ;
                     sniffBase.packet_trunc.len     = 0x0004 ;
                     sniffBase.packet_trunc.data    = 0x00000000 ;

                     sniffBase.resultcode.did     = 0x00008083 ;
                     sniffBase.resultcode.status  = 0x0000 ;
                     sniffBase.resultcode.len     = 0x0004 ;
                     sniffBase.resultcode.data    = 0x00000000 ; // output
                     for( unsigned channel = 1 ; channel < 12 ; channel++ )
                     {
                        printf( "----> channel %u\n", channel );
                        p80211msg_lnxreq_wlansniff_t sniff = sniffBase ;
                        sniff.channel.data = channel ;
                        p80211ioctl_req ioctlReq ;
                        strcpy( ioctlReq.name, "wlan0" );
                        ioctlReq.data = &sniff ;
                        ioctlReq.magic = P80211_IOCTL_MAGIC ;
                        ioctlReq.len  = sizeof( sniff );
                        ioctlReq.result = 0 ;
                        int ioctlRes = ioctl( sock_fd, P80211_IFREQ, &ioctlReq);
                        if( 0 == ioctlRes )
                        {
                           time_t start ;
                           time( &start );

                           time_t now = start ;

                           while( now < start + 2 )
                           {
                              fd_set rs;
         
                              FD_ZERO(&rs);
                              FD_SET(sock_fd,&rs);
                              FD_SET(0,&rs); /* Add stdin to it aswell */
                              
                              struct timeval tm;
                              tm.tv_sec= 1 ;
                              tm.tv_usec= 0 ;
            	                int r = select(sock_fd+1,&rs,NULL,NULL,&tm);
                              if( r > 0 )
                              {
                                 if( FD_ISSET( 0, &rs ) )
                                 {
                                    char inChar ;
                                    if( ( 1 == read( 0, &inChar, 1 ) )
                                        && 
                                        ( ( inChar == 'Q' ) 
                                          ||
                                          ( inChar == 'q') ) )
                                       break;
                                 }
      
                                 if( FD_ISSET( sock_fd, &rs ) ) 
                                 {
                                    unsigned char buf[2048];
                                    struct sockaddr_ll from;
                                    socklen_t fromLen = sizeof( from );
                                    unsigned const dataBytes = recvfrom( sock_fd, buf, sizeof( buf ), MSG_TRUNC, (struct sockaddr *)&from, &fromLen );
                                    if( 0 <= dataBytes )
                                    {
                                       unsigned char const *admbits = buf ;
                                       unsigned char const *fixbits = buf+sizeof(AdmInfo_t); /* This is where the payload starts*/
                                       AdmInfo_t const   *A = (AdmInfo_t *) admbits; /* First the frame from the card */
                                       FixedMgmt_t       *M = (FixedMgmt_t *) fixbits; /*Then the actual data*/
                                       ProbeFixedMgmt_t  *P = (ProbeFixedMgmt_t *) fixbits; /*Then the actual data*/
      
                                       ScanResult_t Res ;
      	
                                       memset(&Res,0,sizeof(Res));
      
                                       Res.FrameType = M->frametype;
      
                                       if( M->frametype  == MGT_PROBE_RESP || M->frametype == MGT_BEACON ){
                                          Res.isAp   = IS_TO_DS(COOK_FLAGS(M->frametype)) != 0;
                                          Res.isAp   += IS_FROM_DS(COOK_FLAGS(M->frametype)) != 0; // in this case we have more than one ap
                                          Res.hasWep = IS_WEP(COOK_FLAGS(M->frametype)) != 0;
                                          Res.hasWep += IS_WEP_REQUIRED(M->Capabilities) != 0;
                                       }
                                       if( M->frametype  == MGT_PROBE || M->frametype == MGT_BEACON ){
                                          if(! ( M->DestAddr[0] == 0xff && M->DestAddr[1] == 0xff &&
                                          M->DestAddr[2] == 0xff && M->DestAddr[3] == 0xff &&
                                          M->DestAddr[4] == 0xff && M->DestAddr[5] == 0xff
                                          )) printf( "non-broadcast beacon\n" );
                                       }
                                       
                                       if( M->frametype  == MGT_PROBE  ){
                                          Res.isAp   = AP_PROBE  ;
                                          Res.hasWep =  0;
                                          Res.Channel = 0;
                                       }
                                       
                                       Res.Signal = A->signal.data;
                                       Res.Noise = A->noise.data;
      
                                       if (M->frametype & DATA_PURE) // do we have a data packet?
                                       {
                                          int const sdt = (M->frametype & 0x0300) >> 8 ;
                                          static char const * const srcDest[] = {
                                             "NN",    // node to node
                                             "AN",    // AP to node
                                             "NA",    // Node to AP
                                             "AA"     // AP to AP
                                          };
                                          static unsigned const hdrLengths[] = {
                                             DATA_SHORT_HDR_LEN,    // node to node
                                             DATA_SHORT_HDR_LEN,    // AP to node
                                             DATA_SHORT_HDR_LEN,    // Node to AP
                                             DATA_LONG_HDR_LEN      // AP to AP
                                          };
                                          
                                          unsigned const hdrlen = hdrLengths[sdt];
                                          unsigned char *iv = buf + hdrlen + sizeof( AdmInfo_t ) - 8 ;
                                          bool const wep = IS_WEP(COOK_FLAGS(M->frametype));
      
                                          printf( "D:%u/%u:%s:%s\n", fromLen, dataBytes, srcDest[sdt], wepFlags[wep] );
                                       }
                                       else
                                       {
                                          if( MGT_BEACON == M->frametype )
                                          {
                                             unsigned char *ap = M->SrcAddr ;
                                             ap[0] = '\0' ;
                                             char ssid[32];
                                             ssid[0] = '\0' ; // flag not present
                                             unsigned frameChannel = 0 ;
                                             bool     hasWep  = IS_WEP_REQUIRED(M->Capabilities);
                                             unsigned char const *varBits = &fixbits[sizeof(FixedMgmt_t)];
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
      /*
                                                         printf( "   tag %u\n", tagType );
                                                         hexDumper_t dumpTag( varBits, tagLen );
                                                         while( dumpTag.nextLine() )
                                                            printf( "   %s\n", dumpTag.getLine() );
      */                                                      
                                                         break;
                                                      }
                                                }	
                                                varBits+=tagLen;
                                             }
                                             if( ssid[0] )
                                             {
                                                if( channel == frameChannel )
                                                {
                                                   printf( "BSS %.2X:%.2X:%.2X:%.2X:%.2X:%.2X"
                                                           " AP %.2X:%.2X:%.2X:%.2X:%.2X:%.2X"
                                                           " %s SSID %s, channel %u, signal %u, noise %u\n", 
                                                           M->BssId[0], M->BssId[1],
                                                           M->BssId[2], M->BssId[3],
                                                           M->BssId[4], M->BssId[5],
                                                           M->SrcAddr[0], M->SrcAddr[1],
                                                           M->SrcAddr[2], M->SrcAddr[3],
                                                           M->SrcAddr[4], M->SrcAddr[5],
                                                           wepFlags[hasWep],
                                                           ssid, frameChannel, A->signal.data, A->noise.data );
                                                }
                                                else
                                                   printf( "leakCH %u, signal %u, noise %u\n", frameChannel, A->signal.data, A->noise.data );
                                             }
                                          }
                                          else
                                          {
//                                             printf( "M:%04x\n", M->frametype );
                                          }
      
                                       } // Mgmt frame
      /*
                                       hexDumper_t dumpHeader( &from, fromLen );
                                       while( dumpHeader.nextLine() )
                                          printf( "%s\n", dumpHeader.getLine() );
                                       hexDumper_t dumpRx( buf, dataBytes );
                                       while( dumpRx.nextLine() )
                                          printf( "%s\n", dumpRx.getLine() );
      */                                    
                                    }
                                    else
                                    {
                                       perror( "recv" );
                                       break;
                                    }
                                 }
                              }

                              time( &now );
                           }
                        }
                        else
                           perror( "P80211_IFREQ" );
                     } // for each channel

                     sniffBase.enable.data    = 0x00000000 ;
                     p80211ioctl_req ioctlReq ;
                     strcpy( ioctlReq.name, "wlan0" );
                     ioctlReq.data = &sniffBase ;
                     ioctlReq.magic = P80211_IOCTL_MAGIC ;
                     ioctlReq.len  = sizeof( sniffBase );
                     ioctlReq.result = 0 ;
                     int ioctlRes = ioctl( sock_fd, P80211_IFREQ, &ioctlReq);
                     if( 0 != ioctlRes )
                        perror( "Turning off sniff" );
                     fcntl( 0, F_SETFL, oldFlags );

                  }
                  else
                     perror( "bind" );
               }
               else
                  perror( "GIFINDEX" );

            }
            else
               perror( "GIFHWADDR" );
         }
         else
            perror( "IFF_PROMISC" );

         //
         // get out of promiscuous mode
         //
         ifr.ifr_flags &= ~IFF_PROMISC ;
         ioctl(sock_fd, SIOCSIFFLAGS, &ifr);
      }
      else
      {
         printf( "ioctl: %m" );
      }
   }
   else
      perror( "socket" );

   return 0 ;
}

