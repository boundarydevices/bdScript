#include "invDCT.h"
//		               r0            r1              r2              r3
//void mpeg2_idct_fill_arm(int dct,uint8_t * dest,const int stride,const int addFlag)
	.global mpeg2_idct_fill_arm
mpeg2_idct_fill_arm:
	add	r0,r0,#4	//round
	mov	r0,r0,asr #3
	cmp	r3,#0
	beq	mm2
	stmdb   sp!, { r4, lr }		// all callee saved regs
	mov	lr,#8			//	int i=8;
mm1:
	doAdd1	r4,r3,r0,r1,0
	doAdd1	r4,r3,r0,r1,2
	doAdd1	r4,r3,r0,r1,4
	doAdd1	r4,r3,r0,r1,6
	subs	lr,lr,#1
	add	r1,r1,r2		//		dest+=stride;
	bne	mm1				//	} while (--i);
	ldmia   sp!, { r4, pc }   // restore callee saved regs and return

mm2:
	cmp	r0,#255
	orr	r3,r1,r2
	mvnhi	r0,r0,asr #31	//neg vals get 0, pos vals get 0xffffffff, or 255 in each byte
	orrls	r0,r0,r0,lsl #8
	orrls	r0,r0,r0,lsl #16
	tst	r3,#7
	bne	mm3		//br if alignment problems
	mov	r3,r1
	mov	r1,r0

	strd	r0,[r3],r2	//0
	strd	r0,[r3],r2	//1
	strd	r0,[r3],r2	//2
	strd	r0,[r3],r2	//3
	strd	r0,[r3],r2	//4
	strd	r0,[r3],r2	//5
	strd	r0,[r3],r2	//6
	strd	r0,[r3]		//7
	mov	pc,lr
mm3:
	mov	r3,#8
mm4:
	strb	r0,[r1,#0]
	strb	r0,[r1,#1]
	strb	r0,[r1,#2]
	strb	r0,[r1,#3]
	strb	r0,[r1,#4]
	strb	r0,[r1,#5]
	strb	r0,[r1,#6]
	strb	r0,[r1,#7]
	add	r1,r1,r2
	subs	r3,r3,#1
	bne	mm4
	mov	pc,lr

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
//srcDCT is already unquantized array of 64.
//dest is array of 64
//results are clamped to be between 0 min and 255 max
//void GetIDCT_ARM(short *srcDCT, BYTE *dest, DWORD rowAdvance, int addFlag)
	.global GetIDCT_ARM
GetIDCT_ARM:
#define _bJPEG 0
#define _fracBits		6
#define _levelShiftRound 	(1<<(_fracBits-1))		//mpeg
	mainIDCT _rSrcDCT,_rTempLo,_rLoop,_rTemp1,_rTemp2,_rDest,_rRowAdvance,_rAddFlag,_rLevelShiftRound,_fracBits,_levelShiftRound,_bJPEG
