#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/*!
 * This structure is used with IOCTL.
 * It defines register, register value, register mask and event number
 */
typedef struct {
	/*!
	 * register number
	 */
	int reg;
	/*!
	 * value of register
	 */
	unsigned int reg_value;
	/*!
	 * mask of bits, only used with PMIC_WRITE_REG
	 */
	unsigned int reg_mask;
} register_info;

/*!
 * @name IOCTL definitions for sc55112 core driver
 */
/*! @{ */
/*! Read a PMIC register */
#define PMIC_READ_REG          _IOWR('P', 0xa0, register_info*)
/*! Write a PMIC register */
#define PMIC_WRITE_REG         _IOWR('P', 0xa1, register_info*)
/*! Subscribe a PMIC interrupt event */
#define PMIC_SUBSCRIBE         _IOR('P', 0xa2, int)
/*! Unsubscribe a PMIC interrupt event */
#define PMIC_UNSUBSCRIBE       _IOR('P', 0xa3, int)
/*! Subscribe a PMIC event for user notification*/
#define PMIC_NOTIFY_USER       _IOR('P', 0xa4, int)
/*! Get the PMIC event occured for which user recieved notification */
#define PMIC_GET_NOTIFY	       _IOW('P', 0xa5, int)
/*! @} */

enum {
	REG_INT_STATUS0 = 0,
	REG_INT_MASK0,
	REG_INT_SENSE0,
	REG_INT_STATUS1,
	REG_INT_MASK1,
	REG_INT_SENSE1,
	REG_PU_MODE_S,
	REG_IDENTIFICATION,
	REG_UNUSED0,
	REG_ACC0,
	REG_ACC1,		/*10 */
	REG_UNUSED1,
	REG_UNUSED2,
	REG_POWER_CTL0,
	REG_POWER_CTL1,
	REG_POWER_CTL2,
	REG_REGEN_ASSIGN,
	REG_UNUSED3,
	REG_MEM_A,
	REG_MEM_B,
	REG_RTC_TIME,		/*20 */
	REG_RTC_ALARM,
	REG_RTC_DAY,
	REG_RTC_DAY_ALARM,
	REG_SW_0,
	REG_SW_1,
	REG_SW_2,
	REG_SW_3,
	REG_SW_4,
	REG_SW_5,
	REG_SETTING_0,		/*30 */
	REG_SETTING_1,
	REG_MODE_0,
	REG_MODE_1,
	REG_POWER_MISC,
	REG_UNUSED4,
	REG_UNUSED5,
	REG_UNUSED6,
	REG_UNUSED7,
	REG_UNUSED8,
	REG_UNUSED9,		/*40 */
	REG_UNUSED10,
	REG_UNUSED11,
	REG_ADC0,
	REG_ADC1,
	REG_ADC2,
	REG_ADC3,
	REG_ADC4,
	REG_CHARGE,
	REG_USB0,
	REG_USB1,		/*50 */
	REG_LED_CTL0,
	REG_LED_CTL1,
	REG_LED_CTL2,
	REG_LED_CTL3,
	REG_UNUSED12,
	REG_UNUSED13,
	REG_TRIM0,
	REG_TRIM1,
	REG_TEST0,
	REG_TEST1,		/*60 */
	REG_TEST2,
	REG_TEST3,
	REG_TEST4,
};
static char const * const regnames[] = {
	"INT_STATUS0",
	"INT_MASK0",
	"INT_SENSE0",
	"INT_STATUS1",
	"INT_MASK1",
	"INT_SENSE1",
	"PU_MODE_S",
	"IDENTIFICATION",
	"UNUSED0",
	"ACC0",
	"ACC1",		/*10 */
	"UNUSED1",
	"UNUSED2",
	"POWER_CTL0",
	"POWER_CTL1",
	"POWER_CTL2",
	"REGEN_ASSIGN",
	"UNUSED3",
	"MEM_A",
	"MEM_B",
	"RTC_TIME",		/*20 */
	"RTC_ALARM",
	"RTC_DAY",
	"RTC_DAY_ALARM",
	"SW_0",
	"SW_1",
	"SW_2",
	"SW_3",
	"SW_4",
	"SW_5",
	"SETTING_0",		/*30 */
	"SETTING_1",
	"MODE_0",
	"MODE_1",
	"POWER_MISC",
	"UNUSED4",
	"UNUSED5",
	"UNUSED6",
	"UNUSED7",
	"UNUSED8",
	"UNUSED9",		/*40 */
	"UNUSED10",
	"UNUSED11",
	"ADC0",
	"ADC1",
	"ADC2",
	"ADC3",
	"ADC4",
	"CHARGE",
	"USB0",
	"USB1",		/*50 */
	"LED_CTL0",
	"LED_CTL1",
	"LED_CTL2",
	"LED_CTL3",
	"UNUSED12",
	"UNUSED13",
	"TRIM0",
	"TRIM1",
	"TEST0",
	"TEST1",		/*60 */
	"TEST2",
	"TEST3",
	"TEST4",
};

typedef enum {
	EVENT_ADCDONEI = 0,
	EVENT_ADCBISDONEI = 1,
	EVENT_TSI = 2,
	EVENT_VBUSVI = 3,
	EVENT_IDFACI = 4,
	EVENT_USBOVI = 5,
	EVENT_CHGDETI = 6,
	EVENT_CHGFAULTI = 7,
	EVENT_CHGREVI = 8,
	EVENT_CHGRSHORTI = 9,
	EVENT_CCCVI = 10,
	EVENT_CHGCURRI = 11,
	EVENT_BPONI = 12,
	EVENT_LOBATLI = 13,
	EVENT_LOBATHI = 14,
	EVENT_IDFLOATI = 19,
	EVENT_IDGNDI = 20,
	EVENT_SE1I = 21,
	EVENT_CKDETI = 22,
	EVENT_1HZI = 24,
	EVENT_TODAI = 25,
	EVENT_PWRONI = 27,
	EVENT_WDIRESETI = 29,
	EVENT_SYSRSTI = 30,
	EVENT_RTCRSTI = 31,
	EVENT_PCI = 32,
	EVENT_WARMI = 33,
	EVENT_MEMHLDI = 34,
	EVENT_THWARNLI = 36,
	EVENT_THWARNHI = 37,
	EVENT_CLKI = 38,
	EVENT_SCPI = 40,
	EVENT_LBPI = 44,
	EVENT_NB,
} type_event;

typedef enum {
	SENSE_VBUSVS = 3,
	SENSE_IDFACS = 4,
	SENSE_USBOVS = 5,
	SENSE_CHGDETS = 6,
	SENSE_CHGREVS = 8,
	SENSE_CHGRSHORTS = 9,
	SENSE_CCCVS = 10,
	SENSE_CHGCURRS = 11,
	SENSE_BPONS = 12,
	SENSE_LOBATLS = 13,
	SENSE_LOBATHS = 14,
	SENSE_IDFLOATS = 19,
	SENSE_IDGNDS = 20,
	SENSE_SE1S = 21,
	SENSE_PWRONS = 27,
	SENSE_THWARNLS = 36,
	SENSE_THWARNHS = 37,
	SENSE_CLKS = 38,
	SENSE_LBPS = 44,
	SENSE_NB,
} t_sensor;

typedef struct {
	bool sense_vbusvs;
	bool sense_idfacs;
	bool sense_usbovs;
	bool sense_chgdets;
	bool sense_chgrevs;
	bool sense_chgrshorts;
	bool sense_cccvs;
	bool sense_chgcurrs;
	bool sense_bpons;
	bool sense_lobatls;
	bool sense_lobaths;
	bool sense_idfloats;
	bool sense_idgnds;
	bool sense_se1s;
	bool sense_pwrons;
	bool sense_thwarnls;
	bool sense_thwarnhs;
	bool sense_clks;
	bool sense_lbps;
} t_sensor_bits;

static char const usage[] = {
	"Usage: pmic 0xregister [value]\n"
	"          or\n"
	"       pmic all\n" 
};

static char const devname[] = {
	"/dev/pmic"
};

int main(int argc, char const * const argv[])
{
	if( 1 < argc ){
		if ((2==argc) && (0 == strcmp("all",argv[1]))) {
			int const fd = open(devname, O_RDWR);
			if( 0 <= fd ){
				unsigned regnum ;
				for( regnum = 0 ; regnum <= REG_TEST4 ; regnum++ ){
					if( (regnum < REG_ADC0) || (REG_ADC4 < regnum) ) {
						register_info reg ; 
						memset(&reg,0,sizeof(reg));
						reg.reg = regnum ;
						int rval = ioctl(fd,PMIC_READ_REG,&reg);
						if( 0 == rval ){
							printf( "0x%02x: %-20s 0x%06x\n", regnum, regnames[regnum], reg.reg_value );
						} else
							perror( "PMIC_READ_REG" );
					} // skip ADC registers (to prevent data loss)
					else
						printf( "0x%02x: %-20s ********\n", regnum, regnames[regnum] );
				}
				close(fd);
			} else
				perror(devname);
		} else {
			unsigned regnum = strtoul(argv[1],0,0);
			if( regnum <= REG_TEST4 ){
				int const fd = open(devname, O_RDWR);
				if( 0 <= fd ){
					register_info reg ; 
					memset(&reg,0,sizeof(reg));
					reg.reg = regnum ;
					int rval = ioctl(fd,PMIC_READ_REG,&reg);
					if( 0 == rval ){
						printf( "reg[0x%x] (%s) == 0x%x\n", regnum, regnames[regnum], reg.reg_value );
						if( 2 < argc ){
							reg.reg_value = strtoul(argv[2],0,0);
							rval = ioctl(fd,PMIC_WRITE_REG,&reg);
							if( 0 == rval ){
								printf( "value changed to 0x%x\n", reg.reg_value );
							}
							else
								perror( "PMIC_WRITE_REG");
						}
					} else 
						perror("PMIC_READ_REG");
				} else
					perror(devname);
			} else 
				fprintf(stderr, "Invalid register: max is 0x%x\n", REG_TEST4 );
		}
	} else
		fprintf(stderr, usage);
	return 0 ;
}
