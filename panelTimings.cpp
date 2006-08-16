#include <stdio.h>

struct lcd_panel_info_t {
   char const       *name ;
   unsigned long     pixclock;
   unsigned short    xres;
   unsigned short    yres;
   
   unsigned          act_high ;     // clock is active high
   unsigned          hsync_len;
   unsigned          left_margin;
   unsigned          right_margin;
   unsigned          vsync_len;
   unsigned          upper_margin;
   unsigned          lower_margin;
   unsigned          active ;       // active matrix (TFT) LCD
   unsigned          crt ;          // 1 == CRT, not LCD
   unsigned          rotation ;
};


static struct lcd_panel_info_t const lcd_panels_[] = {

   /* char const       *name         */   { "hitachi_qvga"
   /* unsigned long     pixclock     */    , 0
   /* unsigned short    xres         */    , 320       
   /* unsigned short    yres         */    , 240
   /* unsigned char     act_high     */    , 1
   /* unsigned char     hsync_len    */    , 64       
   /* unsigned char     left_margin  */    ,  1
   /* unsigned char     right_margin */    , 16       
   /* unsigned char     vsync_len    */    , 20       
   /* unsigned char     upper_margin */    , 8       
   /* unsigned char     lower_margin */    , 3       
   /* unsigned char     active       */    , 1
   /* unsigned char     crt          */    , 0 }

   /* char const       *name         */ , { "sharp_qvga"
   /* unsigned long     pixclock     */    , 0          
   /* unsigned short    xres         */    , 320        /* , 320  */
   /* unsigned short    yres         */    , 240        /* , 240  */
   /* unsigned char     act_high     */    , 1          /* , 1    */
   /* unsigned char     hsync_len    */    , 20         /* , 8    */
   /* unsigned char     left_margin  */    , 1          /* , 16   */
   /* unsigned char     right_margin */    , 30         /* , 1    */
   /* unsigned char     vsync_len    */    , 4          /* , 20   */
   /* unsigned char     upper_margin */    , 17         /* , 17   */
   /* unsigned char     lower_margin */    , 3          /* , 3    */
   /* unsigned char     active       */    , 1          /* , 1    */
   /* unsigned char     crt          */    , 0 }

   /* char const       *name         */ , { "qvga_portrait"
   /* unsigned long     pixclock     */    , 0
   /* unsigned short    xres         */    , 240
   /* unsigned short    yres         */    , 320
   /* unsigned char     act_high     */    , 1
   /* unsigned char     hsync_len    */    , 64       
   /* unsigned char     left_margin  */    , 34
   /* unsigned char     right_margin */    , 1       
   /* unsigned char     vsync_len    */    , 20       
   /* unsigned char     upper_margin */    , 8       
   /* unsigned char     lower_margin */    , 3       
   /* unsigned char     active       */    , 1
   /* unsigned char     crt          */    , 0 
   /* unsigned          rotation     */    , 90 }

   /* char const       *name         */ , { "hitachi_hvga"
   /* unsigned long     pixclock     */    , 1      
   /* unsigned short    xres         */    , 640
   /* unsigned short    yres         */    , 240
   /* unsigned char     act_high     */    , 1
   /* unsigned char     hsync_len    */    , 64       
   /* unsigned char     left_margin  */    , 34       
   /* unsigned char     right_margin */    , 1       
   /* unsigned char     vsync_len    */    , 20       
   /* unsigned char     upper_margin */    , 8       
   /* unsigned char     lower_margin */    , 3       
   /* unsigned char     active       */    , 1
   /* unsigned char     crt          */    , 0 }

   /* char const       *name         */ , { "sharp_vga"
   /* unsigned long     pixclock     */    , 1      
   /* unsigned short    xres         */    , 640
   /* unsigned short    yres         */    , 480
   /* unsigned char     act_high     */    , 1
   /* unsigned char     hsync_len    */    , 64       
   /* unsigned char     left_margin  */    , 60       
   /* unsigned char     right_margin */    , 60       
   /* unsigned char     vsync_len    */    , 20       
   /* unsigned char     upper_margin */    , 34       
   /* unsigned char     lower_margin */    , 3       
   /* unsigned char     active       */    , 1
   /* unsigned char     crt          */    , 0 }
   
   /* char const       *name         */ , { "hitachi_wvga"
   /* unsigned long     pixclock     */    , 1      
   /* unsigned short    xres         */    , 800
   /* unsigned short    yres         */    , 480
   /* unsigned char     act_high     */    , 1 
   /* unsigned char     hsync_len    */    , 64
   /* unsigned char     left_margin  */    , 1       
   /* unsigned char     right_margin */    , 39       
   /* unsigned char     vsync_len    */    , 20       
   /* unsigned char     upper_margin */    , 8       
   /* unsigned char     lower_margin */    , 3       
   /* unsigned char     active       */    , 1
   /* unsigned char     crt          */    , 0 }
// Note that you can use the nifty tool at the 
// following location to generate these values:
//    http://www.tkk.fi/Misc/Electronics/faq/vga2rgb/calc.html
, {
    name: "crt1024x768",
    pixclock: 65000000,
    xres: 1024,
    yres: 768,
    act_high : 0,
    hsync_len: 136,
    left_margin: 24,
    right_margin: 160,
    vsync_len: 6,
    upper_margin: 3,
    lower_margin: 29,
    active : 0,
    crt : 1
}
, {
    name: "hitachi_wxga",
    pixclock: 96000000,
    xres: 1280,
    yres: 800,
    act_high : 1,
    hsync_len: 64,
    left_margin: 1,
    right_margin: 39,
    vsync_len: 20,
    upper_margin: 8,
    lower_margin: 3,
    active : 1,
    crt : 0
}
, {
    name: "lcd_sxga",
    pixclock: 65000000,
    xres: 1024,
    yres: 768,
    act_high : 1,
    hsync_len: 64,
    left_margin: 1,
    right_margin: 39,
    vsync_len: 20,
    upper_margin: 8,
    lower_margin: 3,
    active : 1,
    crt : 0
}
};

/*
. e
typedef enum _polarity_t
{
        POSITIVE,
        NEGATIVE,
}
polarity_t;

typedef struct _mode_table_t
{
        // Horizontal timing.
        int horizontal_total;
        int horizontal_display_end;
        int horizontal_sync_start;
        int horizontal_sync_width;
        polarity_t horizontal_sync_polarity;

        // Vertical timing.
        int vertical_total;
        int vertical_display_end;
        int vertical_sync_start;
        int vertical_sync_height;
        polarity_t vertical_sync_polarity;

        // Refresh timing.
        long pixel_clock;
        long horizontal_frequency;
        long vertical_frequency;
}
mode_table_t;

        // 1024 x 768
 htotal dend hsstrt hsw  hpolar    vtot vdend vdstrt vsh vpolar    pixclk    hfreq vfreq
{ 1344, 1024, 1048, 136, NEGATIVE, 806, 768,    771,  6, NEGATIVE, 65000000, 48363, 60 },
{ 1328, 1024, 1048, 136, NEGATIVE, 806, 768,    771,  6, NEGATIVE, 75000000, 56476, 70 },
{ 1312, 1024, 1040,  96, POSITIVE, 800, 768,    769,  3, POSITIVE, 78750000, 60023, 75 },
{ 1376, 1024, 1072,  96, POSITIVE, 808, 768,    769,  3, POSITIVE, 94500000, 68677, 85 },

0FE80200/00010000 +           CRT regs
0FE80204/00180000 +
0FE80208/08000800 +
0FE8020C/05D003FF +
0FE80210/00C80424 +
0FE80214/032502FF +
0FE80218/00060302 +
0FE8021C/00000000 +
0FE80220/00000000 +
0FE80224/00400200 +
0FE80228/00000000 +
0FE8022C/00000000 +
0FE80230/00000800 +
0FE80234/00000000 +
0FE80238/08000000 +
0FE8023C/00000400 +
*/

struct lcd_panel_info_t const * const lcd_panels = lcd_panels_ ;
unsigned const num_lcd_panels = sizeof(lcd_panels_)/sizeof(lcd_panels_[0]);

#define FASTCLOCK1 0x291A0201    //faster pixel clock:     P2S = 1, P2 =  9  (/6)      ( panel source 1, divide by 6)
                                 //                        V2S = 1, V2 = 10 (/12)      ( crt source 1, divide by 12)
                                 //                        M2S = 0, MR =  2  (/4)      (sdram source 1, divide by 4)
                                 //                        M1S = 0, MR =  1  (/2)
                                 // miscTimReg[5:4] == 0 (336 MHz)
                                 //                   / 6 == 56 MHz
                                 //
#define FASTCLOCK2 0x291A0201
#define FASTCLOCK3 0x00080800

#define SLOWCLOCK1 0x0A1A0201        //slow pixel clock:       P2S = 0, P2 = 10 (/12)      ( panel source 0, divide by 12)  
                                     //                        V2S = 1, V2 = 10 (/12)      ( crt source 1, divide by 12)   
                                     //                        M2S = 0, MR =  2  (/4)      (sdram source 1, divide by 4)   
                                     //                        M1S = 0, MR =  1  (/2)                                      
                                     // miscTimReg[5:4] == 0 (288 MHz)                                                     
                                     //                   / 12 == 24 MHz                                                    
                                     //
#define SLOWCLOCK2 0x0A1A0A09
#define SLOWCLOCK3 0x00090900

static unsigned long const clockRegs[] = {
   SLOWCLOCK1, SLOWCLOCK2,
   FASTCLOCK1, FASTCLOCK2
};

static unsigned const numClockRegs = sizeof(clockRegs)/sizeof(clockRegs[0])/2 ;

/*
 * The following tables were built by screen-scraping and sorting
 * the tables in the SM501 manual:
 *
 * Fields are:
 *    frequency in Hz
 *    clock source (0 == 288MHz, 1 == 366 MHz)
 *    select bits (5 bits for panels, 4 bits for CRTs)
 */ 
#define ENTRIESPERFREQ 3
unsigned long const panelFrequencies[] = {
      450000/2, 0<<29, 0x17<<24, 
      525000/2, 1<<29, 0x17<<24, 
      750000/2, 0<<29, 0x0f<<24, 
      875000/2, 1<<29, 0x0f<<24, 
      900000/2, 0<<29, 0x16<<24, 
     1050000/2, 1<<29, 0x16<<24, 
     1500000/2, 0<<29, 0x0e<<24, 
     1750000/2, 1<<29, 0x0e<<24, 
     1800000/2, 0<<29, 0x15<<24, 
     2100000/2, 1<<29, 0x15<<24, 
     2250000/2, 0<<29, 0x07<<24, 
     2625000/2, 1<<29, 0x07<<24, 
     3000000/2, 0<<29, 0x0d<<24, 
     3500000/2, 1<<29, 0x0d<<24, 
     3600000/2, 0<<29, 0x14<<24, 
     4200000/2, 1<<29, 0x14<<24, 
     4500000/2, 0<<29, 0x06<<24, 
     5250000/2, 1<<29, 0x06<<24, 
     6000000/2, 0<<29, 0x0c<<24, 
     7000000/2, 1<<29, 0x0c<<24, 
     7200000/2, 0<<29, 0x13<<24, 
     8400000/2, 1<<29, 0x13<<24, 
     9000000/2, 0<<29, 0x05<<24, 
    10500000/2, 1<<29, 0x05<<24, 
    12000000/2, 0<<29, 0x0b<<24, 
    14000000/2, 1<<29, 0x0b<<24, 
    14400000/2, 0<<29, 0x12<<24, 
    16800000/2, 1<<29, 0x12<<24, 
    18000000/2, 0<<29, 0x04<<24, 
    21000000/2, 1<<29, 0x04<<24, 
    24000000/2, 0<<29, 0x0a<<24, 
    28000000/2, 1<<29, 0x0a<<24, 
    28800000/2, 0<<29, 0x11<<24, 
    33600000/2, 1<<29, 0x11<<24, 
    36000000/2, 0<<29, 0x03<<24, 
    42000000/2, 1<<29, 0x03<<24, 
    48000000/2, 0<<29, 0x09<<24, 
    56000000/2, 1<<29, 0x09<<24, 
    57600000/2, 0<<29, 0x10<<24, 
    67200000/2, 1<<29, 0x10<<24, 
    72000000/2, 0<<29, 0x02<<24, 
    84000000/2, 1<<29, 0x02<<24, 
    96000000/2, 0<<29, 0x08<<24, 
   112000000/2, 1<<29, 0x08<<24, 
   144000000/2, 0<<29, 0x01<<24, 
   168000000/2, 1<<29, 0x01<<24, 
   288000000/2, 0<<29, 0x00<<24, 
   336000000/2, 1<<29, 0x00<<24 
};
#define numPanelFrequencies (sizeof(panelFrequencies)/sizeof(panelFrequencies[0])/ENTRIESPERFREQ)

unsigned long const crtFrequencies[] = {
      750000/2, 0<<20, 0x0f<<16, 
      875000/2, 1<<20, 0x0f<<16, 
     1500000/2, 0<<20, 0x0e<<16, 
     1750000/2, 1<<20, 0x0e<<16, 
     2250000/2, 0<<20, 0x07<<16, 
     2625000/2, 1<<20, 0x07<<16, 
     3000000/2, 0<<20, 0x0d<<16, 
     3500000/2, 1<<20, 0x0d<<16, 
     4500000/2, 0<<20, 0x06<<16, 
     5250000/2, 1<<20, 0x06<<16, 
     6000000/2, 0<<20, 0x0c<<16, 
     7000000/2, 1<<20, 0x0c<<16, 
     9000000/2, 0<<20, 0x05<<16, 
    10500000/2, 1<<20, 0x05<<16, 
    12000000/2, 0<<20, 0x0b<<16, 
    14000000/2, 1<<20, 0x0b<<16, 
    18000000/2, 0<<20, 0x04<<16, 
    21000000/2, 1<<20, 0x04<<16, 
    24000000/2, 0<<20, 0x0a<<16, 
    28000000/2, 1<<20, 0x0a<<16, 
    36000000/2, 0<<20, 0x03<<16, 
    42000000/2, 1<<20, 0x03<<16, 
    48000000/2, 0<<20, 0x09<<16, 
    56000000/2, 1<<20, 0x09<<16, 
    72000000/2, 0<<20, 0x02<<16, 
    84000000/2, 1<<20, 0x02<<16, 
    96000000/2, 0<<20, 0x08<<16, 
   112000000/2, 1<<20, 0x08<<16, 
   144000000/2, 0<<20, 0x01<<16, 
   168000000/2, 1<<20, 0x01<<16, 
   288000000/2, 0<<20, 0x00<<16, 
   336000000/2, 1<<20, 0x00<<16
};
#define numCrtFrequencies (sizeof(crtFrequencies)/sizeof(crtFrequencies[0])/ENTRIESPERFREQ)

unsigned long const * const frequencies[] = {
   panelFrequencies,
   crtFrequencies
};

unsigned const numFrequencies[] = {
   numPanelFrequencies,
   numCrtFrequencies
};

static unsigned long const clockMasks[] = {
   0x3F<<24,
   0x1F<<16
};

void get_panel_reg( struct lcd_panel_info_t const *panel,
                    unsigned long                 *clockRegs )
{
   int const isCRT = (0 != panel->crt);
   if( panel->pixclock < numClockRegs )
   {
      unsigned long const *clk = clockRegs+(panel->pixclock*2);
      *clockRegs++ = *clk++ ;
      *clockRegs   = *clk ;
   }
   else
   {
      unsigned long reg ;
      unsigned long const *freq = frequencies[isCRT];
      unsigned const count = numFrequencies[isCRT];
      
      unsigned long f, diffl, diffh ;
      int i ;
      unsigned long low, high ;

      //
      // linear scan for closest frequency
      //
      for( i = 0 ; i < count ; i++, freq += ENTRIESPERFREQ )
      {
         if( *freq > panel->pixclock )
            break;
      }
      
      low  = (i > 0) 
             ? freq[0-ENTRIESPERFREQ]
             : 0 ;
      diffl = panel->pixclock - low ;

      high = (i < count ) 
             ? *freq
             : 0xFFFFFFFF ;
      diffh = high - panel->pixclock ;

      if( diffh < diffl )
      {
         f = high ;
      }
      else
      {
         f = low ;
         freq -= ENTRIESPERFREQ ;
      }

      printf( "pixclock == %lu, frequency %u/%u -> %lu\n",
              panel->pixclock, low, high, f );
      
      // Clock source
      reg |= freq[1];
      reg |= freq[2];
      printf( "freq %lu, source %X, divisor %X, reg %X\n", freq[0], freq[1], freq[2], reg );

      clockRegs[0] = clockRegs[1] = reg ;
   }
}

int main( int argc, char const * const argv[] )
{
   for( unsigned i = 0 ; i < num_lcd_panels ; i++ )
   {
      lcd_panel_info_t const &panel = lcd_panels[i];
      printf( "%s\n", panel.name );
      printf( "   %ux%u pixels\n", panel.xres, panel.yres );
      printf( "   %s\n", panel.crt ? "CRT" : "LCD" );
      printf( "   hmargins %u, %u\n", panel.left_margin, panel.right_margin );
      printf( "   hsync len %u\n", panel.hsync_len );
      printf( "   vmargins %u, %u\n", panel.upper_margin, panel.lower_margin );
      printf( "   vsync len %u\n", panel.vsync_len );
      printf( "   pixclock: %lu\n", panel.pixclock );
      unsigned long clockRegs[2] = { 0, 0 };
      get_panel_reg( &panel, clockRegs );
      printf( "   clockRegs: 0x%08lX, 0x%08lX\n", clockRegs[0], clockRegs[1] );
   }
   
   printf( "LCD frequencies:\n" );
   unsigned long const *freq = frequencies[0];
   for( unsigned i = 0 ; i < numPanelFrequencies ; i++, freq += ENTRIESPERFREQ )
   {
      printf( "%10lu  ", *freq );
      if( 3 == ( i & 3 ) )
         printf( "\n" ); 
   }
   printf( "\n" );
   
   return 0 ;
}
