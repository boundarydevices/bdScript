#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

int main( void )
{
   int uid = getuid();
	int euid = geteuid();

   printf( "uid == %d, euid == %d\n", uid, euid ); 

   struct passwd *pw = getpwuid(uid);
	if(pw)
      printf( "have pw\n" );
   else
      printf( "no pw: %m\n" );
      
   return 0 ;
}
