#include <syscall.h>
#include <unistd.h>
extern int errno ;

_syscall3( int,reboot, int, param1, int,param2, int,param3 );

int main( void )
{
   reboot( (int) 0xfee1dead, 672274793, 0x89abcdef ); // allows <Ctrl-Alt-Del>
   reboot( (int) 0xfee1dead, 672274793, 0x1234567 ); // doesn't
   return 0;
}
