var dump = use( { url:"../sampleScripts/dump.js" } );
var rhFont = font( { url: "../sampleScripts/fonts/Vera.ttf" } );
var rhAlpha = rhFont.render( 10, "Hello, world" );
var images = new Array(
               image( {url:"../sampleScripts/images/Boundary.png"} )
               ,image( {url:"../sampleScripts/images/printer.png"} )
               ,image( {url:"../sampleScripts/images/scrib.png"} )
               ,image( {url:"../sampleScripts/images/tm.png"} )
               ,image( {url:"../sampleScripts/images/rightEyeOpen.jpg"} )
               ,image( {url:"../sampleScripts/images/slash.png"} )
             );
var idx = 0 ;
   
   
function ttyExec()
{
   var s ;
   do {
      images[idx++].draw( 0, 0 );
      idx = idx % images.length ;
      s = tty.readln();
   } while( s );
}

/*
images[idx++].draw( 0, 0 );
for( var x = 0 ; x < screen.width ; x++ )
{
   screen.setPixel( x, x%screen.height, 0 );   
}

for( var x = 0 ; x < screen.height ; x += 2 )
{
   screen.box( 0, 0, x, x );
}
*/
rhAlpha.draw( 0, 0, 0 );

tty.onLineIn( "ttyExec()" );

