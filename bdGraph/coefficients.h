
#ifndef __u64
#define __u64 uint64_t
#endif
//the above equate control the precision of multiplications
#define tSqrt2 1.4142135623730950488

#define half 0x80000000
#if 0
#define t1      0.98078528			//cos(11.25)
#define t2      0.923879532			//cos(22.5)
#define t3      0.831469612			//cos(33.75)
#define t4      0.7071067811865475244		//cos(45)
#define t5      0.555570233			//cos(56.25)
#define t6      0.382683432			//cos(67.5)
#define t7      0.195090322			//cos(78.75)
#define halfv(a) DWORD((a) * half)
#else


#define t1		((__u64)(0xfb14be7e))
#define t2		((__u64)(0xec835e78))
#define t3		((__u64)(0xd4db3148))
#define t4		((__u64)(0xb504f334))
#define t5		((__u64)(0x8e39d9cc))
#define t6		((__u64)(0x61f78a99))
#define t7		((__u64)(0x31f17078))
#define halfv(a) ((a) >> 1)
#endif


//the defines below only affect the initial dct multiplies
//cos(a)cos(b) = (cos(a+b)+cos(a-b))/2

#define ccFracBits 32

#define c1c1 (halfv(t2)+half)	//((one+c2)>>1)
#define c1c2 halfv(t1+t3)	//((c1+c3)>>1)
#define c1c3 halfv(t2+t4)	//((c2+c4)>>1)
#define c1c4 halfv(t3+t5)	//((c3+c5)>>1)
#define c1c5 halfv(t4+t6)	//((c4+c6)>>1)
#define c1c6 halfv(t5+t7)	//((c5+c7)>>1)
#define c1c7 halfv(t6)		//(c6>>1)

#define c2c1 c1c2
#define c2c2 (halfv(t4)+half)	//((one+c4)>>1)
#define c2c3 halfv(t1+t5)	//((c1+c5)>>1)
#define c2c4 halfv(t2+t6)	//((c2+c6)>>1)
#define c2c5 halfv(t3+t7)	//((c3+c7)>>1)
#define c2c6 halfv(t4)		//(c4>>1)
#define c2c7 halfv(t5-t7)	//((c5-c7)>>1)

#define c3c1 c1c3
#define c3c2 c2c3
#define c3c3 (halfv(t6)+half)	//((one+c6)>>1)
#define c3c4 halfv(t1+t7)	//((c1+c7)>>1)
#define c3c5 halfv(t2)		//(c2>>1)
#define c3c6 halfv(t3-t7)	//((c3-c7)>>1)
#define c3c7 halfv(t4-t6)	//((c4-c6)>>1)
#define c4c4 half
#define c4c1 c1c4
#define c4c2 c2c4
#define c4c3 c3c4
#define C4C4 half
#define c4c5 halfv(t1-t7)	//((c1-c7)>>1)
#define c4c6 halfv(t2-t6)	//((c2-c6)>>1)
#define c4c7 halfv(t3-t5)	//((c3-c5)>>1)

#define c5c1 c1c5
#define c5c2 c2c5
#define c5c3 c3c5
#define c5c4 c4c5
#define c5c5 (half-halfv(t6))	//((one-c6)>>1)
#define c5c6 halfv(t1-t5)	//((c1-c5)>>1)
#define c5c7 halfv(t2-t4)	//((c2-c4)>>1)

#define c6c1 c1c6
#define c6c2 c2c6
#define c6c3 c3c6
#define c6c4 c4c6
#define c6c5 c5c6
#define c6c6 (half-halfv(t4))	//((one-c4)>>1)
#define c6c7 halfv(t1-t3)	//((c1-c3)>>1)

#define c7c1 c1c7
#define c7c2 c2c7
#define c7c3 c3c7
#define c7c4 c4c7
#define c7c5 c5c7
#define c7c6 c6c7
#define c7c7 (half-halfv(t2))	//((one-c2)>>1)

//The initial matrix multiply of this algorithm is
//Q[] = |c4   0   0   0   0   0   0   0 |
//      | 0 -c1   0   0   0   0   0   0 |
//      | 0   0 -c2   0   0   0   0   0 |
//      | 0   0   0 -c3   0   0   0   0 |
//      | 0   0   0   0 -c4   0   0   0 |
//      | 0   0   0   0   0 -c5   0   0 |
//      | 0   0   0   0   0   0 -c6   0 |
//      | 0   0   0   0   0   0   0  c7 |
//the sign is defined below
#define CE00   c4c4
#define CE01   c4c1
#define CE02   c4c2
#define CE03   c4c3
#define CE04   c4c4
#define CE05   c4c5
#define CE06   c4c6
#define CE07   c4c7

#define CE10   c1c4
#define CE11   c1c1
#define CE12   c1c2
#define CE13   c1c3
#define CE14   c1c4
#define CE15   c1c5
#define CE16   c1c6
#define CE17   c1c7

#define CE20   c2c4
#define CE21   c2c1
#define CE22   c2c2
#define CE23   c2c3
#define CE24   c2c4
#define CE25   c2c5
#define CE26   c2c6
#define CE27   c2c7

#define CE30   c3c4
#define CE31   c3c1
#define CE32   c3c2
#define CE33   c3c3
#define CE34   c3c4
#define CE35   c3c5
#define CE36   c3c6
#define CE37   c3c7

#define CE40   c4c4
#define CE41   c4c1
#define CE42   c4c2
#define CE43   c4c3
#define CE44   c4c4
#define CE45   c4c5
#define CE46   c4c6
#define CE47   c4c7

#define CE50   c5c4
#define CE51   c5c1
#define CE52   c5c2
#define CE53   c5c3
#define CE54   c5c4
#define CE55   c5c5
#define CE56   c5c6
#define CE57   c5c7

#define CE60   c6c4
#define CE61   c6c1
#define CE62   c6c2
#define CE63   c6c3
#define CE64   c6c4
#define CE65   c6c5
#define CE66   c6c6
#define CE67   c6c7

#define CE70   c7c4
#define CE71   c7c1
#define CE72   c7c2
#define CE73   c7c3
#define CE74   c7c4
#define CE75   c7c5
#define CE76   c7c6
#define CE77   c7c7


#define SCE00
#define SCE01  -
#define SCE02  -
#define SCE03  -
#define SCE04  -
#define SCE05  -
#define SCE06  -
#define SCE07

#define SCE10  -
#define SCE11
#define SCE12
#define SCE13
#define SCE14
#define SCE15
#define SCE16
#define SCE17  -

#define SCE20  -
#define SCE21
#define SCE22
#define SCE23
#define SCE24
#define SCE25
#define SCE26
#define SCE27  -

#define SCE30  -
#define SCE31
#define SCE32
#define SCE33
#define SCE34
#define SCE35
#define SCE36
#define SCE37  -

#define SCE40  -
#define SCE41
#define SCE42
#define SCE43
#define SCE44
#define SCE45
#define SCE46
#define SCE47  -

#define SCE50  -
#define SCE51
#define SCE52
#define SCE53
#define SCE54
#define SCE55
#define SCE56
#define SCE57  -

#define SCE60  -
#define SCE61
#define SCE62
#define SCE63
#define SCE64
#define SCE65
#define SCE66
#define SCE67  -

#define SCE70
#define SCE71  -
#define SCE72  -
#define SCE73  -
#define SCE74  -
#define SCE75  -
#define SCE76  -
#define SCE77

//#define RD4(a) (((a)+1)>>1)	//make sure answer fits in a signed 32 bit int, by dumping table when all positive
#define RD4(a) (int)(S##a (((a)+1)>>1))

