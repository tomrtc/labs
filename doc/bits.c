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
