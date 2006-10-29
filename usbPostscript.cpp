#include <usb.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define VENDOR_IBM         0x043d
#define PRODUCT_IP1512     0x00c8

struct vendorAndProduct_t {
   unsigned short vendor_ ;
   unsigned short product_ ;
};

static struct vendorAndProduct_t const compatible[] = {
   { vendor_:  VENDOR_IBM
   , product_: PRODUCT_IP1512 }
};

static unsigned const numCompatible_ = sizeof(compatible)/sizeof(compatible[0]);

static bool findCompatible( unsigned short vendor,
                            unsigned short product,
                            unsigned      &which )
{
   for( unsigned i = 0 ; i < numCompatible_; i++ ){
      if( ( vendor == compatible[i].vendor_ )
          &&
          ( product == compatible[i].product_ ) ){
         which = i ;
         return true ;
      }
   }

   which = -1U ;
   return false ;
}

static char const * const endpointTypes_[] = {
   "CONTROL"
,  "ISOCHRONOUS"
,  "BULK"
,  "INTERRUPT"
};

static unsigned const BAD_ENDPOINT = -1U ;

#include "hexDump.h"
#include "memFile.h"

int main( int argc, char const * const argv[] )
{
   struct usb_bus *busses;
   
   usb_init();
   usb_find_busses();
   usb_find_devices();
   
   busses = usb_get_busses();
   printf( "busses == %p\n", busses );
   
   struct usb_device *printer = 0 ;
   unsigned configNum = 0 ;
   unsigned interfaceNum = 0 ;
   struct usb_endpoint_descriptor *outputEndpoint = 0 ;
   struct usb_endpoint_descriptor *inputEndpoint = 0 ;
   unsigned maxIn = 8 ;
   unsigned addr = 0x81 ;
   for (struct usb_bus *bus = busses; bus; bus = bus->next) {
      for (struct usb_device *dev = bus->devices; dev; dev = dev->next) {
         /* Check if this device is a printer */
         printf( "-- device %p\n"
                 "   class %u\n"
                 "   vendor %04x\n"
                 "   product %04x\n"
                 "   mfr %02x\n"
                 "   iproduct %02x\n"
                 "   filename %s\n"
                 "   %u configurations\n"
                 , dev
                 , dev->descriptor.bDeviceClass
                 , dev->descriptor.idVendor
                 , dev->descriptor.idProduct
                 , dev->descriptor.iManufacturer
                 , dev->descriptor.iProduct
                 , dev->filename
                 , dev->descriptor.bNumConfigurations
                 );
         unsigned partIdx ;
         if( findCompatible( dev->descriptor.idVendor, 
                             dev->descriptor.idProduct,
                             partIdx ) ){
            printf( "------ This device is compatible: %u\n", partIdx );
            printer = dev ;
            for( int c = 0; c < dev->descriptor.bNumConfigurations; c++) {
               configNum = dev->config[c].bConfigurationValue ;
               printf( "   cfg[%d], %u interfaces, cfg #%u\n", c, dev->config[c].bNumInterfaces, configNum );
               /* Loop through all of the interfaces */
               for (int i = 0; i < dev->config[c].bNumInterfaces; i++) {
                  printf( "      iface[%d], %u alts\n", i, dev->config[c].interface[i].num_altsetting );
                  /* Loop through all of the alternate settings */
                  for( int a = 0; a < dev->config[c].interface[i].num_altsetting; a++) {
                     interfaceNum = dev->config[c].interface[i].altsetting[a].bInterfaceNumber ;
                     printf( "      alt[%d]\n"
                             "         ifclass %d\n"
                             "         %d endpoints \n"
                             "         iface #%u\n"
                             , a
                             , dev->config[c].interface[i].altsetting[a].bInterfaceClass 
                             , dev->config[c].interface[i].altsetting[a].bNumEndpoints
                             , interfaceNum 
                             );
                     unsigned numEndpoints = dev->config[c].interface[i].altsetting[a].bNumEndpoints ;
                     struct usb_endpoint_descriptor *endpoint = dev->config[c].interface[i].altsetting[a].endpoint;
                     if( endpoint )
                     {
                        for( unsigned ep = 0 ; ep < numEndpoints ; ep++, endpoint++ ){
                           printf( "      -----> endpoint %u\n"
                                   "      ep: attr 0x%02x, type %s\n"
                                   "      direction: %s\n"
                                   "      addr: 0x%02x\n"
                                   "      max: %u\n"
                                   "      type: %u\n"
                                   "      interval: %u\n"
                                   "      refresh: %u\n"
                                   "      syncaddr: %u\n"
                                   , ep
                                   , endpoint->bmAttributes
                                   , endpointTypes_[endpoint->bmAttributes&USB_ENDPOINT_TYPE_MASK]
                                   , ( 0 != (endpoint->bEndpointAddress & USB_ENDPOINT_IN) ) ? "IN" : "OUT" 
                                   , endpoint->bEndpointAddress
                                   , endpoint->wMaxPacketSize 
                                   , endpoint->bDescriptorType
                                   , endpoint->bInterval
                                   , endpoint->bRefresh
                                   , endpoint->bSynchAddress
                           );
                           if( USB_ENDPOINT_TYPE_BULK == ( endpoint->bmAttributes & USB_ENDPOINT_TYPE_MASK ) ){
                              if( endpoint->bEndpointAddress & USB_ENDPOINT_IN ){
                                 inputEndpoint = endpoint ;
                              }
                              else {
                                 outputEndpoint = endpoint ;
                              }
                           }
                           if( dev == printer )
                           {
                              addr  = endpoint->bEndpointAddress ;
                              maxIn = endpoint->wMaxPacketSize ;
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }

   if( printer )
   {
      printf( "Compatible printer found: %p\n", printer );
      usb_dev_handle *devh = usb_open(printer);

      if( devh )
      {
         if( 0 == usb_set_configuration(devh,configNum) ) // set configuration number
         {
            printf( "set config number %u\n", configNum );
            if( 0 == usb_claim_interface( devh, interfaceNum ) )
            {
               printf( "claimed interface %u\n", interfaceNum );
               if( ( 0 != outputEndpoint ) && ( 1 < argc ) ){
                  memFile_t fIn( argv[1] );
                  if( fIn.worked() ){
                     printf( "read %u bytes from %s\n", fIn.getLength(), argv[1] );
                     char const *nextOut = (char const *)fIn.getData();
                     unsigned numLeft = fIn.getLength();
                     while( 0 < numLeft ){
                        unsigned numToWrite = numLeft ;
                        if( numToWrite > outputEndpoint->wMaxPacketSize )
                           numToWrite = outputEndpoint->wMaxPacketSize ;
                        printf( "writing %u bytes\n", numToWrite );
                        int numWritten = usb_bulk_write( devh, 
                                                         outputEndpoint->bEndpointAddress,
                                                         (char *)nextOut, numToWrite, 100 );
                        if( 0 < numWritten ){
                           printf( "wrote %u bytes\n", numWritten );
                           numLeft -= numWritten ;
                           nextOut += numWritten ;
                        }
                        else {
                           printf( "Error: %u: %m\n", numWritten );
                           break ;
                        }
                     }
                  }
                  else
                     fprintf( stderr, "%s: %m\n", argv[1] );
               }
               
               if( 0 != inputEndpoint ){
                  char * const inData = new char [ inputEndpoint->wMaxPacketSize ];
                  do {
                     printf( "about to read %d bytes\n", inputEndpoint->wMaxPacketSize );
                     int const numRead = usb_bulk_read( devh, inputEndpoint->bEndpointAddress,
                                                        inData, inputEndpoint->wMaxPacketSize, 1000 );
                     if( 0 <= numRead ){
                        printf( "read %d bytes:\n", numRead );
                        hexDumper_t dumpRx( inData, numRead );
                        while( dumpRx.nextLine() )
                           printf( "%s\n", dumpRx.getLine() );
                     }
                     else {
                        printf( "Error: %d: %m\n", numRead );
                        break ;
                     }
                  } while( 1 );

                  delete [] inData ;
               }
            }
            else
               printf( "Error claiming interface %u\n", interfaceNum );
         }
         else
            printf( "Error setting config %u\n", configNum );
         
         if( 0 == usb_close( devh ) )
            printf( "device closed\n" );
         else
            printf( "Error closing printer device\n" );
      }
      else
         printf( "Error opening printer device\n" );
   }
   else
      printf( "No compatible device found\n" );
      
   return 0 ;
}


