#include "invDCT.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////
#define _rTemp1		r8	//_rTemp1,_rTemp2 are even/odd pair
#define _rTemp2		r9
#define _rTempLo	r10
#define _rSrcDCT	r12
#define _rDest		r12	//double duty
#define _rLoop		lr

#define _rRowAdvance	_rTemp1
#define _rAddFlag	_rTemp2
#define _rLevelShiftRound _rTempLo

iOneDivC4:	.int oneDivC4
iC6x:		.int	C6x
iC2mC6x:	.int	C2mC6x
iC2pC6x:	.int	C2pC6x
//void GetIDCT_ARM_JQ(short *srcDCT, BYTE *dest, DWORD rowAdvance, const int* qTable)
	.global GetIDCT_ARM_JQ
GetIDCT_ARM_JQ:
#define _bJPEG_JQ 1
#define _fracBits_JQ		16
#define _levelShiftRound_JQ 	(0x0800000+(1<<(_fracBits_JQ-1)))	//jpeg
	mainIDCT _rSrcDCT,_rTempLo,_rLoop,_rTemp1,_rTemp2,_rDest,_rRowAdvance,_rAddFlag,_rLevelShiftRound,_fracBits_JQ,_levelShiftRound_JQ,_bJPEG_JQ
//
