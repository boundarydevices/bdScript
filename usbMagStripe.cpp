#include <usb.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BOUNDARYVENDOR 0x5a55
#define MAGSTRIPE 0x01

int main( int argc, char const * const argv[] )
{
   unsigned long vendor = BOUNDARYVENDOR ;
   unsigned long product = MAGSTRIPE ;
   if( 2 <= argc )
   {
      vendor = strtoul( argv[1], 0, 0 );
      if( 3 <= argc )
         product = strtoul( argv[2], 0, 0 );
         
      printf( "vendor/prod: %04x/%04x\n", vendor, product );
   }
   
   struct usb_bus *busses;
   
   usb_init();
   usb_find_busses();
   usb_find_devices();
   
   busses = usb_get_busses();
   printf( "busses == %p\n", busses );
   
   struct usb_device *mag = 0 ;
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
                 , dev
                 , dev->descriptor.bDeviceClass
                 , dev->descriptor.idVendor
                 , dev->descriptor.idProduct
                 , dev->descriptor.iManufacturer
                 , dev->descriptor.iProduct
                 , dev->filename
                 );
         if( ( vendor == dev->descriptor.idVendor )
             &&
             ( product == dev->descriptor.idProduct ) )
             mag = dev ;
         for( int c = 0; c < dev->descriptor.bNumConfigurations; c++) {
            printf( "   cfg[%d]\n", c );
            /* Loop through all of the interfaces */
            for (int i = 0; i < dev->config[c].bNumInterfaces; i++) {
               printf( "      iface[%d]\n", i );
               /* Loop through all of the alternate settings */
               for( int a = 0; a < dev->config[c].interface[i].num_altsetting; a++) {
                  printf( "      alt[%d]\n"
                          "         ifclass %d\n"
                          , a
                          , dev->config[c].interface[i].altsetting[a].bInterfaceClass );
   
                  struct usb_endpoint_descriptor *endpoint = dev->config[c].interface[i].altsetting[a].endpoint;
                  if( endpoint )
                  {
                     printf( "      ep: attr 0x%02x\n"
                             "      addr: 0x%02x\n"
                             "      max: %u\n"
                             "      type: %u\n"
                             "      interval: %u\n"
                             "      refresh: %u\n"
                             "      syncaddr: %u\n"
                             , endpoint->bmAttributes
                             , endpoint->bEndpointAddress
                             , endpoint->wMaxPacketSize 
                             , endpoint->bDescriptorType
                             , endpoint->bInterval
                             , endpoint->bRefresh
                             , endpoint->bSynchAddress
                     );
                     if( dev == mag )
                     {
                        addr  = endpoint->bEndpointAddress ;
                        maxIn = endpoint->wMaxPacketSize ;
                        printf( "    matched!\n" );
                     }
                  }
               }
            }
         }
      }
   }

   if( mag )
   {
      printf( "magstripe reader found: %p\n", mag );
      usb_dev_handle *devh = usb_open(mag);

      if( devh )
      {
         if( 0 == usb_set_configuration(devh,1) ) // set HID
         {
            printf( "magstripe device opened\n" );
            if( 0 == usb_claim_interface( devh, 0 ) )
            {
               printf( "claimed interface\n" );
               char buf[8];
               memset(buf, 0x55aa55aa, sizeof(buf));
/*
               int rval = usb_control_msg( devh, USB_ENDPOINT_IN + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                                           1, // HID_GET_REPORT, 
                                           0, // (HID_RT_FEATURE << 8),
                                           0,
                                           buf, sizeof(buf), 100 );
               printf( "ctrl msg: %d\n", rval );
*/

               int rval ;   
               do {
                  rval = usb_interrupt_read( devh, addr, buf, maxIn, 0 );
                  if( 0 < rval )
                  {
                     for( int i = 0 ; i < rval ; i++ )
                        printf( "%02x ", buf[i] );
                     printf( "\n" );
                  }
               } while( ( 0 <= rval ) || ( -ETIMEDOUT == rval ) );
               
               if( 0 == usb_release_interface( devh, 0 ) )
               {
                  printf( "released interface\n" );
               }
               else
                  printf( "Error releasing interface\n" );
            }
            else
               printf( "Error claiming interface\n" );
            
            if( 0 == usb_close( devh ) )
               printf( "device closed\n" );
            else
               printf( "Error closing magstripe device\n" );
         }
         else
            printf( "Error setting config\n" );
      }
      else
         printf( "Error opening magstripe device\n" );
   }
      
   return 0 ;
}


