#ifndef __BOARDTYPE_H__
#define __BOARDTYPE_H__

#include "config.h"

#if (CONFIG_HALOGEN==1)
   #define BOARDTYPE "Halogen board"
#elif (CONFIG_ARGON==1)
   #define BOARDTYPE "Argon board"
#elif (CONFIG_NEON270==1)
   #define BOARDTYPE "Neon270 board"
#elif (CONFIG_XENON==1)
   #define BOARDTYPE "Xenon board"
#elif (KERNEL_MACH_HYDROGEN==1)
   #define BOARDTYPE "Hydrogen board"
#else
   #define BOARDTYPE "unknown board"
#endif

#endif
