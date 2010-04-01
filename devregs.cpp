/*
 * devregs - tool to display and modify a device's registers at runtime
 *
 * Use cases:
 *
 *	devregs
 *		- display all registers
 *
 *	devregs register
 *		- display all registers matching register (strcasestr)
 *
 *	devregs register:field
 *		- display all registers matching register (strcasestr)
 *		- also break out specified field
 *
 *	devregs register value
 *		- set register to specified value (must match single register)
 *
 *	devregs register:field value
 *		- set register field to specified value (read/modify/write)
 *
 * Registers may be specified by name or 0xADDRESS. If specified by name, all
 * registers containing the pattern are considered. If multiple registers 
 * match on a write request (2-parameter use cases), no write will be made.
 *
 * fields may be specified by name or bit numbers of the form "start[-end]"
 *
 * (c) Copyright 2002 by M&N Logistik-Lösungen Online GmbH
 * set under the GPLv2
 *
 * $Id: pxaregs.c,v 1.7 2009-01-28 20:27:55 valli Exp $
 *
 * Please send patches to h.schurig, working at mn-logistik.de
 * - added fix from Bernhard Nemec
 * - i2c registers from Stefan Eletzhofer
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

static bool word_access = false ;

struct fieldDescription_t {
	char const 		   *name ;
	unsigned    		   startbit ;
	unsigned    		   bitcount ;
	struct fieldDescription_t *next ;
};

struct registerDescription_t {
	char const 		*name ;
	fieldDescription_t 	*fields ;
};

struct reglist_t {
	unsigned long 			 address ;
	struct registerDescription_t	*reg ;
	struct fieldDescription_t	*fields ;
	struct reglist_t		*next ;
};

static char const devregsPath[] = {
	".devregs"
};

char *skipSpaces(char *buf){
	while( *buf ){
		if( isprint(*buf) )
			break;
		buf++ ;
	}
	return buf ;
}

static void trimCtrl(char *buf){
	char *tail = buf+strlen(buf);
	// trim trailing <CR> if needed
	while( tail > buf ){
		--tail ;
		if( iscntrl(*tail) ){
			*tail = '\0' ;
		} else
			break;
	}
}

static struct reglist_t const *registerDefs(void){
	static struct reglist_t *regs = 0 ;
	if( 0 == regs ){
		struct reglist_t *head = 0, *tail = 0 ;
		FILE *fDefs = fopen(devregsPath, "rt");
		if( fDefs ){
			char inBuf[256];
			int lineNum = 0 ;
			while( fgets(inBuf,sizeof(inBuf),fDefs) ){
				lineNum++ ;
				// skip unprintables
                                char *next = skipSpaces(inBuf);
				if( *next && ('#' != *next) ){
					trimCtrl(next);
					printf( "<%s>\n", next );
				} // not blank or comment
				if(isalpha(*next)){
					char *start = next++ ;
					while(isalnum(*next) || ('_' == *next)){
						next++ ;
					}
					if(isspace(*next)){
						char *end=next-1 ;
						next=skipSpaces(next);
						if(isxdigit(*next)){
							char *addrEnd ;
							unsigned long addr = strtoul(next,&addrEnd,16);
							if('\0'==*addrEnd){
								unsigned namelen = end-start ;
								char *name = (char *)malloc(namelen+1);
								memcpy(name,start,namelen);
								name[namelen] = '\0' ;
                                                                struct reglist_t *newone = new reglist_t ;
								newone->address=addr ;
								newone->reg = new registerDescription_t ;
								newone->reg->name = name ;
								newone->reg->fields = newone->fields = 0 ;
								if(tail){
									tail->next = newone ;
								} else
									head = newone ;
								tail = newone ;
								continue;
							}
						}
					}
					fprintf(stderr, "%s: syntax error on line %u\n", devregsPath, lineNum );
				} else if(tail){
				}
			}
			fclose(fDefs);
			regs = head ;
		}
		else
			perror(devregsPath);
	}
	return regs ;
}

static struct fieldDescription_t *parseFields
	( struct reglist_t const *regs,
	  char const *fieldname )
{
	if(isdigit(*fieldname)){
		char *end ;
		unsigned long startbit = strtoul(fieldname,&end,0);
		if( (31 >= startbit)
		    &&
		    ( ('\0' == *end)
		      ||
                      ('-' == *end) ) ){
			unsigned long endbit ;
			if( '-' == *end ){
				endbit = strtoul(end+1,&end,0);
				if('\0' != *end){
					endbit = ~startbit ;
				}
			} else {
				endbit = startbit ;
			}
			unsigned const bitcount = endbit-startbit+1 ;
			if( bitcount <= (32-startbit) ){
                                fieldDescription_t *f = new fieldDescription_t ;
				f->name = fieldname ;
				f->startbit = startbit ;
				f->bitcount = bitcount ;
				f->next = 0 ;
				return f ;
			} else
				fprintf(stderr, "Invalid field '%s'. Use form 'start-end' in decimal (%lu,%lu,%u)\n", fieldname,startbit,endbit,bitcount );
		} else
			fprintf(stderr, "Invalid field '%s'. Use form 'start-end' in decimal (%lu,%x)\n", fieldname,startbit,*end );
	} else if( regs ){
                struct fieldDescription_t *head = 0 ;
		struct fieldDescription_t *tail = 0 ;
		while( regs ){
			if( regs->reg ){
				struct fieldDescription_t *f = regs->reg->fields ;
				while( f ){
					if( 0 != strcasestr(f->name,fieldname) ){
						struct fieldDescription_t *newf = new struct fieldDescription_t ;
						*newf = *f ;
						newf->next = 0 ;
						tail = newf ;
						if( 0 == head )
							head = newf ;
					}
					f = f->next ;
				}
			}
			regs = regs->next ;
		}
		return head ;
	} else {
		fprintf(stderr, "Can't parse named fields without matching registers\n" );
	}
	return 0 ;
}

static struct reglist_t const *parseRegisterSpec(char const *regname)
{
	char const c = *regname ;

	if(isalpha(c) || ('_' == c)){
		fprintf(stderr, "No support for named registers (yet)\n" );
	} else if(isdigit(c)){
		char *end ;
		unsigned long address = strtoul(regname,&end,16);
		if( (0 == *end) || (':' == *end) ){
			struct fieldDescription_t *fields = 0 ;
			if( ':' == *end ){
				fields = parseFields(0,end+1);
				if( !fields )
					return 0 ;
			}
			struct reglist_t *out = new struct reglist_t ;
			out->address = address ;
			out->reg = 0 ;
			out->fields = fields ;
			out->next = 0 ;
			return out ;
		} else {
			fprintf( stderr, "Invalid register name or value '%s'. Use name or 0xHEX\n", regname );
		}
	} else {
		fprintf( stderr, "Invalid register name or value '%s'. Use name or 0xHEX\n", regname );
	}
	return 0 ;
}

static int getFd(void){
	static int fd = -1 ;
	if( 0 > fd ){
		fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (fd<0) {
			perror("/dev/mem");
			exit(1);
		}
	}
	return fd ;
}

#define MAP_SIZE 4096
#define MAP_MASK ( MAP_SIZE - 1 )

static unsigned long volatile *getReg(unsigned long addr){
	static void *map = 0 ;
	static unsigned prevPage = -1U ;
	unsigned page = addr & ~MAP_MASK ;
	if( page != prevPage ){
		if( map ){
		   munmap(map,MAP_SIZE);
		}
		map = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, getFd(), page );
		if( MAP_FAILED == map ){
			perror("mmap");
			exit(1);
		}
		prevPage = page ;
	}
	unsigned offs = addr & MAP_MASK ;
	return (unsigned long volatile *)((char *)map+offs);
}

static unsigned fieldVal(struct fieldDescription_t *f, unsigned long v)
{
	v >>= f->startbit ;
	v &= (1<<f->bitcount)-1 ;
	return v ;
}

static void showReg(struct reglist_t const *reg)
{
	unsigned long rv ; 
	if( word_access ) {
		unsigned short volatile *p = (unsigned short volatile *)getReg(reg->address);
		rv = *p ;
	} else {
		unsigned long volatile *p = getReg(reg->address);
		rv = *p ;
	}
	printf( "%s:0x%08lx == 0x%08lx\n", reg->reg ? reg->reg->name : "", reg->address, rv );
	struct fieldDescription_t *f = reg->fields ;
	while(f){
		printf( "   %s: %u.%u == 0x%x\n", f->name, f->startbit, f->bitcount, fieldVal(f,rv) );
		f=f->next ;
	}
}

static void putReg(struct reglist_t const *reg,unsigned long value){
	printf( "store value 0x%08lx here\n", value );
	if( word_access ){
		unsigned short volatile * const rv = (unsigned short volatile *)getReg(reg->address);
		printf( "%s:0x%08lx == 0x%08lx...", reg->reg ? reg->reg->name : "", reg->address, *rv );
		*rv = value ;
	} else {
		unsigned long volatile * const rv = getReg(reg->address);
		printf( "%s:0x%08lx == 0x%08lx...", reg->reg ? reg->reg->name : "", reg->address, *rv );
		*rv = value ;
	}
	printf( "0x%08x\n", value );
}

static void parseArgs( int &argc, char const **argv )
{
	for( unsigned arg = 1 ; arg < argc ; arg++ ){
		if( '-' == *argv[arg] ){
			char const *param = argv[arg]+1 ;
			if( 'w' == tolower(*param) ){
            			word_access = true ;
			}
			else
				printf( "unknown option %s\n", param );

			// pull from argument list
			for( int j = arg+1 ; j < argc ; j++ ){
				argv[j-1] = argv[j];
			}
			--arg ;
			--argc ;
		}
	}
}

int main(int argc, char const **argv)
{
	parseArgs(argc,argv);
	if( 1 == argc ){
		printf( "display all known registers here\n" );
                struct reglist_t const *defs = registerDefs();
		while(defs){
			printf("%s: 0x%08lx\n", defs->reg->name, defs->address );
                        showReg(defs);
			defs = defs->next ;
		}
	} else {
                struct reglist_t const *regs = parseRegisterSpec(argv[1]);
		if( regs ){
			if( 2 == argc ){
				while( regs ){
					showReg(regs);
					regs = regs->next ;
				}
			} else {
				char *end ;
				unsigned long value = strtoul(argv[2],&end,16);
				if( '\0' == *end ){
					while( regs ){
						showReg(regs);
						putReg(regs,value);
						regs = regs->next ;
					}
				} else 
					fprintf( stderr, "Invalid value '%s', use hex\n", argv[2] );
			}
		}
	}
	return 1;
}
