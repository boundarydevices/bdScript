#include <syscall.h>
#include <unistd.h>
int errno ;

_syscall1( int,exit, int, exitStat );

extern int main __P ((int argc, char **argv, char **envp));

/* N.B.: It is important that this be the first function.
   This file is the first thing in the text section.  */

/* If this was in C it might create its own stack frame and
   screw up the arguments.  */
asm (".text; .globl _start; _start: B start1");

/* Make an alias called `start' (no leading underscore, so it can't
   conflict with C symbols) for `_start'.  This is the name vendor crt0.o's
   tend to use, and thus the name most linkers expect.  */
asm (".set start, __start");

/* Fool gcc into thinking that more args are passed.  This makes it look
   on the stack (correctly) for the real arguments.  It causes somewhat
   strange register usage in start1(), but we aren't too bothered about
   that at the moment. */
#define DUMMIES a1, a2, a3, a4

#ifdef	DUMMIES
#define	ARG_DUMMIES	DUMMIES,
#define	DECL_DUMMIES	int DUMMIES;
#else
#define	ARG_DUMMIES
#define	DECL_DUMMIES
#endif

/* ARGSUSED */
static void
start1( int a1,
        int a2,
        int a3,
        int a4,
        int                            argc, 
        char *args,
        int a5,
        int a6,
        int a7 )
{
   char **argv = &args ;
   char **envp = argv+argc+1 ;
//   argv = ((char **)&argc )+1 ;
//   envp = ((char **)&argc )+2 ;
   exit(main( argc, argv, envp ));
}
