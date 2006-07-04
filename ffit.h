#ifndef _FFIT_H_
#define _FFIT_H_

// By default this implementation uses a First-Fit strategy, nonetheless if 
// the BESTFIT macro is set then a Best-Fit strategy is used to find a
// free block

#ifdef __cplusplus
extern "C" {
#endif

#define BESTFIT
#define SANITY_CHECK

void init_memory_pool (unsigned int size, char *ptr);
void *rtl_malloc(unsigned int size);
void rtl_free(void *ptr);
void destroy_memory_pool(void);

// Next function just can be used if the SANITY_CHECK macro has been defined
#ifdef SANITY_CHECK
void print_list (void);
void dump_memory_region (unsigned char *mem_ptr, unsigned int size);
void print_phys_list (void);
#endif

#ifdef __cplusplus
}; // extern "C" 
#endif

#endif
