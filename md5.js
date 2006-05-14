#!/bin/jsExec -
if( 1 <= argc )
{
   if( '@' == argv[0][0] )
   {
      var fileName = argv[0].substr(1);
      print( "md5(",fileName,") == ", md5(readFile(fileName)), "\n" );
   }
   else
      print( "md5(",argv[0],") == ", md5(argv[0]), "\n" );
}
else
   print( "Usage: md5.js text|@filename\n" );

exit();

