/*
//------------------------------------------------------------------------------
//FILE DESCRIPTION: Contain assembler routines for the Inverse DCT
//------------------------------------------------------------------------------


   Copyright (c) 2003 Troy Kisky (troy.kisky@boundarydevices.com

*/

//tens textequ   <1.0000000000000000000>
//t2   textequ    <.9238795320000000000>
//t4   textequ    <.7071067811865475244>
//t6   textequ    <.3826834320000000000>
//tSqrt2 textequ <1.4142135623730950488>

//t2mt6			 <.5411961000000000000>
//t2pt6			<1.3065629640000000000>
#define oneDivC4	0x5a82799a	// /2**30   ((tSqrt2 *(040000000h/400h))/tens);  //sqrt(2)
#define C6x		0x61f78a99	// /2**32   ((t6     *(080000000h/400h))/tens);	//t6=.382683432
#define C2mC6x		0x4545e9ef	// /2**31   (((t2-t6)*(080000000h/400h))/tens); //t2=.923879532, t2-t6=.541
#define C2pC6x		0x539eba44	// /2**30   (((t2+t6)*(040000000h/400h))/tens); //+=1.306
#define C2x		0x7641af3c	// /2**31   ((t2     *(080000000h/400h))/tens);
#define C4x		0x5a82799a	// /2**31   ((t4     *(080000000h/400h))/tens);

#define SHFT_C2mC6x		31
#define SHFT_C2mC6x_M1		30

#define SHFT_oneDivC4		30


#define SHFT_C6x		32
#define SHFT_C6x_M1		31

#define SHFT_C2pC6x		30
#define SHFT_C2pC6x_M1		29




.macro	doAdd1	reg1,reg2,dct,src,offset
	ldrb	\reg1,[\src,#\offset]
	ldrb	\reg2,[\src,#\offset+1]
	add	\reg1,\reg1,\dct
	add	\reg2,\reg2,\dct
	cmp	\reg1,#255
	mvnhi	\reg1,\reg1,asr #31	//neg vals get 0, pos vals get 0xffffffff, or 255 if just worried about low byte

	cmp	\reg2,#255
	mvnhi	\reg2,\reg2,asr #31

	strb	\reg1,[\src,#\offset]
	strb	\reg2,[\src,#\offset+1]
.endm
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

.macro	StoreIntCol src
	str	r0,[\src,#1*8*4]
	str	r1,[\src,#2*8*4]
	str	r2,[\src,#3*8*4]
	str	r3,[\src,#4*8*4]
	str	r4,[\src,#5*8*4]
	str	r5,[\src,#6*8*4]
	str	r6,[\src,#7*8*4]
	str	r7,[\src,#8*8*4]
	add	\src,\src,#4
.endm

//for use with jpeg
#define A00 0
#define A01 1		//Aij, row, then column, zig-zag sequence
#define A02 5
#define A03 6
#define A04 14
#define A05 15
#define A06 27
#define A07 28
#define A10 2
#define A11 4
#define A12 7
#define A13 13
#define A14 16
#define A15 26
#define A16 29
#define A17 42
#define A20 3
#define A21 8
#define A22 12
#define A23 17
#define A24 25
#define A25 30
#define A26 41
#define A27 43
#define A30 9
#define A31 11
#define A32 18
#define A33 24
#define A34 31
#define A35 40
#define A36 44
#define A37 53
#define A40 10
#define A41 19
#define A42 23
#define A43 32
#define A44 39
#define A45 45
#define A46 52
#define A47 54
#define A50 20
#define A51 22
#define A52 33
#define A53 38
#define A54 46
#define A55 51
#define A56 55
#define A57 60
#define A60 21
#define A61 34
#define A62 37
#define A63 47
#define A64 50
#define A65 56
#define A66 59
#define A67 61
#define A70 35
#define A71 36
#define A72 48
#define A73 49
#define A74 57
#define A75 58
#define A76 62
#define A77 63
.macro	LoadAll_JQ	src,off0,off1,off2,off3,off4,off5,off6,off7,rTemp1,rTemp2
	ldrsh	\rTemp1,[\src,#\off0*2]
	smull	r0,\rTemp2,\rTemp1,r0
	ldrsh	\rTemp1,[\src,#\off1*2]
	smull	r1,\rTemp2,\rTemp1,r1
	ldrsh	\rTemp1,[\src,#\off2*2]
	smull	r2,\rTemp2,\rTemp1,r2
	ldrsh	\rTemp1,[\src,#\off3*2]
	smull	r3,\rTemp2,\rTemp1,r3
	ldrsh	\rTemp1,[\src,#\off4*2]
	smull	r4,\rTemp2,\rTemp1,r4
	ldrsh	\rTemp1,[\src,#\off5*2]
	smull	r5,\rTemp2,\rTemp1,r5
	ldrsh	\rTemp1,[\src,#\off6*2]
	smull	r6,\rTemp2,\rTemp1,r6
	ldrsh	\rTemp1,[\src,#\off7*2]
.endm
.macro	LoadInt16Col_JQ src,qscale,rLoop,rTemp1,rTemp2
	ldmia	\qscale!,{r0-r7}
	add	pc,pc,\rLoop,LSL #4+2
	nop
	LoadAll_JQ	\src,A00,A40,A20,A60,A50,A10,A70,A30,\rTemp1,\rTemp2
	b	1f
	LoadAll_JQ	\src,A04,A44,A24,A64,A54,A14,A74,A34,\rTemp1,\rTemp2
	b	1f
	LoadAll_JQ	\src,A02,A42,A22,A62,A52,A12,A72,A32,\rTemp1,\rTemp2
	b	1f
	LoadAll_JQ	\src,A06,A46,A26,A66,A56,A16,A76,A36,\rTemp1,\rTemp2
	b	1f
	LoadAll_JQ	\src,A05,A45,A25,A65,A55,A15,A75,A35,\rTemp1,\rTemp2
	b	1f
	LoadAll_JQ	\src,A01,A41,A21,A61,A51,A11,A71,A31,\rTemp1,\rTemp2
	b	1f
	LoadAll_JQ	\src,A07,A47,A27,A67,A57,A17,A77,A37,\rTemp1,\rTemp2
	b	1f
	LoadAll_JQ	\src,A03,A43,A23,A63,A53,A13,A73,A33,\rTemp1,\rTemp2

1:
	smull	r7,\rTemp2,\rTemp1,r7
.endm
//  *********************************************************************
.macro	LoadInt16Col src
	ldrsh	r0,[\src],#2
	ldrsh	r1,[\src,#1*8*2 -2]
	ldrsh	r2,[\src,#2*8*2 -2]
	ldrsh	r3,[\src,#3*8*2 -2]
	ldrsh	r4,[\src,#4*8*2 -2]
	ldrsh	r5,[\src,#5*8*2 -2]
	ldrsh	r6,[\src,#6*8*2 -2]
	ldrsh	r7,[\src,#7*8*2 -2]
.endm
//  *********************************************************************

.macro	DivC4 reg,rTempLo,rTemp1
	ldr	\rTemp1,iOneDivC4
	smull	\rTempLo,\reg,\rTemp1,\reg
	mov	\reg,\reg,LSL #32-SHFT_oneDivC4
	add	\reg,\reg,\rTempLo,LSR #SHFT_oneDivC4
.endm

.macro	SMullI	a,constVal,shift,rTempLo,rTemp1
	ldr	\rTemp1,\constVal
	smull	\rTempLo,\a,\rTemp1,\a
	mov	\a,\a,LSL #32-\shift
	add	\a,\a,\rTempLo,LSR #\shift
.endm

.macro	DoCalculate rTempLo,rTemp1,rTemp2
//	P' and Q' are already done

//  a0 a1 a2 a3 a4 a5 a6 a7
// | 1  0  0  0  0  0  0  0  |
// | 0  1  0  0  0  0  0  0  |
// | 0  0  1  0  0  0  0  0  |
// | 0  0  0  1  0  0  0  0  |
// | 0  0  0  0  1  0  0 -1  |
// | 0  0  0  0  0  1 -1  0  |
// | 0  0  0  0  0  1  1  0  |
// | 0  0  0  0  1  0  0  1  |
//  B1 4 additions
	sub	r4,r4,r7		//	a4 -= a7;
	sub	r5,r5,r6		//	a5 -= a6;
	add	r7,r4,r7,lsl #1		//	a7 = a4 + (a7<<1);
	add	r6,r5,r6,lsl #1		//	a6 = a5 + (a6<<1);

//  a0 a1 a2 a3 a4 a5 a6 a7
// | 1  0  0  0  0  0  0  0  |
// | 0  1  0  0  0  0  0  0  |
// | 0  0  1 -1  0  0  0  0  |
// | 0  0  1  1  0  0  0  0  |
// | 0  0  0  0  1  0  0  0  |
// | 0  0  0  0  0  1  0 -1  |
// | 0  0  0  0  0  0  1  0  |
// | 0  0  0  0  0  1  0  1  |
//  B2  4 additions
	ldr	\rTemp1,iOneDivC4
	sub	r2,r2,r3		//	a2 -= a3;
	sub	r5,r5,r7		//	a5 -= a7;

//  a0 a1 a2   a3  a4   a5   a6   a7
// | 1  0  0    0   0    0    0    0  |
// | 0  1  0    0   0    0    0    0  |
// | 0  0  1/c4 0   0    0    0    0  |
// | 0  0  0    1   0    0    0    0  |
// | 0  0  0    0   2c2  0    2c6  0  |
// | 0  0  0    0   0    1/c4 0    0  |
// | 0  0  0    0  -2c6  0    2c2  0  |
// | 0  0  0    0   0    0    0    1  |
//   M  5 mult  3 additions

//	DivC4	r2,\rTempLo,\rTemp1 //	a2 = DivC4(a2);
	smull	\rTempLo,\rTemp2,\rTemp1,r2
	add	r7,r5,r7,lsl #1		//	a7 = a5 + (a7<<1); from above
	add	r3,r2,r3,lsl #1		//	a3 = a2 + (a3<<1);
	mov	r2,\rTemp2,LSL #32-SHFT_oneDivC4
	add	r2,r2,\rTempLo,LSR #SHFT_oneDivC4

//	DivC4	r5,\rTempLo,\rTemp1 //	a5 = DivC4(a5);
	smull	\rTempLo,r5,\rTemp1,r5
	ldr	\rTemp1,iC6x
	add	\rTemp2,r4,r6		//		int mab = a4+a6;
	mov	r5,r5,LSL #32-SHFT_oneDivC4
	add	r5,r5,\rTempLo,LSR #SHFT_oneDivC4

//	a4' = a4*(2c2) + a6*(2c6);
//	a6' = a6*(2c2) - a4*(2c6);
//is equal to
//	a4' = (a4)*(2c2-2c6) + (a4+a6)*(2c6);
//	a6' = (a6)*(2c2+2c6) - (a4+a6)*(2c6);
					//	{
//	SMullI	\rTemp2,iC6x,SHFT_C6x_M1,\rTempLo,\rTemp1	//	mab = smull(mab,C6x,SHFT_C6x_M1);	//31-1
//	SMullI	r4,iC2mC6x,SHFT_C2mC6x_M1,\rTempLo,\rTemp1	//	a4 = smull(a4,C2mC6x,SHFT_C2mC6x_M1) + mab;	//31-1
//	SMullI	r6,iC2pC6x,SHFT_C2pC6x_M1,\rTempLo,\rTemp1	//	a6 = smull(a6,C2pC6x,SHFT_C2pC6x_M1) - mab;	//30-1


	smull	\rTempLo,\rTemp2,\rTemp1,\rTemp2
	ldr	\rTemp1,iC2mC6x
	sub	r0,r0,r1		//	a0 -= a1; //from below
	mov	\rTemp2,\rTemp2,LSL #32-SHFT_C6x_M1
	add	\rTemp2,\rTemp2,\rTempLo,LSR #SHFT_C6x_M1

	smull	\rTempLo,r4,\rTemp1,r4
	ldr	\rTemp1,iC2pC6x
	sub	r2,r2,r3		//	a2 -= a3;
	mov	r4,r4,LSL #32-SHFT_C2mC6x_M1
	add	r4,r4,\rTempLo,LSR #SHFT_C2mC6x_M1

	smull	\rTempLo,r6,\rTemp1,r6
	mov	r6,r6,LSL #32-SHFT_C2pC6x_M1
	add	r6,r6,\rTempLo,LSR #SHFT_C2pC6x_M1
/////////////////////////////////
	add	r4,r4,\rTemp2
	sub	r6,r6,\rTemp2
					//	}


//  a0 a1 a2 a3 a4 a5 a6 a7
// | 1 -1  0  0  0  0  0  0  |
// | 1  1  0  0  0  0  0  0  |
// | 0  0  1 -1  0  0  0  0  |
// | 0  0  0  1  0  0  0  0  |
// | 0  0  0  0  1  0  0  0  |
// | 0  0  0  0  0  1  0  0  |
// | 0  0  0  0  0  0  1 -1  |
// | 0  0  0  0  0  0  0  1  |
//   A1  4 additions
	sub	r6,r6,r7		//	a6 -= a7;
	add	r1,r0,r1,lsl #1		//	a1 = a0 + (a1<<1);


//  a0 a1 a2 a3 a4 a5 a6 a7
// | 1  0  0 -1  0  0  0  0 |
// | 0  1 -1  0  0  0  0  0 |
// | 0  1  1  0  0  0  0  0 |
// | 1  0  0  1  0  0  0  0 |
// | 0  0  0  0  1  0  0  0 |
// | 0  0  0  0  0  1 -1  0 |
// | 0  0  0  0  0  0  1  0 |
// | 0  0  0  0  0  0  0  1 |
//   A2 5 additions

	sub	r0,r0,r3		//	a0 -= a3;
	sub	r1,r1,r2		//	a1 -= a2;
	sub	r5,r5,r6		//	a5 -= a6;
	add	r3,r0,r3,lsl #1		//	a3 = a0 + (a3<<1);
	add	r2,r1,r2,lsl #1		//	a2 = a1 + (a2<<1);

//  a0 a1 a2 a3 a4 a5 a6 a7
// | 1  0  0  0  0  0  0  0 |
// | 0  1  0  0  0  0  0  0 |
// | 0  0  1  0  0  0  0  0 |
// | 0  0  0  1  0  0  0  0 |
// | 0  0  0  0  1 -1  0  0 |
// | 0  0  0  0  0  1  0  0 |
// | 0  0  0  0  0  0  1  0 |
// | 0  0  0  0  0  0  0  1 |
//   A3  1 addition
	sub	r4,r4,r5		//	a4 -= a5;


//  a0 a1 a2 a3 a4 a5 a6 a7
// | 1  0  0  0  0  0  0 -1 |
// | 0  1  0  0  0  0 -1  0 |
// | 0  0  1  0  0 -1  0  0 |
// | 0  0  0  1 -1  0  0  0 |
// | 0  0  0  1  1  0  0  0 |
// | 0  0  1  0  0  1  0  0 |
// | 0  1  0  0  0  0  1  0 |
// | 1  0  0  0  0  0  0  1 |
//   A4   8 additions
	sub	r0,r0,r7		//	a0 -= a7;
	sub	r1,r1,r6		//	a1 -= a6;
	sub	r2,r2,r5		//	a2 -= a5;
	sub	r3,r3,r4		//	a3 -= a4;
	add	r7,r0,r7,lsl #1		//	a7 = a0 + (a7<<1);
	add	r6,r1,r6,lsl #1		//	a6 = a1 + (a6<<1);
	add	r5,r2,r5,lsl #1		//	a5 = a2 + (a5<<1);
	add	r4,r3,r4,lsl #1		//	a4 = a3 + (a4<<1);
.endm


.macro rangeCheck	reg1,reg2,regLSR,fracBits,levelShiftRound,bJPEG
    .if (\bJPEG)
	add	\reg1,\reg1,\regLSR		//levelShiftRound
	add	\reg2,\reg2,\regLSR
    .else
	add	\reg1,\reg1,#\levelShiftRound
	add	\reg2,\reg2,#\levelShiftRound
    .endif
	mov	\reg1,\reg1,asr #\fracBits	//int a = ((*dct)+levelShiftRound)>>fracBits;
	mov	\reg2,\reg2,asr #\fracBits
	cmp	\reg1,#255
	mvnhi	\reg1,\reg1,asr #31	//neg vals get 0, pos vals get 0xffffffff, or 255 if just worried about low byte

	cmp	\reg2,#255
	mvnhi	\reg2,\reg2,asr #31

.endm


	
.macro mainIDCT	rSrcDCT,rTempLo,rLoop,rTemp1,rTemp2,rDest,rRowAdvance,rAddFlag,rLevelShiftRound,fracBits,levelShiftRound,bJPEG
	stmdb   sp!, { r1 - r12, lr }		// all callee saved regs
	add	fp,sp,#3*4
	sub	sp, sp, #(64+8)*4		// reserve some space on the stack, the top 8 entries are not used, but allow the
						// the loop to increase the sp as it goes without losing good data (signals won't trash good data)
	bic	sp, sp, #31			// get to a cache line boundary
	mov	\rSrcDCT,r0
	mov	\rLoop,#0
//nextCol
0:
    .if (\bJPEG)
	ldr	\rTempLo,[fp,#-4]
	LoadInt16Col_JQ \rSrcDCT,\rTempLo,\rLoop,\rTemp1,\rTemp2
	str	\rTempLo,[fp,#-4]
    .else
	LoadInt16Col \rSrcDCT
    .endif
//Calc1
1:
	add	\rLoop,\rLoop,#1
	DoCalculate \rTempLo,\rTemp1,\rTemp2
	cmp	\rLoop,#8
	bhi	2f			//SaveRow
	StoreIntCol sp			//advances sp by 4
	blo	0b			//nextCol
//////	sub	sp,sp, #8*4	//reset pointer, this is no longer needed
	ldr	\rDest,[fp,#-12]
	ldmia	sp!,{r0-r7}
	b	1b			//Calc1
//SaveRow
2:
    .if (\bJPEG)
	ldr	\rRowAdvance,[fp,#-8]
	mov	\rLevelShiftRound,#\levelShiftRound & 0xffff0000
	orr	\rLevelShiftRound,\rLevelShiftRound,#\levelShiftRound & 0xffff
    .else
	ldmdb   fp, { \rRowAdvance,\rAddFlag}		// restore saved parameters

	cmp	\rAddFlag,#0
	beq	3f

	ldrb	\rAddFlag,[\rDest,#0]
	ldrb	\rTempLo,[\rDest,#1]
	add	r0,r0,\rAddFlag,LSL #\fracBits
	add	r1,r1,\rTempLo,LSL #\fracBits

	ldrb	\rAddFlag,[\rDest,#2]
	ldrb	\rTempLo,[\rDest,#3]
	add	r2,r2,\rAddFlag,LSL #\fracBits
	add	r3,r3,\rTempLo,LSL #\fracBits

	ldrb	\rAddFlag,[\rDest,#4]
	ldrb	\rTempLo,[\rDest,#5]
	add	r4,r4,\rAddFlag,LSL #\fracBits
	add	r5,r5,\rTempLo,LSL #\fracBits

	ldrb	\rAddFlag,[\rDest,#6]
	ldrb	\rTempLo,[\rDest,#7]
	add	r6,r6,\rAddFlag,LSL #\fracBits
	add	r7,r7,\rTempLo,LSL #\fracBits
3:
    .endif

	rangeCheck	r0,r1,\rLevelShiftRound,\fracBits,\levelShiftRound,\bJPEG	//		*dest++ = rangeCheck(dct++); *dest++ = rangeCheck(dct++);
	rangeCheck	r2,r3,\rLevelShiftRound,\fracBits,\levelShiftRound,\bJPEG	//		*dest++ = rangeCheck(dct++); *dest++ = rangeCheck(dct++);
	rangeCheck	r4,r5,\rLevelShiftRound,\fracBits,\levelShiftRound,\bJPEG	//		*dest++ = rangeCheck(dct++); *dest++ = rangeCheck(dct++);
	rangeCheck	r6,r7,\rLevelShiftRound,\fracBits,\levelShiftRound,\bJPEG	//		*dest++ = rangeCheck(dct++); *dest++ = rangeCheck(dct++);
	strb	r1,[\rDest,#1]
	strb	r2,[\rDest,#2]
	strb	r3,[\rDest,#3]
	strb	r4,[\rDest,#4]
	strb	r5,[\rDest,#5]
	strb	r6,[\rDest,#6]
	strb	r7,[\rDest,#7]
	strb	r0,[\rDest],\rRowAdvance

	cmp	\rLoop,#16
	ldmneia	sp!,{r0-r7}
	bne	1b		//Calc1

	mov	sp,fp
	ldmia   sp!, { r4 - r12, pc }   // restore callee saved regs and return
.endm
