Bitwise Tricks
C code	Description	Obtained from
a ^= b; b ^= a; a ^= b;	Swap a and b around	ComputerShopper
c & -c or c & (~c + 1)	return first bit set	Eivind Eklund
~c & (c + 1)	return first unset bit	Eivind Eklund
unsigned i, c = <number>;
for (i = 0; c; i++) c ^= c & -c;	i will contain the number of bits set in c	Eivind Eklund
m = (m & 0x55555555) + ((m & 0xaaaaaaaa) >> 1);
m = (m & 0x33333333) + ((m & 0xcccccccc) >> 2);
m = (m & 0x0f0f0f0f) + ((m & 0xf0f0f0f0) >> 4);
m = (m & 0x00ff00ff) + ((m & 0xff00ff00) >> 8);
m = (m & 0x0000ffff) + ((m & 0xffff0000) >> 16);	m will now contain number of bits set in the original m	popcnt in FreeBSD's sys/i386/i386/mp_machdep.c
v -= ((v >> 1) & 0x55555555);
v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
v = (v + (v >> 4)) & 0x0F0F0F0F;
v = (v * 0x01010101) >> 24;	v will now contain number of bits set in the original v	popcnt comment in FreeBSD's sys/i386/i386/mp_machdep.c
#define BITCOUNT(x) (((BX_(x)+(BX_(x)>>4)) & 0x0F0F0F0F) % 255)
#define BX_(x) ((x) - (((x)>>1)&0x77777777) \
- (((x)>>2)&0x33333333) \
- (((x)>>3)&0x11111111))	macro to return number of bits in x (assumes 32bit integer)	BSD fortune
n = ((n >> 1) & 0x55555555) | ((n << 1) & 0xaaaaaaaa);
n = ((n >> 2) & 0x33333333) | ((n << 2) & 0xcccccccc);
n = ((n >> 4) & 0x0f0f0f0f) | ((n << 4) & 0xf0f0f0f0);
n = ((n >> 8) & 0x00ff00ff) | ((n << 8) & 0xff00ff00);
n = ((n >> 16) & 0x0000ffff) | ((n << 16) & 0xffff0000);	reverse bits in a 32bit integer	BSD fortune
popcnt(x ^ (x - 1)) & 31	return first bit set in x, where popcnt returns number of bits set	Peter Wemm
ffs using a lookup table	return first bit set	Colin Percival

If you have any other cool bitwise tricks you know about, please e-mail them to John-Mark Gurney <gurney_j@resnet.uoregon.edu>, and I will add them to the list.

	3.2.3.5 Division by Integer Constants
On many implementations, integer division is rather slow compared to integer multiplication or other integer arithmetic and logical operations. When the divisor is a constant, integer division instructions can be replaced by shifts to the right for divisors that are powers of 2 or by multiplication by a magic number for other divisors. The following describes techniques for 32-bit code, but everything extends in a straightforward way to 64-bit code.



Figure 3-27. Signed Maximum of a and b Code Sequence
	# R3 = a
	# R4 = b
	xoris 	R5,R4,0x8000 	# flip sign b
	xoris 	R6,R3,0x8000 	# flip sign a
	# the problem is now analogous to that of the unsigned maximum
	subfc 	R6,R6,R5 	# R6 = R5 - R6 = b - a with carry
			# CA = (b >= a) ? 1 : 0
	subfe 	R5,R5,R5 	# R5 = (b >= a) ? 0 : -1
	andc 	R6,R6,R5 	# R6 = (b >= a) ? (Rb - Ra) : 0
	add 	R6,R6,R3 	# R6 = (b >= a) ? Rb : Ra
	# R6 = result

Signed Division
Most computers and high-level languages use a truncating type of signed-integer division in which the quotient q is defined as the integer part of n/d with the fraction discarded. The remainder is defined as the integer r that satisfies

n = q × d + r

where 0 r < |d| if n 0, and -|d| < r 0 if n < 0. If n = -231 and d = -1, the quotient is undefined. Most computers implement this definition, including PowerPC processor-based computers. Consider the following examples of truncating division:

7/3 = 2 remainder 1

(-7)/3 = -2 remainder -1

7/(-3) = -2 remainder 1

(-7)/(-3) = 2 remainder -1

Signed Division by a Power of 2
If the divisor is a power of 2, that is 2k for 1 k 31, integer division may be computed using two elementary instructions:

srawi Rq,Rn,Rk
addze Rq,Rq

Rn contains the dividend, and after executing the preceding instructions, Rq contains the quotient of n divided by 2k. This code uses the fact that, in the PowerPC architecture, the shift right algebraic instructions set the Carry bit if the source register contains a negative number and one or more 1-bits are shifted out. Otherwise, the carry bit is cleared. The addze instruction corrects the quotient, if necessary, when the dividend is negative. For example, if n = -13, (0xFFFF_FFF3), and k = 2, after executing the srawi instruction, q = -4 (0xFFFF_FFFC) and CA = 1. After executing the addze instruction, q = -3, the correct quotient.

Signed Division by Non-Powers of 2
For any divisor d other than 0, division by d can be computed by a multiplication and a few elementary instructions such as adds and shifts. The basic idea is to multiply the dividend n by a magic number, a sort of reciprocal of d that is approximately equal to 232/d. The high-order 32 bits of the product represent the quotient. This algorithm uses the PowerPC multiply high instructions. The details, however, are complicated, particularly for certain divisors such as 7. Figures 3-28, 3-29, and 3-30 show the code for divisors of 3, 5, and 7, respectively. The examples include steps for obtaining the remainder by simply subtracting q ×d from the dividend n.



Figure 3-28. Signed Divide by 3 Code Sequence
	lis 	Rm,0x5555 	# load magic number = m
	addi 	Rm,Rm,0x5556 	# m = 0x55555556 = (232 + 2)/3
	mulhw 	Rq,Rm,Rn 	# q = floor(m*n/232)
	srwi 	Rt,Rn,31 	# add 1 to q if
	add 	Rq,Rq,Rt 	# n is negative
			#
	mulli 	Rt,Rq,3 	# compute remainder from
	sub 	Rr,Rn,Rt 	# r = n - q*3



Figure 3-29. Signed Divide by 5 Code Sequence
	lis 	Rm,0x6666 	# load magic number = m
	addi 	Rm,Rm,0x6667 	# m = 0x66666667 = (233 + 3)/5
	mulhw 	Rq,Rm,Rn 	# q = floor(m*n/232)
	srawi 	Rq,Rq,1 	
	srwi 	Rt,Rn,31 	# add 1 to q if
	add 	Rq,Rq,Rt 	# n is negative
			#
	mulli 	Rt,Rq,5 	# compute remainder from
	sub 	Rr,Rn,Rt 	# r = n - q*5



Figure 3-30. Signed Divide by 7 Code Sequence
	lis 	Rm,0x9249 	# load magic number = m
	addi 	Rm,Rm,0x2493 	# m = 0x92492493 = (234 + 5)/7 - 232
	mulhw 	Rq,Rm,Rn 	# q = floor(m*n/232)
	add 	Rq,Rq,Rn 	# q = floor(m*n/232) + n
	srawi 	Rq,Rq,2 	# q = floor(q/4)
	srwi 	Rt,Rn,31 	# add 1 to q if
	add 	Rq,Rq,Rt 	# n is negative
			#
	mulli 	Rt,Rq,7 	# compute remainder from
	sub 	Rr,Rn,Rt 	# r = n - q*7

The general method is:

    Multiply n by a certain magic number.
    Obtain the high-order half of the product and shift it right some number of positions from 0 to 31.
    Add 1 if n is negative. 

The general method always reduces to one of the cases illustrated by divisors of 3, 5, and 7. In the case of division by 3, the multiplier is representable in 32 bits, and the shift amount is 0. In the case of division by 5, the multiplier is again representable in 32 bits, but the shift amount is 1. In the case of 7, the multiplier is not representable in 32 bits, but the multiplier less 232 is representable in 32 bits. Therefore, the code multiplies by the multiplier less 232, and then corrects the product by adding n ×232, that is by adding n to the high-order half of the product. For d = 7, the shift amount is 2.

For most divisors, there exists more than one multiplier that gives the correct result with this method. It is generally desirable, however, to use the minimum multiplier because this sometimes results in a zero shift amount and the saving of an instruction.

The corresponding procedure for dividing by a negative constant is analogous. Because signed integer division satisfies the equality n/(-d) = -(n/d), one method involves the generation of code for division by the absolute value of d followed by negation. It is possible, however, to avoid the negation, as illustrated by the code in Figure 3-31 for the case of d = -7. This approach does not give the correct result for d = -231, but for this case and other divisors that are negative powers of 2, you may use the code described previously for division by a positive power of 2, followed by negation.



Figure 3-31. Signed Divide by -7 Code Sequence
	lis 	Rm,0x6DB7 	# load magic number = m
	addi 	Rm,Rm,0xDB6D 	# m = 0x6DB6DB6D = -(234 + 5)/7 +232
	mulhw 	Rq,Rm,Rn 	# q = floor(m*n/232)
	sub 	Rq,Rq,Rn 	# q = floor(m*n/232) - n
	srawi 	Rq,Rq,2 	# q = floor(q/4)
	srwi 	Rt,Rq,31 	# add 1 to q if
	add 	Rq,Rq,Rt 	# q is negative (n is positive)
			#
	mulli 	Rt,Rq,-7 	# compute remainder from
	sub 	Rr,Rn,Rt 	# r = n - q*(-7)

The code in Figure 3-31 is the same as that for division by +7, except that it uses the multiplier of the opposite sign, subtracts rather than adds following the multiply, and shifts q rather than n to the right by 31. (The case of d = +7 could also shift q to the right by 31, but there would be less parallelism in the code.)

The multiplier for -d is nearly always the negative of the multiplier for d. For 32-bit operations, the only exceptions to this rule are d = 3 and 715,827,883.

Unsigned Division
Perform unsigned division by a power of 2 using a srwi instruction (a form of rlwinm). For other divisors, except 0 and 1, Figures 3-32 and 3-33 illustrate the two cases that arise.



Figure 3-32. Unsigned Divide by 3 Code Sequence
	lis 	Rm,0xAAAB 	# load magic number = m
	addi 	Rm,Rm,0xAAAB 	# m = 0xAAAAAAAB = (233 + 1)/3
	mulhwu 	Rq,Rm,Rn 	# q = floor(m*n/232)
	srwi 	Rq,Rq,1 	# q = q/2
			#
	mulli 	Rt,Rq,3 	# compute remainder from
	sub 	Rr,Rn,Rt 	# r = n - q*3



Figure 3-33. Unsigned Divide by 7 Code Sequence
	lis 	Rm,0x2492 	# load magic number = m
	addi 	Rm,Rm,0x4925 	# m = 0x24924925 = (235 + 3)/7 - 232
	mulhwu 	Rq,Rm,Rn 	# q = floor(m*n/232)
	sub 	Rt,Rn,Rq 	# t = n - q
	srwi 	Rt,Rt,1 	# t = (n - q)/2
	add 	Rt,Rt,Rq 	# t = (n - q)/2 + q = (n + q)/2
	srwi 	Rq,Rt,2 	# q = (n + m*n/232)/8 = floor(n/7)
			#
	mulli 	Rt,Rq,7 	# compute remainder from
	sub 	Rr,Rn,Rt 	# r = n - q*7

The quotient is

(m ×n)/2p,

where m is the magic number (e.g., (235 + 3)/7 in the case of division by 7), n is the dividend, p 32, and the "/" denotes unsigned integer (truncating) division. The multiply high of c and n yields (c× n)/232, so we can rewrite the quotient as

[(m ×n)/232]/2s,

where s 0.

In many cases, m is too large to represent in 32 bits, but m is always less than 233. For those cases in which m 232, we may rewrite the computation as

[((m - 232) ×n)/232 + n]/2s,

which is of the form (x + n)/2s, and the addition may cause an overflow. If the PowerPC architecture had a Shift Right instruction in which the Carry bit participated, that would be useful here. This instruction is not available, but the computation may be done without causing an overflow by rearranging it:

[(n - x)/2 + x]/2s-1,

where x = [(m - 232)n]/232. This expression does not overflow, and s > 0 when c 232. The code for division by 7 in Figure 3-33 uses this rearrangement.

If the shift amount is zero, the srwi instruction can be omitted, but a shift amount of zero occurs only rarely. For 32-bit operations, the code illustrated in the divide by 3 example has a shift amount of zero only for d = 641 and 6,700,417. For 64-bit operations, the analogous code has a shift amount of zero only for d = 274,177 and 67,280,421,310,721.

Sample Magic Numbers
The C code sequences in Figures 3-34 and 3-35 produce the magic numbers and shift values for signed and unsigned divisors, respectively. The derivation of these algorithms is beyond the scope of this book, but it is given in Warren [1992] and Granlund and Montgomery [1994].



Figure 3-34. Signed Division Magic Number Computation Code Sequence
	struct ms {int m; /* magic number */
	int s; }; /* shift amount */
	
	struct ms magic(int d)
	/* must have 2 <= d <= 231-1 or -231 <= d <= -2 */
	{
	int p;
	unsigned int ad, anc, delta, q1, r1, q2, r2, t;
	const unsigned int two31 = 2147483648; /* 231 */
	struct ms mag;
	
	ad = abs(d);
	t = two31 + ((unsigned int)d >> 31);
	anc = t - 1 - t%ad; /* absolute value of nc */
	p = 31; /* initialize p */
	q1 = two31/anc; /* initialize q1 = 2p/abs(nc) */
	r1 = two31 - q1*anc; /* initialize r1 = rem(2p,abs(nc)) */
	q2 = two31/ad; /* initialize q2 = 2p/abs(d) */
	r2 = two31 - q2*ad; /* initialize r2 = rem(2p,abs(d)) */
	do {
	p = p + 1;
	q1 = 2*q1; /* update q1 = 2p/abs(nc) */
	r1 = 2*r1; /* update r1 = rem(2p/abs(nc)) */
	if (r1 >= anc) { /* must be unsigned comparison */
	q1 = q1 + 1;
	r1 = r1 - anc;
	}
	q2 = 2*q2 /* update q2 = 2p/abs(d) */
	r2 = 2*r2 /* update r2 = rem(2p/abs(d)) */
	if (r2 >= ad) { /* must be unsigned comparison */
	q2 = q2 + 1;
	r2 = r2 - ad;
	}
	delta = ad - r2;
	} while (q1 < delta || (q1 == delta && r1 == 0));
			
	mag.m = q2 + 1;
	if (d < 0) mag.m = -mag.m; /* resulting magic number */
	mag.s = p - 32; /* resulting shift */
	return mag;
	}



Figure 3-35. Unsigned Division Magic Number Computation Code Sequence
	struct mu {unsigned int m; /* magic number */
	int a; /* "add" indicator */
	int s;} /* shift amount */
	
	struct mu magicu(unsigned int d)
	/* must have 1 <= d <= 232-1 */
	{
	int p;
	unsigned int nc, delta, q1, r1, q2, r2;
	struct mu magu;
	
	magu.a = 0; /* initialize "add" indicator */
	nc = - 1 - (-d)%d;
	p = 31; /* initialize p */
	q1 = 0x80000000/nc; /* initialize q1 = 2p/nc */
	r1 = 0x80000000 - q1*nc; /* initialize r1 = rem(2p,nc) */
	q2 = 0x7FFFFFFF/d; /* initialize q2 = (2p-1)/d */
	r2 = 0x7FFFFFFF - q2*d; /* initialize r2 = rem((2p-1),d) */
	do {
	p = p + 1;
	if (r1 >= nc - r1 ) {
	q1 = 2*q1 + 1; /* update q1 */
	r1 = 2*r1 - nc; /* update r1 */
	}
	else {
	q1 = 2*q1; /* update q1 */
	r1 = 2*r1; /* update r1 */
	}
	if (r2 + 1 >= d - r2) {
	if (q2 >= 0x7FFFFFFF) magu.a = 1;
	q2 = 2*q2 + 1; /* update q2 */
	r2 = 2*r2 + 1 - d; /* update r2 */
	}
	else {
	if (q2 >= 0x80000000) magu.a = 1;
	q2 = 2*q2; /* update q2 */
	r2 = 2*r2 + 1; /* update r2 */
	}
	delta = d - 1 - r2;
	} while (p < 64 && (q1 < delta || (q1 == delta && r1 == 0)));
	
	magu.m = q2 + 1; /* resulting magic number */
	mag.s = p - 32; /* resulting shift */
	return magu;
	}

Even if a compiler includes these functions to calculate the magic numbers, it may also incorporate a table of magic numbers for a few small divisors. Figure 3-36 shows an example of such a table. Magic numbers and shift amounts for divisors that are negative or are powers of 2 are shown just as a matter of general interest; a compiler would probably not include them in its tables. Figure 3-37 shows the analogous table for 64-bit operations.

The tables need not include even divisors because other techniques handle them better. If the divisor d is of the form b ×2k, where b is odd, the magic number for d is the same as that for b, and the shift amount is the shift for b increased by k. This procedure does not always give the minimum magic number, but it nearly always does. For example, the magic number for 10 is the same as that for 5, and the shift amount for 10 is 1 plus the shift amount for 5.



Figure 3-36. Some Magic Numbers for 32-Bit Operations
d 	signed 	unsigned
m (hex) 	s 	m (hex) 	a 	s
-5 	99999999 	1 			
-3 	55555555 	1 			
-2k 	7FFFFFFF 	k-1 			
1 	— 	— 	0 	1 	0
2k 	80000001 	k-1 	232-k 	0 	0
3 	55555556 	0 	AAAAAAAB 	0 	1
5 	66666667 	1 	CCCCCCCD 	0 	2
6 	2AAAAAAB 	0 	AAAAAAAB 	0 	2
7 	92492493 	2 	24924925 	1 	3
9 	38E38E39 	1 	38E38E39 	0 	1
10 	66666667 	2 	CCCCCCCD 	0 	3
11 	2E8BA2E9 	1 	BA2E8BA3 	0 	3
12 	2AAAAAAB 	1 	AAAAAAAB 	0 	3
25 	51EB851F 	3 	51EB851F 	0 	3
125 	10624DD3 	3 	10624DD3 	0 	3



Figure 3-37. Some Magic Numbers for 64-Bit Operations
d 	signed 	unsigned
m (hex) 	s 	m (hex) 	a 	s
-5 	9999999999999999 	1 			
-3 	5555555555555555 	1 			
-2k 	7FFFFFFFFFFFFFFF 	k-1 			
1 	— 	— 	0 	1 	0
2k 	8000000000000001 	k-1 	264-k 	0 	0
3 	5555555555555556 	0 	AAAAAAAAAAAAAAAB 	0 	1
5 	6666666666666667 	1 	CCCCCCCCCCCCCCCD 	0 	2
6 	2AAAAAAAAAAAAAAB 	0 	AAAAAAAAAAAAAAAB 	0 	2
7 	4924924924924925 	1 	2492492492492493 	1 	3
9 	1C71C71C71C71C72 	0 	E38E38E38E38E38F 	0 	3
10 	6666666666666667 	2 	CCCCCCCCCCCCCCCD 	0 	3
11 	2E8BA2E8BA2E8BA3 	1 	2E8BA2E8BA2E8BA3 	0 	1
12 	2AAAAAAAAAAAAAAB 	1 	AAAAAAAAAAAAAAAB 	0 	3
25 	A3D70A3D70A3D70B 	4 	47AE147AE147AE15 	1 	5
125 	20C49BA5E353F7CF 	4 	0624DD2F1A9FBE77 	1 	7

In the special case when the magic number is even, divide the magic number by 2 and reduce the shift amount by 1. The resulting shift might be 0 (as in the case of signed division by 6), saving an instruction.

To use the values in Figure 3-36 to replace signed division:

    Load the magic value.
    Multiply the numerator by the magic value with the mulhw instruction.
    If d > 0 and m < 0, add n. 

If d < 0 and m > 0, subtract n.

    Shift s places to the right with the srawi instruction.
    Add the sign bit extracted with the srwi instruction. 

To use the values in Figure 3-37 to replace unsigned division:

    Load the magic value.
    Multiply the numerator by the magic value with the mulhwu instruction.
    If a = 0, shift s places to the right with the srwi instruction.
    If a = 1,
        Subtract q from n.
        Shift to the right 1 place with the srwi instruction.
        Add q.
        Shift s - 1 places to the right with the srwi instruction. 

It can be shown that s - 1 0, except in the degenerate case d = 1, for which this technique is not recommended.

If your divisor happens to be relatively prime with 2 and you know that it divides the dividend with no remainder, then you can do the division with a single multiplication. For example, suppose you have some multiple of 13 (e.g. 13*a) that you want to divide by 13. All you have to do is multiply by the magic number 3303820997.

13*a*3303820997 = 42949672961*a = (10*2^32 + 1)*a

With 32 bit integers, 2^32 = 0, so 10*2^32 = 0. Therefore you’re left with a. Unfortunately, if the division has a remainder then this trick won’t work: 11/13 != 11*3303820997 = 1982292599. This whole trick is based on the extended euclidean algorithm, which says that given integers a and b you can find integers u and v such that

a*u + b*v = gcd(a, b)

In this case, a = 13, b = 2^32, u = 3303820997, v = -10 and gcd(a, b) = 1.


Every divisor has a magic number, and most have more than one! A magic number for d is nothing more than a precomputed quotient: a power of 2 divided by d and then rounded up. At runtime, we do the same thing, except backwards: multiply by this magic number and then divide by the power of 2, rounding down. The tricky part is finding a power big enough that the "rounding up" part doesn't hurt anything. If we are lucky, a multiple of d will happen to be only slightly larger than a power of 2, so rounding up doesn't change much and our magic number will fit in 32 bits. If we are unlucky, well, we can always fall back to a 33 bit number, which is almost as efficient.

I humbly acknowledge the legendary Guy Steele Henry Warren (must have been a while) and his book Hacker's Delight, which introduced me to this line of proof. (Except his version has, you know, rigor.)


 Generating Random Numbers using LFSRs

On a recent project I needed a simple random number generator. It didn't need to be anything fancy, nothing mathematically pure or statistically valid, just a quick'n'dirty little generator that would spit out numbers that looked random to the casual eye.

In a previous incarnation I worked for a mobile radio consultancy. One of the things I came across in that particular life was the concept of Pseudo Random Binary Sequences (PRBSs).

These are sequences of 0s and 1s that are generated deterministically, yet are random enough for many practical purposes. They are used extensively in communications to spread the bandwidth occupied by a signal. This makes the signal more immune to jamming, as the signal is smeared across a bigger chunk of spectrum than would otherwise be the case. It is also more difficult to intercept, since any receiver must know and be synchronised to the spreading sequence at the transmitter. A more subtle usage is that multiple signals can be spread across the same spectrum by multiple PRBSs, yet be teased apart at multiple receivers by correlation with the correct individual spreading sequences.

PRBSs are typically generated using Linear Feedback Shift Registers (LFSRs). These are cheap and easy to build in hardware, and easy to code in software.

At any time, a function of the LFSR contents is generated as the output bit value. This is fed back into the shift register as the next input. The output bit sequence is essentially random, as is the sequence of states formed by the LFSR contents.

The 'function of the LFSR contents' mentioned above is called a generator polynomial. It determines which LFSR cells are combined and fed back to form the LFSR input for the next state.

For instance, the Wikipedia article on LFSRs lists the following generator polynomial for an 8-bit PRBS:

x^8 + x^6 + x^5 + x^4 + 1

We now need to know how to convert this into code.

As we are generating an 8-bit polynomial, we start off with the 8-bit unsigned value 0x00u as our feedback function. If a power of x, say m, is used in the polynomial, assign a 1 to polynomial bit (m-1). The +1 in the polynomial corresponds to the output bit, and does not appear in the feedback settings. Here we have an 8-bit generator polynomial with bits 7, 5, 4, and 3 set, giving a value of 0xB8u.

Given this, and also using the example code in the Wikipedia article, we can now write a function that returns the next value in a pseudo-random 8-bit sequence.

#define LFSR_8_INITIAL_VALUE 0x01u
#define LFSR_8_POLYNOMIAL    0xB8u

uint8_t next_lfsr_8( void )
{
 /* seed LFSR */
 static uint8_t lfsr = LFSR_8_INITIAL_VALUE;

 /* get LFSR LSB */
 uint8_t lsb = (uint8_t)( lfsr & 0x01u );

 /* shift LFSR contents */
 lfsr = (uint8_t)( lfsr >> 1u );

 /* toggle feedback taps if we output a 1 */
 if( 1 == lsb )
 {
    lfsr ^= LFSR_8_POLYNOMIAL;
 }

 return lfsr;
}


An n-bit PRBS is said to be maximal length if the number of distinct output values it generates is 2^n - 1. That is, a maximal length 8-bit PRBS consists of 255 values. Our generator polynomial is indeed maximal length, and so the values returned will start to repeat after 255 calls.

In the above code I have set the initial value of the LFSR to 0x01u. The LFSR will generate a fixed sequence of output values, and the initial value merely determines where in the sequence the reported values will start.

Note that because the feedback taps uses the XOR operator, the LFSR will get stuck in the all-zeros state if it ever gets there. Care must be taken that an LFSR is not initialised with this value.

The corresponding 16-bit function is as follows.

#define LFSR_16_INITIAL_VALUE 0x0001u
#define LFSR_16_POLYNOMIAL    0xB400u

uint16_t next_lfsr_16( void )
{
 /* seed LFSR */
 static uint16_t lfsr = LFSR_16_INITIAL_VALUE;

 /* get LFSR LSB */
 uint8_t lsb = (uint8_t)( lfsr & 0x0001u );

 /* shift LFSR contents */
 lfsr = (uint16_t)( lfsr >> 1u );

 /* toggle feedback taps if we output a 1 */
 if( 1 == lsb )
 {
    lfsr ^= LFSR_16_POLYNOMIAL;
 }

 return lfsr;
}

Using the above approach, you can generate PRBSs of arbitrary length, given tables of generator polynomials (your search engine of choice will unearth plenty).
