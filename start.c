#include <syscall.h>
#include <unistd.h>
int errno ;

_syscall1( int,exit, int, exitStat );

extern int main( void );

void _start( void )
{
   main();
   exit(0);
}
