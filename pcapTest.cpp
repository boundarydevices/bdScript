/*
 * Module pcapTest.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: pcapTest.cpp,v $
 * Revision 1.1  2003-08-10 15:15:58  ericn
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

static char const * const wepFlags[] = {
   "clear", "WEP"
};

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
                     while( 1 )
                     {
                        fd_set rs;
   
                        FD_ZERO(&rs);
                        FD_SET(sock_fd,&rs);
                        FD_SET(0,&rs); /* Add stdin to it aswell */
                        
                        struct timeval tm;
                        tm.tv_sec= 10 ;
                        tm.tv_usec= 0 ;
      	                int r = select(sock_fd+1,&rs,NULL,NULL,&tm);
                        if( r <= 0 )
                        {
                           printf( "timeout: %u\n", r );
                           break;
                        }
                        else
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
                                 AdmInfo_t const *A = (AdmInfo_t *) admbits; /* First the frame from the card */
	                         FixedMgmt_t     *M = (FixedMgmt_t *) fixbits; /*Then the actual data*/
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
                                       char ssid[32];
                                       ssid[0] = '\0' ; // flag not present
                                       unsigned channel = 0 ;
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
                                                      channel = *varBits ;
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
                                       if( ssid[0] && ( 0 != channel ) )
                                       {
                                          printf( "BSS %.2X:%.2X:%.2X:%.2X:%.2X:%.2X "
                                                  "SSID %s, channel %u, signal %u, noise %u\n", 
                                                  M->BssId[0], M->BssId[1],
                                                  M->BssId[2], M->BssId[3],
                                                  M->BssId[4], M->BssId[5],
                                                  ssid, channel, A->signal.data, A->noise.data );
                                       }
                                    }
                                    else
                                       printf( "M:%04x\n", M->frametype );

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
                     } // until error or stop

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

