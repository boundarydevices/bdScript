#include <wlan/wlan_compat.h>
#include <wlan/version.h>
#include <wlan/p80211types.h>
#include <wlan/p80211meta.h>
#include <wlan/p80211metamsg.h>
#include <wlan/p80211msg.h>
#include <wlan/p80211ioctl.h>
#include <wlan/p80211metastruct.h>
#include <wlan/p80211metadef.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "hexDump.h"

int main( void )
{
   int const fd = socket(AF_INET, SOCK_STREAM, 0);
   if( 0 <= fd )
   {
      p80211ioctl_req_t         req;
      memset( &req, 0, sizeof( req ) );

      p80211msg_p2req_readcis_t cisReq ;
      memset( &cisReq, 0, sizeof( cisReq ) );

      req.magic = P80211_IOCTL_MAGIC;
      req.len = sizeof( cisReq );
      req.data = &cisReq ;
      strcpy( req.name, "wlan0" );
      req.result = 0;
      cisReq.msgcode = DIDmsg_p2req_readcis ; // wlan sniff
      cisReq.msglen = sizeof( cisReq );
      strcpy( (char *)cisReq.devname, "wlan0" );

      cisReq.cis.did    = DIDmsg_p2req_readcis_cis ;
      cisReq.cis.status = 0 ;
      cisReq.cis.len    = sizeof( cisReq.cis.data );
      
      cisReq.resultcode.did    = DIDmsg_p2req_readcis_resultcode ;
      cisReq.resultcode.status = 0 ;
      cisReq.resultcode.len    = sizeof( cisReq.resultcode.data );

      int result = ioctl( fd, P80211_IFREQ, &req);
      printf( "result == %x\n", result );
      if( 0 == result )
      {
         printf( "cis: %08lx %04x %04x\n", cisReq.cis.did, cisReq.cis.status, cisReq.cis.len );
         hexDumper_t dumpCIS( cisReq.cis.data, cisReq.cis.len );
         while( dumpCIS.nextLine() )
            printf( "%s\n", dumpCIS.getLine() );
         printf( "resultcode: %08lx %04x %04x\n", cisReq.resultcode.did, cisReq.resultcode.status, cisReq.resultcode.len );
         printf( "      data: %08lx\n", cisReq.resultcode.data );
      }
      close( fd );
   }
   else
      perror( "socket" );
      
   return 0 ;
}
