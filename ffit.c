/*
 * First-Fit / Best-Fit
 * Version 0.2
 *
 * Written by Miguel Masmano Tello <mmasmano@disca.upv.es>
 * Best-Fit strategy implemented by Ismael Ripoll <iripoll@disca.upv.es>
 *
 * Thanks to Ismael Ripoll for his suggestions and reviews
 *
 * Copyright (C) April 2004
 *
 * This code is released using a dual license strategy: GPL/LGPL
 * You can choose the license that better fits your requirements.
 *
 * Released under the terms of the GNU General Public License Version 2.0
 * Released under the terms of the GNU Lesser General Public License Version 2.1
 *
 */
		

#include "ffit.h"

#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////
// Once SANITY_CHECK is enabled, following functions can be used: memory_dump,
// show_structure, and so on.
#define SANITY_CHECK
//////////////////////////////////////////////////////////////////////////////


// Some trivial definitions like NULL and printf
#ifndef NULL
#define NULL ((void *)0)
#endif

extern int printf (char *str, ...);

#ifdef SANITY_CHECK
#define MN 0xABCDEF01
#endif

struct free_ptr {
  struct head_struct *next, *prev;
};

#define USED_BLOCK 0x80000000
#define FREE_BLOCK ~USED_BLOCK //0x7FFFFFFF

#define LAST_BLOCK 0x40000000
#define NOT_LAST_BLOCK ~LAST_BLOCK //0xBFFFFFFF

#define IS_USED_BLOCK(x) ((x -> size & USED_BLOCK) == USED_BLOCK)
#define IS_LAST_BLOCK(x) ((x -> size & LAST_BLOCK) == LAST_BLOCK)
#define GET_BLOCK_SIZE(x) (x -> size & FREE_BLOCK & NOT_LAST_BLOCK)
#define SET_USED_BLOCK(x) (x -> size |= USED_BLOCK)
#define SET_FREE_BLOCK(x) (x -> size &= FREE_BLOCK)
#define SET_LAST_BLOCK(x) (x -> size |= LAST_BLOCK)
#define SET_NOT_LAST_BLOCK(x) (x -> size &= NOT_LAST_BLOCK)

typedef struct head_struct {
#ifdef SANITY_CHECK
  unsigned int mn;
#endif
  
  unsigned int size;
  //unsigned int free; 
  //struct head_struct *prev_phys, *next_phys;
  
  struct head_struct *prev_phys;
  union mem {
    struct free_ptr free_ptr;
    unsigned char ptr[sizeof (struct free_ptr)];
  } mem;
} list_t;

typedef struct list_header_struct {
  list_t *head;
#ifdef SANITY_CHECK
  int blocks;
  int free_blocks;
  list_t *first_block;
#endif
} list_header_t;

static list_header_t list;
static unsigned int HEADER_SIZE;
static unsigned int MIN_SIZE = 8;

#define PRINTF printf

#ifdef SANITY_CHECK

#define PRINT_MSG printf
#define PRINT_DBG_C(message) printf(message)
#define PRINT_DBG_D(message) printf("%i", message);
#define PRINT_DBG_F(message) printf("%f", message);
#define PRINT_DBG_H(message) printf("%x", (unsigned int) message);

void dump_memory_region (unsigned char *mem_ptr, unsigned int size) {

  unsigned int begin = (unsigned int) mem_ptr;
  unsigned int end = (unsigned int) mem_ptr + size;
  int column = 0;

  begin >>= 2;
  begin <<= 2;

  end >>= 2;
  end ++;
  end <<= 2;

  PRINT_DBG_C ("\nMemory region dumped: 0x");
  PRINT_DBG_H (begin);
  PRINT_DBG_C (" - ");
  PRINT_DBG_H (end);
  PRINT_DBG_C ("\n\n");

  column = 0;
  PRINT_DBG_C ("\t\t+0");
  PRINT_DBG_C ("\t+1");
  PRINT_DBG_C ("\t+2");
  PRINT_DBG_C ("\t+3");
  PRINT_DBG_C ("\t+4");
  PRINT_DBG_C ("\t+5");
  PRINT_DBG_C ("\n0x");
  PRINT_DBG_H (begin);
  PRINT_DBG_C ("\t");

  while (begin < end) {
    if (((unsigned char *) begin) [0] == 0)
      PRINT_DBG_C ("00");
    else
      PRINT_DBG_H (((unsigned char *) begin) [0]);
    if (((unsigned char *) begin) [1] == 0)
      PRINT_DBG_C ("00");
    else
      PRINT_DBG_H (((unsigned char *) begin) [1]);
    PRINT_DBG_C ("\t");
    begin += 2;
    column ++;
    if (column == 6) {
      PRINT_DBG_C ("\n0x");
      PRINT_DBG_H (begin);
      PRINT_DBG_C ("\t");
      column = 0;
    }
    
  }
  PRINT_DBG_C ("\n\n");
}

extern void print_phys_list (void);
extern void print_list (void);
#define check_list(x)
/*
void check_list (char *str) {
  list_t *aux;
  int n = 0;
  
  aux = list.head;
  while (aux) {
    if (n == 0) {
      if (aux -> mem.free_ptr.prev) {
	PRINTF ("Error prev must be 0: %s\n", str);
	//print_list();
	exit (-1);
      }
    }
    if (n >= list.free_blocks) {
      if (aux -> mem.free_ptr.prev) {
	PRINTF ("Error n(%d) blocks is greater than allowed %s\n", n, str);
	exit (-1);
      }
    }
    
    if (aux -> mem.free_ptr.next)
      if (aux -> mem.free_ptr.next->mem.free_ptr.prev != aux) {
	PRINTF("ERROR free_prev not points to aux %s\n", str);
	exit (-1);
      }
    aux = aux->mem.free_ptr.next;
    n++;
  }
  
  aux = list.first_block;
  n = 0;
  while (aux) {
    if (n == 0) {
      if (aux -> prev_phys != NULL) {
	PRINTF ("Error prev must be 0 2: %s\n", str);
	exit (-1);
      }
    }
    if (n > list.blocks) {
      if (aux -> next_phys != NULL) {
	PRINTF ("Error n (%d) blocks is greater than allowed 2 %s (%d)\n", n, 
		str, list.blocks);
	exit (-1);
      }
    }
    if (aux -> next_phys && aux >= aux -> next_phys) {
      PRINTF ("ERROR next_phys must be greater than this one %s\n", str);
      print_phys_list ();
      exit (-1);
    }
    if (aux -> next_phys)
      if (aux -> next_phys-> prev_phys != aux) {
	PRINTF("ERROR phys_prev not points to aux %s\n", str);
	exit (-1);
      }
    aux = aux -> next_phys;
    n++;
  }
}
*/
void print_list (void) {
  list_t *b;
  PRINTF ("\nPRINTING FREE LIST\n\n");
  b = list.head;
  while (b) {
    PRINTF ("<0x%x> MN: %x size: %d used: %d\n", (int)b, b -> mn, 
	    (int) GET_BLOCK_SIZE (b), IS_USED_BLOCK(b));
    PRINTF ("\tprev_free: 0x%x\n", (int) b -> mem.free_ptr.prev);
    PRINTF ("\tnext_free: 0x%x\n", (int) b -> mem.free_ptr.next);
    PRINTF ("\tprev_phys: 0x%x\n", (int)b -> prev_phys);
    if (!IS_LAST_BLOCK (b))
      PRINTF ("\tnext_phys: 0x%x\n",  (int) b + GET_BLOCK_SIZE (b) 
	      + HEADER_SIZE);
    else {
      PRINTF ("\tnext_phys: 0x0\n");
      break;
    }
    b = b -> mem.free_ptr.next;
  }
}

void print_phys_list (void) {
  list_t *b;
  PRINTF ("\nPRINTING PHYS. LIST\n\n");
  b = list.first_block;
  while (1) {
    PRINTF ("<0x%x> MN: %x size: %d used: %d\n", (int)b, b -> mn, 
	    GET_BLOCK_SIZE(b), IS_USED_BLOCK(b));
    PRINTF ("\tprev_phys: 0x%x\n", (int)b -> prev_phys);
    if (!IS_LAST_BLOCK (b))
      PRINTF ("\tnext_phys: 0x%x\n",  (int) b + GET_BLOCK_SIZE (b) 
	      + HEADER_SIZE);
    else {
      PRINTF ("\tnext_phys: 0x0\n");
      break;
    }
    b = (list_t *) (((char *) b) + (unsigned long) HEADER_SIZE + 
		      (unsigned long) GET_BLOCK_SIZE(b));

  }
}

#endif

void init_memory_pool (unsigned int size, char *ptr) {
  list.head = (list_t *) ptr;
#ifdef SANITY_CHECK
  list.first_block = list.head;
  list.head -> mn = MN;
#endif

  HEADER_SIZE = ((unsigned int)&list.head -> mem.ptr[0]) - 
    (unsigned int) list.head;
  size = size & (~0x3);
  if (HEADER_SIZE > MIN_SIZE) MIN_SIZE = HEADER_SIZE;
  
  list.head -> size = size - HEADER_SIZE;
  SET_FREE_BLOCK (list.head);
  SET_LAST_BLOCK (list.head);
  list.head -> mem.free_ptr.next = NULL;
  list.head -> mem.free_ptr.prev = NULL;
  list.head -> prev_phys = NULL;
#ifdef SANITY_CHECK
  list.blocks = 1;
  list.free_blocks = 1;
#endif
}


// No matter if it is used
void destroy_memory_pool(){
	
}

void *rtl_malloc (unsigned int size) {
  list_t *aux, *pos = NULL, *new, *bh3;
  unsigned int new_size;

  if (!size) return (void *) NULL;
#ifdef SANITY_CHECK
  check_list ("entrando malloc");
#endif
  if (size < MIN_SIZE) size = MIN_SIZE;
  // Rounding up the requested size
  size = (size & 0x3)? (size & ~(0x3)) + 0x4: size;
  
  aux = list.head;

#ifdef BESTFIT
  for ( ; aux ;  aux = aux -> mem.free_ptr.next){
     if (GET_BLOCK_SIZE (aux) >= size) {
	 if (!pos || (GET_BLOCK_SIZE (pos) > GET_BLOCK_SIZE(aux))){
	  pos = aux;
	 }
	 if (GET_BLOCK_SIZE(pos) == size)
	     break;
     }
  }
#else // FIRST_FIST
  for ( ; aux ;  aux = aux -> mem.free_ptr.next){
      if (GET_BLOCK_SIZE(aux) >= size) {
	  pos = aux;
	  break;
      }
  }
#endif  // BESTFIT

  aux = pos;

  if (!aux) return (void *) NULL;
  
  if (aux -> mem.free_ptr.next)
    aux -> mem.free_ptr.next -> mem.free_ptr.prev = aux -> mem.free_ptr.prev;

  if (aux -> mem.free_ptr.prev)
    aux -> mem.free_ptr.prev -> mem.free_ptr.next = aux -> mem.free_ptr.next;
  
  if (list.head == aux)
    list.head = aux -> mem.free_ptr.next;
  
  SET_USED_BLOCK (aux);
#ifdef SANITY_CHECK
  list.free_blocks --;
#endif
  aux -> mem.free_ptr.next = NULL;
  aux -> mem.free_ptr.prev = NULL;
  
  new_size = GET_BLOCK_SIZE(aux) - size - HEADER_SIZE;
  if (((int) new_size) >= (int)MIN_SIZE) {
#ifdef SANITY_CHECK
    list.blocks ++;
#endif
    new = (list_t *) (((char *) aux) + (unsigned long) HEADER_SIZE + 
		      (unsigned long) size);
    new -> size = new_size;
#ifdef SANITY_CHECK
    new -> mn = MN;
#endif
    new -> mem.free_ptr.prev = NULL;
    new -> mem.free_ptr.next = NULL;

    SET_FREE_BLOCK (new);
#ifdef SANITY_CHECK
    list.free_blocks ++;
#endif

    new -> prev_phys = aux;
    if (IS_LAST_BLOCK(aux)) { 
      SET_LAST_BLOCK (new);
    } else {
      // updating prev_phys pointer
      bh3 = (list_t *)((char *) new + (unsigned long) HEADER_SIZE + 
		       (unsigned long) GET_BLOCK_SIZE(new));
      bh3 -> prev_phys = new;
    }
    aux -> size = size;
    SET_USED_BLOCK (aux);
    // the new block is indexed inside of the list of free blocks
    new -> mem.free_ptr.next = list.head;
    if (list.head)
      list.head -> mem.free_ptr.prev = new;
    list.head = new;

  }
#ifdef SANITY_CHECK
  check_list ("Saliendo malloc");
#endif
  return (void *) &aux -> mem.ptr [0];
}

void rtl_free (void *ptr) {
  list_t *b = (list_t *) ((char *)ptr - HEADER_SIZE), *b2, *b3;
  if (!ptr) {
    PRINTF ("FREE ERROR: ptr cannot be null\n");
    return;
  }

#ifdef SANITY_CHECK
  check_list ("Entrando free");
  if (b -> mn != MN) {
    PRINTF ("ERROR MN 1\n");
    PRINTF ("size ->%d\n", b -> size);
    return;
  }
#endif
  if (!IS_USED_BLOCK(b)) {
    PRINTF ("You are releasing a previously released block\n");
    return;
  }
  SET_FREE_BLOCK (b);
  b -> mem.free_ptr.next = NULL;
  b -> mem.free_ptr.prev = NULL;
  if (b -> prev_phys) {
    b2 = b -> prev_phys;
    if (!IS_USED_BLOCK (b2)) {
#ifdef SANITY_CHECK
      list.blocks --;
#endif
      b2 -> size = GET_BLOCK_SIZE(b2) + GET_BLOCK_SIZE (b) + HEADER_SIZE;
      if (b2 -> mem.free_ptr.next)
	b2 -> mem.free_ptr.next -> mem.free_ptr.prev = b2 -> mem.free_ptr.prev;

      if (b2 -> mem.free_ptr.prev)
	b2 -> mem.free_ptr.prev -> mem.free_ptr.next = b2 -> mem.free_ptr.next;
     
      if (list.head == b2)
	list.head = b2 -> mem.free_ptr.next;
#ifdef SANITY_CHECK
      list.free_blocks --;
#endif
      SET_FREE_BLOCK (b2);
      b2 -> mem.free_ptr.next = NULL;
      b2 -> mem.free_ptr.prev = NULL;
      if (IS_LAST_BLOCK (b)) {
	SET_LAST_BLOCK (b2);
      } else {
	b3 = (list_t *) (((char *) b2) + (unsigned long) HEADER_SIZE + 
			  (unsigned long) GET_BLOCK_SIZE (b2));
	b3 -> prev_phys = b2;
      }
      b = b2;
    }
  }
  if (!IS_LAST_BLOCK (b)) {
    b2 = (list_t *) (((char *) b) + (unsigned long) HEADER_SIZE + 
		     (unsigned long) GET_BLOCK_SIZE (b));
    
    if (!IS_USED_BLOCK (b2)) {
#ifdef SANITY_CHECK
      list.blocks --;
#endif
      b -> size += GET_BLOCK_SIZE(b2) + HEADER_SIZE;
      
      if (b2 -> mem.free_ptr.next)
	b2 -> mem.free_ptr.next -> mem.free_ptr.prev = b2 -> mem.free_ptr.prev;
      
      if (b2 -> mem.free_ptr.prev)
	b2 -> mem.free_ptr.prev -> mem.free_ptr.next = b2 -> mem.free_ptr.next;
      
      if (list.head == b2)
	list.head = b2 -> mem.free_ptr.next;
#ifdef SANITY_CHECK
      list.free_blocks --;
#endif
      b2 -> mem.free_ptr.next = NULL;
      b2 -> mem.free_ptr.prev = NULL;

      if (IS_LAST_BLOCK (b2)) {
	SET_LAST_BLOCK (b);
      } else {
	b3 = (list_t *) (((char *) b) + (unsigned long) HEADER_SIZE + 
			  (unsigned long) GET_BLOCK_SIZE (b));
	b3 -> prev_phys = b;
      }
    }
    b -> mem.free_ptr.next = list.head;
  
    if (list.head)
      list.head -> mem.free_ptr.prev = b;
#ifdef SANITY_CHECK
    list.free_blocks ++;
#endif
    list.head = b;
  }
#ifdef SANITY_CHECK
  check_list ("Saliendo free");
#endif
}
