.global uDivRem
.global uReciprocal
.global uShiftRight
.global reverseDec
//extern "C" unsigned long reverseDec(unsigned long v);
reverseDec:
	clz		r1,r0
	mov		r2,#0x80000000
	eor		r0,r0,r2,ASR r1
	mov		pc,lr
	
//extern "C" unsigned long uDivRem(unsigned long value,unsigned long divisor,unsigned long * rem);
//In: r0 - value, r1 - divisor, r2 - @remainder
//Out: r0 - value/divisor, @r2 - remainder
uDivRem:
	str		lr,[sp,#-4]!
	clz		r3,r0
	clz		lr,r1
	subs	r3,lr,r3
	movmi	r3,#0
	mov		lr,r0,LSR r3
	rsb		r3,r3,#32
	mov		r0,r0,LSL r3
	rsb		r3,r3,#33
	b		Z3

Z1:	movs	r0,r0,LSL #1
	adc		lr,lr,lr
Z3:	cmp		lr,r1
	subhs	lr,lr,r1
	addhs	r0,r0,#1
	subs	r3,r3,#1
	bne		Z1

	str		lr,[r2]
	ldr		pc,[sp],#4

//extern "C" unsigned long uReciprocal(unsigned long value,unsigned long * rem);
//In: r0 - value, r1 - @remainder
//Out: r0 - (1<<32)/value, @r1 - remainder
uReciprocal:
	str		lr,[sp,#-4]!
	mov		r2,r1
	mov		r1,r0
	mov		r0,#0x80000000
	clz		r3,r1
	mov		lr,r0,LSR r3
	mov		r0,#0
	add		r3,r3,#2
	b		Z3

//extern "C" unsigned long uShiftRight(ULONGLONG* m_nextBitsBuffer,int shift);
uShiftRight:
	ldr		r2,[r0]
	ldr		r3,[r0,#4]
	mov		r0,r2,LSR r1
	rsb		r1,r1,#32
	orr		r0,r0,r3,LSL r1
	mov		pc,lr
	
//int __umull18(unsigned valueA,unsigned valueB);
__umull18:
	umull	r3,r2,r1,r0
	mov		r0,r2,LSL # (32-18)
	add		r0,r0,r3,LSR # (18)
	mov		pc,lr
