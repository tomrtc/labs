The asker has self-answered their question (a class assignment), so providing alternative solutions seems appropriate at this time. The question clearly assumes that integers are represented as two's complement numbers.

One approach is to consider how CPUs compute predicates for conditional branching by means of a compare instruction. "signed less than" as expressed in processor condition codes is SF ≠ OF. SF is the sign flag, a copy of the sign-bit, or most significant bit (MSB) of the result. OF is the overflow flag which indicates overflow in signed integer operations. This is computed as the XOR of the carry-in and the carry-out of the sign-bit or MSB. With two's complement arithmetic, a - b = a + ~b + 1, and therefore a < b = a + ~b < 0. It remains to separate computation on the sign bit (MSB) sufficiently from the lower order bits. This leads to the following code:

int isLessOrEqual (int a, int b)
{
    int nb = ~b;
    int ma = a  & ((1U << (sizeof(a) * CHAR_BIT - 1)) - 1);
    int mb = nb & ((1U << (sizeof(b) * CHAR_BIT - 1)) - 1);
    // for the following, only the MSB is of interest, other bits are don't care
    int cyin = ma + mb;
    int ovfl = (a ^ cyin) & (a ^ b);
    int sign = (a ^ nb ^ cyin);
    int lteq = sign ^ ovfl;
    // desired predicate is now in the MSB (sign bit) of lteq, extract it
    return (int)((unsigned int)lteq >> (sizeof(lteq) * CHAR_BIT - 1));
}

The casting to unsigned int prior to the final right shift is necessary because right-shifting of signed integers with negative value is implementation-defined, per the ISO-C++ standard, section 5.8. Asker has pointed out that casts are not allowed. When right shifting signed integers, C++ compilers will generate either a logical right shift instruction, or an arithmetic right shift instruction. As we are only interested in extracting the MSB, we can isolate ourselves from the choice by shifting then masking out all other bits besides the LSB, at the cost of one additional operation:

    return (lteq >> (sizeof(lteq) * CHAR_BIT - 1)) & 1;

The above solution requires a total of eleven or twelve basic operations. A significantly more efficient solution is based on the 1972 MIT HAKMEM memo, which contains the following observation:

    ITEM 23 (Schroeppel): (A AND B) + (A OR B) = A + B = (A XOR B) + 2 (A AND B).

This is straightforward, as A AND B represent the carry bits, and A XOR B represent the sum bits. In a newsgroup posting to comp.arch.arithmetic on February 11, 2000, Peter L. Montgomery provided the following extension:

    If XOR is available, then this can be used to average two unsigned variables A and B when the sum might overflow:

     (A+B)/2 = (A AND B) + (A XOR B)/2

In the context of this question, this allows us to compute (a + ~b) / 2 without overflow, then inspect the sign bit to see if the result is less than zero. While Montgomery only referred to unsigned integers, the extension to signed integers is straightforward by use of an arithmetic right shift, keeping in mind that right shifting is an integer division which rounds towards negative infinity, rather than towards zero as regular integer division.

int isLessOrEqual (int a, int b)
{
    int nb = ~b;
    // compute avg(a,~b) without overflow, rounding towards -INF; lteq(a,b) = SF
    int lteq = (a & nb) + arithmetic_right_shift (a ^ nb, 1);
    return (int)((unsigned int)lteq >> (sizeof(lteq) * CHAR_BIT - 1));
}

Unfortunately, C++ itself provides no portable way to code an arithmetic right shift, but we can emulate it fairly efficiently using this answer:

int arithmetic_right_shift (int a, int s)
{
    unsigned int mask_msb = 1U << (sizeof(mask_msb) * CHAR_BIT - 1);
    unsigned int ua = a;
    ua = ua >> s;
    mask_msb = mask_msb >> s;
    return (int)((ua ^ mask_msb) - mask_msb);
}

When inlined, this adds just a couple of instructions to the code when the shift count is a compile-time constant. If the compiler documentation indicates that the implementation-defined handling of signed integers of negative value is accomplished via arithmetic right shift instruction, it is safe to simplify to this six-operation solution:

int isLessOrEqual (int a, int b)
{
    int nb = ~b;
    // compute avg(a,~b) without overflow, rounding towards -INF; lteq(a,b) = SF
    int lteq = (a & nb) + ((a ^ nb) >> 1);
    return (int)((unsigned int)lteq >> (sizeof(lteq) * CHAR_BIT - 1));
}

The previously made comments regarding use of a cast when converting the sign bit into a predicate apply here as well.
order-statistics tree


In various contexts, such as bioinformatics, computations on byte-size integers is sufficient. For best performance, many processor architectures offer SIMD instruction sets (e.g. MMX, SSE, AVX) which partition registers into byte-, halfword-, and word-sized components, then perform arithmetic, logical, and shift operations individually on corresponding components.

However, some architecture do not offer such SIMD instructions, requiring them to be emulated, which often requires a significant amount of bit-twiddling. At the moment, I am looking at SIMD comparisons, and in particular the parallel comparison of signed, byte-sized, integers. I have a solution that I think is quite efficient using portable C code (see the function vsetles4 below). It is based on an observation made in year 2000 by Peter Montgomery in a newsgroup posting, that (A+B)/2 = (A AND B) + (A XOR B)/2 without overflow in intermediate computation.

Can this particular emulation code (function vsetles4) be accelerated further? To first order any solution with a lower count of basic operations would qualify. I am looking for solutions in portable ISO-C99, without use of machine-specific intrinsics. Most architectures support ANDN (a & ~b), so this may be assumed to be available as a single operation in terms of efficiency.

#include <stdint.h>

/*
   vsetles4 treats its inputs as arrays of bytes each of which comprises
   a signed integers in [-128,127]. Compute in byte-wise fashion, between
   corresponding bytes of 'a' and 'b', the boolean predicate "less than
   or equal" as a value in [0,1] into the corresponding byte of the result.
*/

/* reference implementation */
uint32_t vsetles4_ref (uint32_t a, uint32_t b)
{
    uint8_t a0 = (uint8_t)((a >>  0) & 0xff);
    uint8_t a1 = (uint8_t)((a >>  8) & 0xff);
    uint8_t a2 = (uint8_t)((a >> 16) & 0xff);
    uint8_t a3 = (uint8_t)((a >> 24) & 0xff);
    uint8_t b0 = (uint8_t)((b >>  0) & 0xff);
    uint8_t b1 = (uint8_t)((b >>  8) & 0xff);
    uint8_t b2 = (uint8_t)((b >> 16) & 0xff);
    uint8_t b3 = (uint8_t)((b >> 24) & 0xff);
    int p0 = (int32_t)(int8_t)a0 <= (int32_t)(int8_t)b0;
    int p1 = (int32_t)(int8_t)a1 <= (int32_t)(int8_t)b1;
    int p2 = (int32_t)(int8_t)a2 <= (int32_t)(int8_t)b2;
    int p3 = (int32_t)(int8_t)a3 <= (int32_t)(int8_t)b3;

    return (((uint32_t)p3 << 24) | ((uint32_t)p2 << 16) |
            ((uint32_t)p1 <<  8) | ((uint32_t)p0 <<  0));
}

/* Optimized implementation:

   a <= b; a - b <= 0;  a + ~b + 1 <= 0; a + ~b < 0; (a + ~b)/2 < 0.
   Compute avg(a,~b) without overflow, rounding towards -INF; then
   lteq(a,b) = sign bit of result. In other words: compute 'lteq' as
   (a & ~b) + arithmetic_right_shift (a ^ ~b, 1) giving the desired
   predicate in the MSB of each byte.
*/
uint32_t vsetles4 (uint32_t a, uint32_t b)
{
    uint32_t m, s, t, nb;
    nb = ~b;            // ~b
    s = a & nb;         // a & ~b
    t = a ^ nb;         // a ^ ~b
    m = t & 0xfefefefe; // don't cross byte boundaries during shift
    m = m >> 1;         // logical portion of arithmetic right shift
    s = s + m;          // start (a & ~b) + arithmetic_right_shift (a ^ ~b, 1)
    s = s ^ t;          // complete arithmetic right shift and addition
    s = s & 0x80808080; // MSB of each byte now contains predicate
    t = s >> 7;         // result is byte-wise predicate in [0,1]
    return t;
}
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

long opt_R;
long opt_N;

void *aptr;
void *bptr;
void *cptr;

/*
   vsetles4 treats its inputs as arrays of bytes each of which comprises
   a signed integers in [-128,127]. Compute in byte-wise fashion, between
   corresponding bytes of 'a' and 'b', the boolean predicate "less than
   or equal" as a value in [0,1] into the corresponding byte of the result.
*/

/* base implementation */
void
vsetles4_base(const void *va, const void *vb, long count, void *vc)
{
    const char *aptr;
    const char *bptr;
    char *cptr;
    long idx;

    count *= 4;
    aptr = va;
    bptr = vb;
    cptr = vc;

    for (idx = 0;  idx < count;  ++idx)
        cptr[idx] = (aptr[idx] <= bptr[idx]);
}

/* reference implementation */
static inline uint32_t
_vsetles4_ref(uint32_t a, uint32_t b)
{
    uint8_t a0 = (uint8_t)((a >>  0) & 0xff);
    uint8_t a1 = (uint8_t)((a >>  8) & 0xff);
    uint8_t a2 = (uint8_t)((a >> 16) & 0xff);
    uint8_t a3 = (uint8_t)((a >> 24) & 0xff);
    uint8_t b0 = (uint8_t)((b >>  0) & 0xff);
    uint8_t b1 = (uint8_t)((b >>  8) & 0xff);
    uint8_t b2 = (uint8_t)((b >> 16) & 0xff);
    uint8_t b3 = (uint8_t)((b >> 24) & 0xff);

    int p0 = (int32_t)(int8_t)a0 <= (int32_t)(int8_t)b0;
    int p1 = (int32_t)(int8_t)a1 <= (int32_t)(int8_t)b1;
    int p2 = (int32_t)(int8_t)a2 <= (int32_t)(int8_t)b2;
    int p3 = (int32_t)(int8_t)a3 <= (int32_t)(int8_t)b3;

    return (((uint32_t)p3 << 24) | ((uint32_t)p2 << 16) |
            ((uint32_t)p1 <<  8) | ((uint32_t)p0 <<  0));
}

uint32_t
vsetles4_ref(uint32_t a, uint32_t b)
{

    return _vsetles4_ref(a,b);
}

/* Optimized implementation:
   a <= b; a - b <= 0;  a + ~b + 1 <= 0; a + ~b < 0; (a + ~b)/2 < 0.
   Compute avg(a,~b) without overflow, rounding towards -INF; then
   lteq(a,b) = sign bit of result. In other words: compute 'lteq' as
   (a & ~b) + arithmetic_right_shift (a ^ ~b, 1) giving the desired
   predicate in the MSB of each byte.
*/
static inline uint32_t
_vsetles4(uint32_t a, uint32_t b)
{
    uint32_t m, s, t, nb;
    nb = ~b;            // ~b
    s = a & nb;         // a & ~b
    t = a ^ nb;         // a ^ ~b
    m = t & 0xfefefefe; // don't cross byte boundaries during shift
    m = m >> 1;         // logical portion of arithmetic right shift
    s = s + m;          // start (a & ~b) + arithmetic_right_shift (a ^ ~b, 1)
    s = s ^ t;          // complete arithmetic right shift and addition
    s = s & 0x80808080; // MSB of each byte now contains predicate
    t = s >> 7;         // result is byte-wise predicate in [0,1]
    return t;
}

uint32_t
vsetles4(uint32_t a, uint32_t b)
{

    return _vsetles4(a,b);
}

/* Optimized implementation:
   a <= b; a - b <= 0;  a + ~b + 1 <= 0; a + ~b < 0; (a + ~b)/2 < 0.
   Compute avg(a,~b) without overflow, rounding towards -INF; then
   lteq(a,b) = sign bit of result. In other words: compute 'lteq' as
   (a & ~b) + arithmetic_right_shift (a ^ ~b, 1) giving the desired
   predicate in the MSB of each byte.
*/
static inline uint64_t
_vsetles8(uint64_t a, uint64_t b)
{
    uint64_t m, s, t, nb;
    nb = ~b;            // ~b
    s = a & nb;         // a & ~b
    t = a ^ nb;         // a ^ ~b
    m = t & 0xfefefefefefefefell; // don't cross byte boundaries during shift
    m = m >> 1;         // logical portion of arithmetic right shift
    s = s + m;          // start (a & ~b) + arithmetic_right_shift (a ^ ~b, 1)
    s = s ^ t;          // complete arithmetic right shift and addition
    s = s & 0x8080808080808080ll; // MSB of each byte now contains predicate
    t = s >> 7;         // result is byte-wise predicate in [0,1]
    return t;
}

uint32_t
vsetles8(uint64_t a, uint64_t b)
{

    return _vsetles8(a,b);
}

void
aryref(const void *va,const void *vb,long count,void *vc)
{
    long idx;
    const uint32_t *aptr;
    const uint32_t *bptr;
    uint32_t *cptr;

    aptr = va;
    bptr = vb;
    cptr = vc;

    for (idx = 0;  idx < count;  ++idx)
        cptr[idx] = _vsetles4_ref(aptr[idx],bptr[idx]);
}

void
arybest4(const void *va,const void *vb,long count,void *vc)
{
    long idx;
    const uint32_t *aptr;
    const uint32_t *bptr;
    uint32_t *cptr;

    aptr = va;
    bptr = vb;
    cptr = vc;

    for (idx = 0;  idx < count;  ++idx)
        cptr[idx] = _vsetles4(aptr[idx],bptr[idx]);
}

void
arybest8(const void *va,const void *vb,long count,void *vc)
{
    long idx;
    const uint64_t *aptr;
    const uint64_t *bptr;
    uint64_t *cptr;

    count >>= 1;

    aptr = va;
    bptr = vb;
    cptr = vc;

    for (idx = 0;  idx < count;  ++idx)
        cptr[idx] = _vsetles8(aptr[idx],bptr[idx]);
}

double
tvgetf(void)
{
    struct timespec ts;
    double sec;

    clock_gettime(CLOCK_REALTIME,&ts);
    sec = ts.tv_nsec;
    sec /= 1e9;
    sec += ts.tv_sec;

    return sec;
}

void
timeit(void (*fnc)(const void *,const void *,long,void *),const char *sym)
{
    double tvbeg;
    double tvend;

    tvbeg = tvgetf();
    fnc(aptr,bptr,opt_N,cptr);
    tvend = tvgetf();

    printf("timeit: %.9f %s\n",tvend - tvbeg,sym);
}

// fill -- fill array with random numbers
void
fill(void *vptr)
{
    uint32_t *iptr = vptr;

    for (long idx = 0;  idx < opt_N;  ++idx)
        iptr[idx] = rand();
}

// main -- main program
int
main(int argc,char **argv)
{
    char *cp;

    --argc;
    ++argv;

    for (;  argc > 0;  --argc, ++argv) {
        cp = *argv;
        if (*cp != '-')
            break;

        switch (cp[1]) {
        case 'R':
            opt_R = strtol(cp + 2,&cp,10);
            break;

        case 'N':
            opt_N = strtol(cp + 2,&cp,10);
            break;

        default:
            break;
        }
    }

    if (opt_R == 0)
        opt_R = 1;
    srand(opt_R);
    printf("R=%ld\n",opt_R);

    if (opt_N == 0)
        opt_N = 100000000;
    printf("N=%ld\n",opt_N);

    aptr = calloc(opt_N,sizeof(uint32_t));
    bptr = calloc(opt_N,sizeof(uint32_t));
    cptr = calloc(opt_N,sizeof(uint32_t));

    fill(aptr);
    fill(bptr);

    timeit(vsetles4_base,"base");
    timeit(aryref,"aryref");
    timeit(arybest4,"arybest4");
    timeit(arybest8,"arybest8");
    timeit(vsetles4_base,"base");

    return 0;
}




I've got an application that needs to send a stream of data from one process to multiple readers, each of which needs to see its own copy of the stream. This is reasonably high-rate (100MB/s is not uncommon), so I'd like to avoid duplication if possible. In my ideal world, linux would have named pipes that supported multiple readers, with a fast path for the common single-reader case.

I'd like something that provides some measure of namespace isolation (eg: broadcasting on 127.0.0.1 is open to any process I believe...). Unix domain sockets don't support broadcast, and UDP is "unreliable" anyways (server will drop packets instead of blocking in my case). I supposed I could create a shared-memory segment and store the common buffers there, but that feels like reinventing the wheel. Is there a canonical way to do this in linux?

    I supposed I could create a shared-memory segment and store the common buffers there, but that feels like reinventing the wheel. Is there a canonical way to do this in linux?

The short answer: No

The long answer: Yes [and you're on the right track]

I've had to do this before [for even higher speeds], so I had to research this. The following is what I came up with.

In the main process, create a pool of shared buffers [use SysV shm or private mmap as you chose]. Assign ID numbers to them (e.g. 1,2,3,...). Now there is a mapping from bufid to buffer memory address. To make this accessible to child processes, do this before you fork them. The children also inherit the shared memory mappings, so not much work

Now fork the children. Give them each a unique process id. You can just incrementally start with a number: 2,3,4,... [main is 1] or just use regular pids.

Open up a SysV msg channel (msgget et. al.). Again, if you do this in the main process before the fork, they are available to the children [IIRC].

Now here's how it works:

main finds an unused buffer and fills it. For each child, main sends an IPC message via msgsnd (on the single common IPC channel) where the message payload [mtext] is the bufid number. Each message has the standard header's mtype field set to the destination child's pid.

After doing this, main remembers the buffer as "in flight" and not yet reusable.

Each child does a msgrcv with the mtype set to its pid. It then extracts the bufid from mtext and processes the buffer. When it's done, it sends an IPC message [again on the same channel] with mtype set to main's pid with an mtext of the bufid it just processed.

main's loop does an non-blocking msgrcv, noting all "release" messages for a given bufid. When all children have released the buffer, it's put back on the buffer "free queue". In main's service loop, it may fill new buffers and send more messages as appropriate [intersperse with the waits].

The child then does an msgrcv and the cycle repeats.

So, we're using [large] shared memory buffers and short [a few bytes] bufid descriptor IPC messages.

Okay, so the question you may be asking: "Why SysV IPC for the comm channel?" [vs. multiple pipes or sockets].

You already know that a shared buffer avoids sending multiple copies of your data.

So, that's the way to go. But, why not send the above bufid messages across sockets or pipes [or shared queues, condition variables, mutexes, etc]?

The answer is speed and the wakeup characteristics of the target process.

For a highly realtime response, when main sends out the bufid messages, you want the target process [if it's been sleeping] to wake up immediately and start processing the buffer.

I examined the linux kernel source and the only mechanism that has that characteristic is SysV IPC. All others have a [scheduling] lag.

When process A does msgsnd on a channel that process B has done msgrcv on, three things will happen:

    process B will be marked runnable by the scheduler.
    [IIRC] B will be moved to the front of its scheduling queue
    Also, more importantly, this then causes an immediate reschedule of all processes.

B will start right away [as opposed to next timer interrupt or when some other process just happens to sleep]. On a single core machine, A will be put to sleep and B will run in its stead.

Caveat: All my research was done a few years back before the CFS scheduler, but, I believe the above should still hold. Also, I was using the RT scheduler, which may be a possible option if CFS doesn't work as intended.

UPDATE:

    Looking at the POSIX message queue source, I think that the same immediate-wakeup behavior you discussed with the System V queues is going on, which gives the added benefit of POSIX compatibility.

The timing semantics are possible [and desirable] so I wouldn't be surprised. But, SysV is actually more standard and ubiquitous than POSIX mqueues. And, there are some semantic differences [See below].

For timing, you can build a unit test program [just using msgs] with nsec timestamps. I used TSC stamps, but clock_gettime(CLOCK_REALTIME,...) might also work. Stamp departure time and arrival/wakeup time to see. Compare both SysV and mq

With either SysV or mq you may need to bump up the max # of msgs, max msg size, max # of queues via /proc/*. The default values are relatively small. If you don't, you may find tasks blocked waiting for a msg but master can't send one [is blocked] due to a msg queue maximum parameter being exceeded. I actually had such a bug, so I changed my code to bump up these values [it was running as root] during startup. So, you may need to do this as an RC boot script (or whatever the [atrocious ;-)] systemd equivalent is)

I looked at using mq to replace SysV in my own code. It didn't have the same semantics for a many-to-one return-to-free-pool msg. In my original answer, I had forgotten to mention that two msg queues are needed: master-to-children (e.g. work-to-do) and children-to-master (e.g. returning a now available buffer).

I had several different types of buffers (e.g. compressed video, compressed audio, uncompressed video, uncompressed audio) that had varying types and struct descriptors.

Also, multiple different buffer queues as these buffers got passed from thread to thread [different processing stages].

With SysV you can use a single msg queue for multiple buffer lists/queues, the buffer list ID is the msg mtype. A child msgrcv waits with mtype set to the ID value. The master waits on the return-to-free msg queue with mtype of 0.

mq* requires a separate mqd_t for each ID because it doesn't allow a wait on a msg subtype.

msgrcv allows IPC_NOWAIT on each call, but to get the same effect with mq_receive you have to open the queue with O_NONBLOCK or use the timed version. This gets used during the "shutdown" or "restart" phase (e.g. send a msg to children that no more data will arrive and they should terminate [or reconfigure, etc.]). The IPC_NOWAIT is handy for "draining" a queue during program startup [to get rid of stale messages from a prior invocation] or drain stale messages from a prior configuration during operation.

So, instead of just two SysV msg queues to handle an arbitrary number of buffer lists, you'll need a separate mqd_t for each buffer list/type.
// _prngstd -- get random number
static inline u32
_prngstd(prng_p prng)
{
    long rhs;
    u32 lhs;

    // NOTE: random is faster and has a _long_ period, but it _only_ produces
    // positive integers but jrand48 produces positive _and_ negative
#if 0
    rhs = jrand48(btc->btc_seed);
    lhs = rhs;
#endif

    // this has collisions
#if 0
    rhs = rand();
    PRNG_FLIP;
#endif

    // this has collisions because it defaults to TYPE_3
#if 0
    rhs = random();
    PRNG_FLIP;
#endif

    // this is random in TYPE_0 (linear congruential) mode
#if 0
    prng->prng_state = ((prng->prng_state * 1103515245) + 12345) & 0x7fffffff;
    rhs = prng->prng_state;
    PRNG_FLIP;
#endif

    // this is random in TYPE_0 (linear congruential) mode with the mask
    // removed to get full range numbers
    // this does _not_ produce overlaps
#if 1
    prng->prng_state = ((prng->prng_state * 1103515245) + 12345);
    rhs = prng->prng_state;
    lhs = rhs;
#endif

    return lhs;
}

Four options, all of which are O(1) in both memory and time:

    Just increment a global counter. Since you want uniqueness, you can't generate random numbers anyway.
    Generate a number from a set sufficiently large that it is highly improbable that a number repeats. 64 bits is sufficient for in-app uniqueness; 128 bits is sufficient for global uniqueness. This is how UUIDs work.
    If option 1 isn't "random" enough for you, use the CRC-32 hash of said global (32-bit) counter. There is a 1-to-1 mapping (bijection) between N-bit integers and their CRC-N so uniqueness will still be guaranteed.
    Generate numbers using a Linear Feedback Shift Register. These are N-bit counters which count in a seemingly "random" (though obviously deterministic) pattern. For your purposes, this would essentially be a modestly faster version of option 3.

How to combine CRC-32 results?
Ask Question
Asked 2 years, 3 months ago
Active 2 years, 3 months ago
Viewed 490 times
2

Let's say I have two numbers X = 0xABC; Y = 0xDE. I want to calculate CRC-32 over Z, which is X concatenated to Y i.e. Z = 0xABCDE.

I have CRC(X) and CRC(Y) available. How can I compute CRC(Z) ?
ake a look at crc32_combine() in zlib for the approach.

The basic idea is to use the linearity of CRCs. The CRC of 0xABC00 exclusive-or'ed with 0x000DE is the exclusive-or of their corresponding CRCs. If I ignore the pre and post-processing (which I can for reasons that are left to the reader to derive), leading zeros do not change the CRC, so the CRC of 0xDE is the same as the CRC of 0x000DE. So all I need to do is calculate the effect of appending zeros when starting with the CRC of 0xABC, in order to get the CRC of 0xABC00. Then exclusive-or those two CRCs.

crc32_combine() uses matrix multiplication to compute the effect of appending n zeros in O(log(n)) time instead of O(n) time, so combining CRCs is very fast regardless of the lengths of the original messages.The basic CRC algorithm is additive/linear. If you have two messages a and b of the same length, then CRC(a XOR b) = CRC(a) XOR CRC(b).

Furthermore, if you pad the message on the right side with n zeros, the new CRC will be the old CRC times x^n mod the CRC polynomial.



With all that said, the only way to solve your problem is to really understand the mathematics behind the CRC algorithm and write your own custom code. Here's a long but very complete explanation of CRCs: http://www.ross.net/crc/download/crc_v3.txt
A PAINLESS GUIDE TO CRC ERROR DETECTION ALGORITHMS
==================================================
"Everything you wanted to know about CRC algorithms, but were afraid
to ask for fear that errors in your understanding might be detected."

Version : 3.
Date    : 19 August 1993.
Author  : Ross N. Williams.
Net     : ross@guest.adelaide.edu.au.
FTP     : ftp.adelaide.edu.au/pub/rocksoft/crc_v3.txt
Company : Rocksoft^tm Pty Ltd.
Snail   : 16 Lerwick Avenue, Hazelwood Park 5066, Australia.
Fax     : +61 8 373-4911 (c/- Internode Systems Pty Ltd).
Phone   : +61 8 379-9217 (10am to 10pm Adelaide Australia time).
Note    : "Rocksoft" is a trademark of Rocksoft Pty Ltd, Australia.
Status  : Copyright (C) Ross Williams, 1993. However, permission is
          granted to make and distribute verbatim copies of this
          document provided that this information block and copyright
          notice is included. Also, the C code modules included
          in this document are fully public domain.
Thanks  : Thanks to Jean-loup Gailly (jloup@chorus.fr) and Mark Adler
          (me@quest.jpl.nasa.gov) who both proof read this document
          and picked out lots of nits as well as some big fat bugs.

Table of Contents
-----------------
    Abstract
 1. Introduction: Error Detection
 2. The Need For Complexity
 3. The Basic Idea Behind CRC Algorithms
 4. Polynomical Arithmetic
 5. Binary Arithmetic with No Carries
 6. A Fully Worked Example
 7. Choosing A Poly
 8. A Straightforward CRC Implementation
 9. A Table-Driven Implementation
10. A Slightly Mangled Table-Driven Implementation
11. "Reflected" Table-Driven Implementations
12. "Reversed" Polys
13. Initial and Final Values
14. Defining Algorithms Absolutely
15. A Parameterized Model For CRC Algorithms
16. A Catalog of Parameter Sets for Standards
17. An Implementation of the Model Algorithm
18. Roll Your Own Table-Driven Implementation
19. Generating A Lookup Table
20. Summary
21. Corrections
 A. Glossary
 B. References
 C. References I Have Detected But Haven't Yet Sighted


Abstract
--------
This document explains CRCs (Cyclic Redundancy Codes) and their
table-driven implementations in full, precise detail. Much of the
literature on CRCs, and in particular on their table-driven
implementations, is a little obscure (or at least seems so to me).
This document is an attempt to provide a clear and simple no-nonsense
explanation of CRCs and to absolutely nail down every detail of the
operation of their high-speed implementations. In addition to this,
this document presents a parameterized model CRC algorithm called the
"Rocksoft^tm Model CRC Algorithm". The model algorithm can be
parameterized to behave like most of the CRC implementations around,
and so acts as a good reference for describing particular algorithms.
A low-speed implementation of the model CRC algorithm is provided in
the C programming language. Lastly there is a section giving two forms
of high-speed table driven implementations, and providing a program
that generates CRC lookup tables.


1. Introduction: Error Detection
--------------------------------
The aim of an error detection technique is to enable the receiver of a
message transmitted through a noisy (error-introducing) channel to
determine whether the message has been corrupted. To do this, the
transmitter constructs a value (called a checksum) that is a function
of the message, and appends it to the message. The receiver can then
use the same function to calculate the checksum of the received
message and compare it with the appended checksum to see if the
message was correctly received. For example, if we chose a checksum
function which was simply the sum of the bytes in the message mod 256
(i.e. modulo 256), then it might go something as follows. All numbers
are in decimal.

   Message                    :  6 23  4
   Message with checksum      :  6 23  4 33
   Message after transmission :  6 27  4 33

In the above, the second byte of the message was corrupted from 23 to
27 by the communications channel. However, the receiver can detect
this by comparing the transmitted checksum (33) with the computer
checksum of 37 (6 + 27 + 4). If the checksum itself is corrupted, a
correctly transmitted message might be incorrectly identified as a
corrupted one. However, this is a safe-side failure. A dangerous-side
failure occurs where the message and/or checksum is corrupted in a
manner that results in a transmission that is internally consistent.
Unfortunately, this possibility is completely unavoidable and the best
that can be done is to minimize its probability by increasing the
amount of information in the checksum (e.g. widening the checksum from
one byte to two bytes).

Other error detection techniques exist that involve performing complex
transformations on the message to inject it with redundant
information. However, this document addresses only CRC algorithms,
which fall into the class of error detection algorithms that leave the
data intact and append a checksum on the end. i.e.:

      <original intact message> <checksum>


2. The Need For Complexity
--------------------------
In the checksum example in the previous section, we saw how a
corrupted message was detected using a checksum algorithm that simply
sums the bytes in the message mod 256:

   Message                    :  6 23  4
   Message with checksum      :  6 23  4 33
   Message after transmission :  6 27  4 33

A problem with this algorithm is that it is too simple. If a number of
random corruptions occur, there is a 1 in 256 chance that they will
not be detected. For example:

   Message                    :  6 23  4
   Message with checksum      :  6 23  4 33
   Message after transmission :  8 20  5 33

To strengthen the checksum, we could change from an 8-bit register to
a 16-bit register (i.e. sum the bytes mod 65536 instead of mod 256) so
as to apparently reduce the probability of failure from 1/256 to
1/65536. While basically a good idea, it fails in this case because
the formula used is not sufficiently "random"; with a simple summing
formula, each incoming byte affects roughly only one byte of the
summing register no matter how wide it is. For example, in the second
example above, the summing register could be a Megabyte wide, and the
error would still go undetected. This problem can only be solved by
replacing the simple summing formula with a more sophisticated formula
that causes each incoming byte to have an effect on the entire
checksum register.

Thus, we see that at least two aspects are required to form a strong
checksum function:

   WIDTH: A register width wide enough to provide a low a-priori
          probability of failure (e.g. 32-bits gives a 1/2^32 chance
          of failure).

   CHAOS: A formula that gives each input byte the potential to change
          any number of bits in the register.

Note: The term "checksum" was presumably used to describe early
summing formulas, but has now taken on a more general meaning
encompassing more sophisticated algorithms such as the CRC ones. The
CRC algorithms to be described satisfy the second condition very well,
and can be configured to operate with a variety of checksum widths.


3. The Basic Idea Behind CRC Algorithms
---------------------------------------
Where might we go in our search for a more complex function than
summing? All sorts of schemes spring to mind. We could construct
tables using the digits of pi, or hash each incoming byte with all the
bytes in the register. We could even keep a large telephone book
on-line, and use each incoming byte combined with the register bytes
to index a new phone number which would be the next register value.
The possibilities are limitless.

However, we do not need to go so far; the next arithmetic step
suffices. While addition is clearly not strong enough to form an
effective checksum, it turns out that division is, so long as the
divisor is about as wide as the checksum register.

The basic idea of CRC algorithms is simply to treat the message as an
enormous binary number, to divide it by another fixed binary number,
and to make the remainder from this division the checksum. Upon
receipt of the message, the receiver can perform the same division and
compare the remainder with the "checksum" (transmitted remainder).

Example: Suppose the the message consisted of the two bytes (6,23) as
in the previous example. These can be considered to be the hexadecimal
number 0617 which can be considered to be the binary number
0000-0110-0001-0111. Suppose that we use a checksum register one-byte
wide and use a constant divisor of 1001, then the checksum is the
remainder after 0000-0110-0001-0111 is divided by 1001. While in this
case, this calculation could obviously be performed using common
garden variety 32-bit registers, in the general case this is messy. So
instead, we'll do the division using good-'ol long division which you
learnt in school (remember?). Except this time, it's in binary:

          ...0000010101101 = 00AD =  173 = QUOTIENT
         ____-___-___-___-
9= 1001 ) 0000011000010111 = 0617 = 1559 = DIVIDEND
DIVISOR   0000.,,....,.,,,
          ----.,,....,.,,,
           0000,,....,.,,,
           0000,,....,.,,,
           ----,,....,.,,,
            0001,....,.,,,
            0000,....,.,,,
            ----,....,.,,,
             0011....,.,,,
             0000....,.,,,
             ----....,.,,,
              0110...,.,,,
              0000...,.,,,
              ----...,.,,,
               1100..,.,,,
               1001..,.,,,
               ====..,.,,,
                0110.,.,,,
                0000.,.,,,
                ----.,.,,,
                 1100,.,,,
                 1001,.,,,
                 ====,.,,,
                  0111.,,,
                  0000.,,,
                  ----.,,,
                   1110,,,
                   1001,,,
                   ====,,,
                    1011,,
                    1001,,
                    ====,,
                     0101,
                     0000,
                     ----
                      1011
                      1001
                      ====
                      0010 = 02 = 2 = REMAINDER


In decimal this is "1559 divided by 9 is 173 with a remainder of 2".

Although the effect of each bit of the input message on the quotient
is not all that significant, the 4-bit remainder gets kicked about
quite a lot during the calculation, and if more bytes were added to
the message (dividend) it's value could change radically again very
quickly. This is why division works where addition doesn't.

In case you're wondering, using this 4-bit checksum the transmitted
message would look like this (in hexadecimal): 06172 (where the 0617
is the message and the 2 is the checksum). The receiver would divide
0617 by 9 and see whether the remainder was 2.


4. Polynomical Arithmetic
-------------------------
While the division scheme described in the previous section is very
very similar to the checksumming schemes called CRC schemes, the CRC
schemes are in fact a bit weirder, and we need to delve into some
strange number systems to understand them.

The word you will hear all the time when dealing with CRC algorithms
is the word "polynomial". A given CRC algorithm will be said to be
using a particular polynomial, and CRC algorithms in general are said
to be operating using polynomial arithmetic. What does this mean?

Instead of the divisor, dividend (message), quotient, and remainder
(as described in the previous section) being viewed as positive
integers, they are viewed as polynomials with binary coefficients.
This is done by treating each number as a bit-string whose bits are
the coefficients of a polynomial. For example, the ordinary number 23
(decimal) is 17 (hex) and 10111 binary and so it corresponds to the
polynomial:

   1*x^4 + 0*x^3 + 1*x^2 + 1*x^1 + 1*x^0

or, more simply:

   x^4 + x^2 + x^1 + x^0

Using this technique, the message, and the divisor can be represented
as polynomials and we can do all our arithmetic just as before, except
that now it's all cluttered up with Xs. For example, suppose we wanted
to multiply 1101 by 1011. We can do this simply by multiplying the
polynomials:

(x^3 + x^2 + x^0)(x^3 + x^1 + x^0)
= (x^6 + x^4 + x^3
 + x^5 + x^3 + x^2
 + x^3 + x^1 + x^0) = x^6 + x^5 + x^4 + 3*x^3 + x^2 + x^1 + x^0

At this point, to get the right answer, we have to pretend that x is 2
and propagate binary carries from the 3*x^3 yielding

   x^7 + x^3 + x^2 + x^1 + x^0

It's just like ordinary arithmetic except that the base is abstracted
and brought into all the calculations explicitly instead of being
there implicitly. So what's the point?

The point is that IF we pretend that we DON'T know what x is, we CAN'T
perform the carries. We don't know that 3*x^3 is the same as x^4 + x^3
because we don't know that x is 2. In this true polynomial arithmetic
the relationship between all the coefficients is unknown and so the
coefficients of each power effectively become strongly typed;
coefficients of x^2 are effectively of a different type to
coefficients of x^3.

With the coefficients of each power nicely isolated, mathematicians
came up with all sorts of different kinds of polynomial arithmetics
simply by changing the rules about how coefficients work. Of these
schemes, one in particular is relevant here, and that is a polynomial
arithmetic where the coefficients are calculated MOD 2 and there is no
carry; all coefficients must be either 0 or 1 and no carries are
calculated. This is called "polynomial arithmetic mod 2". Thus,
returning to the earlier example:

(x^3 + x^2 + x^0)(x^3 + x^1 + x^0)
= (x^6 + x^4 + x^3
 + x^5 + x^3 + x^2
 + x^3 + x^1 + x^0)
= x^6 + x^5 + x^4 + 3*x^3 + x^2 + x^1 + x^0

Under the other arithmetic, the 3*x^3 term was propagated using the
carry mechanism using the knowledge that x=2. Under "polynomial
arithmetic mod 2", we don't know what x is, there are no carries, and
all coefficients have to be calculated mod 2. Thus, the result
becomes:

= x^6 + x^5 + x^4 + x^3 + x^2 + x^1 + x^0

As Knuth [Knuth81] says (p.400):

   "The reader should note the similarity between polynomial
   arithmetic and multiple-precision arithmetic (Section 4.3.1), where
   the radix b is substituted for x. The chief difference is that the
   coefficient u_k of x^k in polynomial arithmetic bears little or no
   relation to its neighboring coefficients x^{k-1} [and x^{k+1}], so
   the idea of "carrying" from one place to another is absent. In fact
   polynomial arithmetic modulo b is essentially identical to multiple
   precision arithmetic with radix b, except that all carries are
   suppressed."

Thus polynomical arithmetic mod 2 is just binary arithmetic mod 2 with
no carries. While polynomials provide useful mathematical machinery in
more analytical approaches to CRC and error-correction algorithms, for
the purposes of exposition they provide no extra insight and some
encumbrance and have been discarded in the remainder of this document
in favour of direct manipulation of the arithmetical system with which
they are isomorphic: binary arithmetic with no carry.


5. Binary Arithmetic with No Carries
------------------------------------
Having dispensed with polynomials, we can focus on the real arithmetic
issue, which is that all the arithmetic performed during CRC
calculations is performed in binary with no carries. Often this is
called polynomial arithmetic, but as I have declared the rest of this
document a polynomial free zone, we'll have to call it CRC arithmetic
instead. As this arithmetic is a key part of CRC calculations, we'd
better get used to it. Here we go:

Adding two numbers in CRC arithmetic is the same as adding numbers in
ordinary binary arithmetic except there is no carry. This means that
each pair of corresponding bits determine the corresponding output bit
without reference to any other bit positions. For example:

        10011011
       +11001010
        --------
        01010001
        --------

There are only four cases for each bit position:

   0+0=0
   0+1=1
   1+0=1
   1+1=0  (no carry)

Subtraction is identical:

        10011011
       -11001010
        --------
        01010001
        --------

with

   0-0=0
   0-1=1  (wraparound)
   1-0=1
   1-1=0

In fact, both addition and subtraction in CRC arithmetic is equivalent
to the XOR operation, and the XOR operation is its own inverse. This
effectively reduces the operations of the first level of power
(addition, subtraction) to a single operation that is its own inverse.
This is a very convenient property of the arithmetic.

By collapsing of addition and subtraction, the arithmetic discards any
notion of magnitude beyond the power of its highest one bit. While it
seems clear that 1010 is greater than 10, it is no longer the case
that 1010 can be considered to be greater than 1001. To see this, note
that you can get from 1010 to 1001 by both adding and subtracting the
same quantity:

   1010 = 1010 + 0011
   1010 = 1010 - 0011

This makes nonsense of any notion of order.

Having defined addition, we can move to multiplication and division.
Multiplication is absolutely straightforward, being the sum of the
first number, shifted in accordance with the second number.

        1101
      x 1011
        ----
        1101
       1101.
      0000..
     1101...
     -------
     1111111  Note: The sum uses CRC addition
     -------

Division is a little messier as we need to know when "a number goes
into another number". To do this, we invoke the weak definition of
magnitude defined earlier: that X is greater than or equal to Y iff
the position of the highest 1 bit of X is the same or greater than the
position of the highest 1 bit of Y. Here's a fully worked division
(nicked from [Tanenbaum81]).

            1100001010
       _______________
10011 ) 11010110110000
        10011,,.,,....
        -----,,.,,....
         10011,.,,....
         10011,.,,....
         -----,.,,....
          00001.,,....
          00000.,,....
          -----.,,....
           00010,,....
           00000,,....
           -----,,....
            00101,....
            00000,....
            -----,....
             01011....
             00000....
             -----....
              10110...
              10011...
              -----...
               01010..
               00000..
               -----..
                10100.
                10011.
                -----.
                 01110
                 00000
                 -----
                  1110 = Remainder

That's really it. Before proceeding further, however, it's worth our
while playing with this arithmetic a bit to get used to it.

We've already played with addition and subtraction, noticing that they
are the same thing. Here, though, we should note that in this
arithmetic A+0=A and A-0=A. This obvious property is very useful
later.

In dealing with CRC multiplication and division, it's worth getting a
feel for the concepts of MULTIPLE and DIVISIBLE.

If a number A is a multiple of B then what this means in CRC
arithmetic is that it is possible to construct A from zero by XORing
in various shifts of B. For example, if A was 0111010110 and B was 11,
we could construct A from B as follows:

                  0111010110
                = .......11.
                + ....11....
                + ...11.....
                  .11.......

However, if A is 0111010111, it is not possible to construct it out of
various shifts of B (can you see why? - see later) so it is said to be
not divisible by B in CRC arithmetic.

Thus we see that CRC arithmetic is primarily about XORing particular
values at various shifting offsets.


6. A Fully Worked Example
-------------------------
Having defined CRC arithmetic, we can now frame a CRC calculation as
simply a division, because that's all it is! This section fills in the
details and gives an example.

To perform a CRC calculation, we need to choose a divisor. In maths
marketing speak the divisor is called the "generator polynomial" or
simply the "polynomial", and is a key parameter of any CRC algorithm.
It would probably be more friendly to call the divisor something else,
but the poly talk is so deeply ingrained in the field that it would
now be confusing to avoid it. As a compromise, we will refer to the
CRC polynomial as the "poly". Just think of this number as a sort of
parrot. "Hello poly!"

You can choose any poly and come up with a CRC algorithm. However,
some polys are better than others, and so it is wise to stick with the
tried an tested ones. A later section addresses this issue.

The width (position of the highest 1 bit) of the poly is very
important as it dominates the whole calculation. Typically, widths of
16 or 32 are chosen so as to simplify implementation on modern
computers. The width of a poly is the actual bit position of the
highest bit. For example, the width of 10011 is 4, not 5. For the
purposes of example, we will chose a poly of 10011 (of width W of 4).

Having chosen a poly, we can proceed with the calculation. This is
simply a division (in CRC arithmetic) of the message by the poly. The
only trick is that W zero bits are appended to the message before the
CRC is calculated. Thus we have:

   Original message                : 1101011011
   Poly                            :      10011
   Message after appending W zeros : 11010110110000

Now we simply divide the augmented message by the poly using CRC
arithmetic. This is the same division as before:

            1100001010 = Quotient (nobody cares about the quotient)
       _______________
10011 ) 11010110110000 = Augmented message (1101011011 + 0000)
=Poly  10011,,.,,....
        -----,,.,,....
         10011,.,,....
         10011,.,,....
         -----,.,,....
          00001.,,....
          00000.,,....
          -----.,,....
           00010,,....
           00000,,....
           -----,,....
            00101,....
            00000,....
            -----,....
             01011....
             00000....
             -----....
              10110...
              10011...
              -----...
               01010..
               00000..
               -----..
                10100.
                10011.
                -----.
                 01110
                 00000
                 -----
                  1110 = Remainder = THE CHECKSUM!!!!

The division yields a quotient, which we throw away, and a remainder,
which is the calculated checksum. This ends the calculation.

Usually, the checksum is then appended to the message and the result
transmitted. In this case the transmission would be: 11010110111110.

At the other end, the receiver can do one of two things:

   a. Separate the message and checksum. Calculate the checksum for
      the message (after appending W zeros) and compare the two
      checksums.

   b. Checksum the whole lot (without appending zeros) and see if it
      comes out as zero!

These two options are equivalent. However, in the next section, we
will be assuming option b because it is marginally mathematically
cleaner.

A summary of the operation of the class of CRC algorithms:

   1. Choose a width W, and a poly G (of width W).
   2. Append W zero bits to the message. Call this M'.
   3. Divide M' by G using CRC arithmetic. The remainder is the checksum.

That's all there is to it.

7. Choosing A Poly
------------------
Choosing a poly is somewhat of a black art and the reader is referred
to [Tanenbaum81] (p.130-132) which has a very clear discussion of this
issue. This section merely aims to put the fear of death into anyone
who so much as toys with the idea of making up their own poly. If you
don't care about why one poly might be better than another and just
want to find out about high-speed implementations, choose one of the
arithmetically sound polys listed at the end of this section and skip
to the next section.

First note that the transmitted message T is a multiple of the poly.
To see this, note that 1) the last W bits of T is the remainder after
dividing the augmented (by zeros remember) message by the poly, and 2)
addition is the same as subtraction so adding the remainder pushes the
value up to the next multiple. Now note that if the transmitted
message is corrupted in transmission that we will receive T+E where E
is an error vector (and + is CRC addition (i.e. XOR)). Upon receipt of
this message, the receiver divides T+E by G. As T mod G is 0, (T+E)
mod G = E mod G. Thus, the capacity of the poly we choose to catch
particular kinds of errors will be determined by the set of multiples
of G, for any corruption E that is a multiple of G will be undetected.
Our task then is to find classes of G whose multiples look as little
like the kind of line noise (that will be creating the corruptions) as
possible. So let's examine the kinds of line noise we can expect.

SINGLE BIT ERRORS: A single bit error means E=1000...0000. We can
ensure that this class of error is always detected by making sure that
G has at least two bits set to 1. Any multiple of G will be
constructed using shifting and adding and it is impossible to
construct a value with a single bit by shifting an adding a single
value with more than one bit set, as the two end bits will always
persist.

TWO-BIT ERRORS: To detect all errors of the form 100...000100...000
(i.e. E contains two 1 bits) choose a G that does not have multiples
that are 11, 101, 1001, 10001, 100001, etc. It is not clear to me how
one goes about doing this (I don't have the pure maths background),
but Tanenbaum assures us that such G do exist, and cites G with 1 bits
(15,14,1) turned on as an example of one G that won't divide anything
less than 1...1 where ... is 32767 zeros.

ERRORS WITH AN ODD NUMBER OF BITS: We can catch all corruptions where
E has an odd number of bits by choosing a G that has an even number of
bits. To see this, note that 1) CRC multiplication is simply XORing a
constant value into a register at various offsets, 2) XORing is simply
a bit-flip operation, and 3) if you XOR a value with an even number of
bits into a register, the oddness of the number of 1 bits in the
register remains invariant. Example: Starting with E=111, attempt to
flip all three bits to zero by the repeated application of XORing in
11 at one of the two offsets (i.e. "E=E XOR 011" and "E=E XOR 110")
This is nearly isomorphic to the "glass tumblers" party puzzle where
you challenge someone to flip three tumblers by the repeated
application of the operation of flipping any two. Most of the popular
CRC polys contain an even number of 1 bits. (Note: Tanenbaum states
more specifically that all errors with an odd number of bits can be
caught by making G a multiple of 11).

BURST ERRORS: A burst error looks like E=000...000111...11110000...00.
That is, E consists of all zeros except for a run of 1s somewhere
inside. This can be recast as E=(10000...00)(1111111...111) where
there are z zeros in the LEFT part and n ones in the RIGHT part. To
catch errors of this kind, we simply set the lowest bit of G to 1.
Doing this ensures that LEFT cannot be a factor of G. Then, so long as
G is wider than RIGHT, the error will be detected. See Tanenbaum for a
clearer explanation of this; I'm a little fuzzy on this one. Note:
Tanenbaum asserts that the probability of a burst of length greater
than W getting through is (0.5)^W.

That concludes the section on the fine art of selecting polys.

Some popular polys are:
16 bits: (16,12,5,0)                                [X25 standard]
         (16,15,2,0)                                ["CRC-16"]
32 bits: (32,26,23,22,16,12,11,10,8,7,5,4,2,1,0)    [Ethernet]


8. A Straightforward CRC Implementation
---------------------------------------
That's the end of the theory; now we turn to implementations. To start
with, we examine an absolutely straight-down-the-middle boring
straightforward low-speed implementation that doesn't use any speed
tricks at all. We'll then transform that program progessively until we
end up with the compact table-driven code we all know and love and
which some of us would like to understand.

To implement a CRC algorithm all we have to do is implement CRC
division. There are two reasons why we cannot simply use the divide
instruction of whatever machine we are on. The first is that we have
to do the divide in CRC arithmetic. The second is that the dividend
might be ten megabytes long, and todays processors do not have
registers that big.

So to implement CRC division, we have to feed the message through a
division register. At this point, we have to be absolutely precise
about the message data. In all the following examples the message will
be considered to be a stream of bytes (each of 8 bits) with bit 7 of
each byte being considered to be the most significant bit (MSB). The
bit stream formed from these bytes will be the bit stream with the MSB
(bit 7) of the first byte first, going down to bit 0 of the first
byte, and then the MSB of the second byte and so on.

With this in mind, we can sketch an implementation of the CRC
division. For the purposes of example, consider a poly with W=4 and
the poly=10111. Then, the perform the division, we need to use a 4-bit
register:

                  3   2   1   0   Bits
                +---+---+---+---+
       Pop! <-- |   |   |   |   | <----- Augmented message
                +---+---+---+---+

             1    0   1   1   1   = The Poly

(Reminder: The augmented message is the message followed by W zero bits.)

To perform the division perform the following:

   Load the register with zero bits.
   Augment the message by appending W zero bits to the end of it.
   While (more message bits)
      Begin
      Shift the register left by one bit, reading the next bit of the
         augmented message into register bit position 0.
      If (a 1 bit popped out of the register during step 3)
         Register = Register XOR Poly.
      End
   The register now contains the remainder.

(Note: In practice, the IF condition can be tested by testing the top
 bit of R before performing the shift.)

We will call this algorithm "SIMPLE".

This might look a bit messy, but all we are really doing is
"subtracting" various powers (i.e. shiftings) of the poly from the
message until there is nothing left but the remainder. Study the
manual examples of long division if you don't understand this.

It should be clear that the above algorithm will work for any width W.


9. A Table-Driven Implementation
--------------------------------
The SIMPLE algorithm above is a good starting point because it
corresponds directly to the theory presented so far, and because it is
so SIMPLE. However, because it operates at the bit level, it is rather
awkward to code (even in C), and inefficient to execute (it has to
loop once for each bit). To speed it up, we need to find a way to
enable the algorithm to process the message in units larger than one
bit. Candidate quantities are nibbles (4 bits), bytes (8 bits), words
(16 bits) and longwords (32 bits) and higher if we can achieve it. Of
these, 4 bits is best avoided because it does not correspond to a byte
boundary. At the very least, any speedup should allow us to operate at
byte boundaries, and in fact most of the table driven algorithms
operate a byte at a time.

For the purposes of discussion, let us switch from a 4-bit poly to a
32-bit one. Our register looks much the same, except the boxes
represent bytes instead of bits, and the Poly is 33 bits (one implicit
1 bit at the top and 32 "active" bits) (W=32).

                   3    2    1    0   Bytes
                +----+----+----+----+
       Pop! <-- |    |    |    |    | <----- Augmented message
                +----+----+----+----+

               1<------32 bits------>

The SIMPLE algorithm is still applicable. Let us examine what it does.
Imagine that the SIMPLE algorithm is in full swing and consider the
top 8 bits of the 32-bit register (byte 3) to have the values:

   t7 t6 t5 t4 t3 t2 t1 t0

In the next iteration of SIMPLE, t7 will determine whether the Poly
will be XORed into the entire register. If t7=1, this will happen,
otherwise it will not. Suppose that the top 8 bits of the poly are g7
g6.. g0, then after the next iteration, the top byte will be:

        t6 t5 t4 t3 t2 t1 t0 ??
+ t7 * (g7 g6 g5 g4 g3 g2 g1 g0)    [Reminder: + is XOR]

The NEW top bit (that will control what happens in the next iteration)
now has the value t6 + t7*g7. The important thing to notice here is
that from an informational point of view, all the information required
to calculate the NEW top bit was present in the top TWO bits of the
original top byte. Similarly, the NEXT top bit can be calculated in
advance SOLELY from the top THREE bits t7, t6, and t5. In fact, in
general, the value of the top bit in the register in k iterations can
be calculated from the top k bits of the register. Let us take this
for granted for a moment.

Consider for a moment that we use the top 8 bits of the register to
calculate the value of the top bit of the register during the next 8
iterations. Suppose that we drive the next 8 iterations using the
calculated values (which we could perhaps store in a single byte
register and shift out to pick off each bit). Then we note three
things:

   * The top byte of the register now doesn't matter. No matter how
     many times and at what offset the poly is XORed to the top 8
     bits, they will all be shifted out the right hand side during the
     next 8 iterations anyway.


   * The remaining bits will be shifted left one position and the
     rightmost byte of the register will be shifted in the next byte

   AND

   * While all this is going on, the register will be subjected to a
     series of XOR's in accordance with the bits of the pre-calculated
     control byte.

Now consider the effect of XORing in a constant value at various
offsets to a register. For example:

       0100010  Register
       ...0110  XOR this
       ..0110.  XOR this
       0110...  XOR this
       -------
       0011000
       -------

The point of this is that you can XOR constant values into a register
to your heart's delight, and in the end, there will exist a value
which when XORed in with the original register will have the same
effect as all the other XORs.

Perhaps you can see the solution now. Putting all the pieces together
we have an algorithm that goes like this:

   While (augmented message is not exhausted)
      Begin
      Examine the top byte of the register
      Calculate the control byte from the top byte of the register
      Sum all the Polys at various offsets that are to be XORed into
         the register in accordance with the control byte
      Shift the register left by one byte, reading a new message byte
         into the rightmost byte of the register
      XOR the summed polys to the register
      End

As it stands this is not much better than the SIMPLE algorithm.
However, it turns out that most of the calculation can be precomputed
and assembled into a table. As a result, the above algorithm can be
reduced to:

   While (augmented message is not exhaused)
      Begin
      Top = top_byte(Register);
      Register = (Register << 24) | next_augmessage_byte;
      Register = Register XOR precomputed_table[Top];
      End

There! If you understand this, you've grasped the main idea of
table-driven CRC algorithms. The above is a very efficient algorithm
requiring just a shift, and OR, an XOR, and a table lookup per byte.
Graphically, it looks like this:

                   3    2    1    0   Bytes
                +----+----+----+----+
         +-----<|    |    |    |    | <----- Augmented message
         |      +----+----+----+----+
         |                ^
         |                |
         |               XOR
         |                |
         |     0+----+----+----+----+       Algorithm
         v      +----+----+----+----+       ---------
         |      +----+----+----+----+       1. Shift the register left by
         |      +----+----+----+----+          one byte, reading in a new
         |      +----+----+----+----+          message byte.
         |      +----+----+----+----+       2. Use the top byte just rotated
         |      +----+----+----+----+          out of the register to index
         +----->+----+----+----+----+          the table of 256 32-bit values.
                +----+----+----+----+       3. XOR the table value into the
                +----+----+----+----+          register.
                +----+----+----+----+       4. Goto 1 iff more augmented
                +----+----+----+----+          message bytes.
             255+----+----+----+----+


In C, the algorithm main loop looks like this:

   r=0;
   while (len--)
     {
      byte t = (r >> 24) & 0xFF;
      r = (r << 8) | *p++;
      r^=table[t];
     }

where len is the length of the augmented message in bytes, p points to
the augmented message, r is the register, t is a temporary, and table
is the computed table. This code can be made even more unreadable as
follows:

   r=0; while (len--) r = ((r << 8) | *p++) ^ t[(r >> 24) & 0xFF];

This is a very clean, efficient loop, although not a very obvious one
to the casual observer not versed in CRC theory. We will call this the
TABLE algorithm.


10. A Slightly Mangled Table-Driven Implementation
--------------------------------------------------
Despite the terse beauty of the line

   r=0; while (len--) r = ((r << 8) | *p++) ^ t[(r >> 24) & 0xFF];

those optimizing hackers couldn't leave it alone. The trouble, you
see, is that this loop operates upon the AUGMENTED message and in
order to use this code, you have to append W/8 zero bytes to the end
of the message before pointing p at it. Depending on the run-time
environment, this may or may not be a problem; if the block of data
was handed to us by some other code, it could be a BIG problem. One
alternative is simply to append the following line after the above
loop, once for each zero byte:

      for (i=0; i<W/4; i++) r = (r << 8) ^ t[(r >> 24) & 0xFF];

This looks like a sane enough solution to me. However, at the further
expense of clarity (which, you must admit, is already a pretty scare
commodity in this code) we can reorganize this small loop further so
as to avoid the need to either augment the message with zero bytes, or
to explicitly process zero bytes at the end as above. To explain the
optimization, we return to the processing diagram given earlier.

                   3    2    1    0   Bytes
                +----+----+----+----+
         +-----<|    |    |    |    | <----- Augmented message
         |      +----+----+----+----+
         |                ^
         |                |
         |               XOR
         |                |
         |     0+----+----+----+----+       Algorithm
         v      +----+----+----+----+       ---------
         |      +----+----+----+----+       1. Shift the register left by
         |      +----+----+----+----+          one byte, reading in a new
         |      +----+----+----+----+          message byte.
         |      +----+----+----+----+       2. Use the top byte just rotated
         |      +----+----+----+----+          out of the register to index
         +----->+----+----+----+----+          the table of 256 32-bit values.
                +----+----+----+----+       3. XOR the table value into the
                +----+----+----+----+          register.
                +----+----+----+----+       4. Goto 1 iff more augmented
                +----+----+----+----+          message bytes.
             255+----+----+----+----+

Now, note the following facts:

TAIL: The W/4 augmented zero bytes that appear at the end of the
      message will be pushed into the register from the right as all
      the other bytes are, but their values (0) will have no effect
      whatsoever on the register because 1) XORing with zero does not
      change the target byte, and 2) the four bytes are never
      propagated out the left side of the register where their
      zeroness might have some sort of influence. Thus, the sole
      function of the W/4 augmented zero bytes is to drive the
      calculation for another W/4 byte cycles so that the end of the
      REAL data passes all the way through the register.

HEAD: If the initial value of the register is zero, the first four
      iterations of the loop will have the sole effect of shifting in
      the first four bytes of the message from the right. This is
      because the first 32 control bits are all zero and so nothing is
      XORed into the register. Even if the initial value is not zero,
      the first 4 byte iterations of the algorithm will have the sole
      effect of shifting the first 4 bytes of the message into the
      register and then XORing them with some constant value (that is
      a function of the initial value of the register).

These facts, combined with the XOR property

   (A xor B) xor C = A xor (B xor C)

mean that message bytes need not actually travel through the W/4 bytes
of the register. Instead, they can be XORed into the top byte just
before it is used to index the lookup table. This leads to the
following modified version of the algorithm.


         +-----<Message (non augmented)
         |
         v         3    2    1    0   Bytes
         |      +----+----+----+----+
        XOR----<|    |    |    |    |
         |      +----+----+----+----+
         |                ^
         |                |
         |               XOR
         |                |
         |     0+----+----+----+----+       Algorithm
         v      +----+----+----+----+       ---------
         |      +----+----+----+----+       1. Shift the register left by
         |      +----+----+----+----+          one byte, reading in a new
         |      +----+----+----+----+          message byte.
         |      +----+----+----+----+       2. XOR the top byte just rotated
         |      +----+----+----+----+          out of the register with the
         +----->+----+----+----+----+          next message byte to yield an
                +----+----+----+----+          index into the table ([0,255]).
                +----+----+----+----+       3. XOR the table value into the
                +----+----+----+----+          register.
                +----+----+----+----+       4. Goto 1 iff more augmented
             255+----+----+----+----+          message bytes.


Note: The initial register value for this algorithm must be the
initial value of the register for the previous algorithm fed through
the table four times. Note: The table is such that if the previous
algorithm used 0, the new algorithm will too.

This is an IDENTICAL algorithm and will yield IDENTICAL results. The C
code looks something like this:

   r=0; while (len--) r = (r<<8) ^ t[(r >> 24) ^ *p++];

and THIS is the code that you are likely to find inside current
table-driven CRC implementations. Some FF masks might have to be ANDed
in here and there for portability's sake, but basically, the above
loop is IT. We will call this the DIRECT TABLE ALGORITHM.

During the process of trying to understand all this stuff, I managed
to derive the SIMPLE algorithm and the table-driven version derived
from that. However, when I compared my code with the code found in
real-implementations, I was totally bamboozled as to why the bytes
were being XORed in at the wrong end of the register! It took quite a
while before I figured out that theirs and my algorithms were actually
the same. Part of why I am writing this document is that, while the
link between division and my earlier table-driven code is vaguely
apparent, any such link is fairly well erased when you start pumping
bytes in at the "wrong end" of the register. It looks all wrong!

If you've got this far, you not only understand the theory, the
practice, the optimized practice, but you also understand the real
code you are likely to run into. Could get any more complicated? Yes
it can.


11. "Reflected" Table-Driven Implementations
--------------------------------------------
Despite the fact that the above code is probably optimized about as
much as it could be, this did not stop some enterprising individuals
from making things even more complicated. To understand how this
happened, we have to enter the world of hardware.

DEFINITION: A value/register is reflected if it's bits are swapped
around its centre. For example: 0101 is the 4-bit reflection of 1010.
0011 is the reflection of 1100.
0111-0101-1010-1111-0010-0101-1011-1100 is the reflection of
0011-1101-1010-0100-1111-0101-1010-1110.

Turns out that UARTs (those handy little chips that perform serial IO)
are in the habit of transmitting each byte with the least significant
bit (bit 0) first and the most significant bit (bit 7) last (i.e.
reflected). An effect of this convention is that hardware engineers
constructing hardware CRC calculators that operate at the bit level
took to calculating CRCs of bytes streams with each of the bytes
reflected within itself. The bytes are processed in the same order,
but the bits in each byte are swapped; bit 0 is now bit 7, bit 1 is
now bit 6, and so on. Now this wouldn't matter much if this convention
was restricted to hardware land. However it seems that at some stage
some of these CRC values were presented at the software level and
someone had to write some code that would interoperate with the
hardware CRC calculation.

In this situation, a normal sane software engineer would simply
reflect each byte before processing it. However, it would seem that
normal sane software engineers were thin on the ground when this early
ground was being broken, because instead of reflecting the bytes,
whoever was responsible held down the byte and reflected the world,
leading to the following "reflected" algorithm which is identical to
the previous one except that everything is reflected except the input
bytes.


             Message (non augmented) >-----+
                                           |
           Bytes   0    1    2    3        v
                +----+----+----+----+      |
                |    |    |    |    |>----XOR
                +----+----+----+----+      |
                          ^                |
                          |                |
                         XOR               |
                          |                |
                +----+----+----+----+0     |
                +----+----+----+----+      v
                +----+----+----+----+      |
                +----+----+----+----+      |
                +----+----+----+----+      |
                +----+----+----+----+      |
                +----+----+----+----+      |
                +----+----+----+----+<-----+
                +----+----+----+----+
                +----+----+----+----+
                +----+----+----+----+
                +----+----+----+----+
                +----+----+----+----+255

Notes:

   * The table is identical to the one in the previous algorithm
   except that each entry has been reflected.

   * The initial value of the register is the same as in the previous
   algorithm except that it has been reflected.

   * The bytes of the message are processed in the same order as
   before (i.e. the message itself is not reflected).

   * The message bytes themselves don't need to be explicitly
   reflected, because everything else has been!

At the end of execution, the register contains the reflection of the
final CRC value (remainder). Actually, I'm being rather hard on
whoever cooked this up because it seems that hardware implementations
of the CRC algorithm used the reflected checksum value and so
producing a reflected CRC was just right. In fact reflecting the world
was probably a good engineering solution - if a confusing one.

We will call this the REFLECTED algorithm.

Whether or not it made sense at the time, the effect of having
reflected algorithms kicking around the world's FTP sites is that
about half the CRC implementations one runs into are reflected and the
other half not. It's really terribly confusing. In particular, it
would seem to me that the casual reader who runs into a reflected,
table-driven implementation with the bytes "fed in the wrong end"
would have Buckley's chance of ever connecting the code to the concept
of binary mod 2 division.

It couldn't get any more confusing could it? Yes it could.


12. "Reversed" Polys
--------------------
As if reflected implementations weren't enough, there is another
concept kicking around which makes the situation bizaarly confusing.
The concept is reversed Polys.

It turns out that the reflection of good polys tend to be good polys
too! That is, if G=11101 is a good poly value, then 10111 will be as
well. As a consequence, it seems that every time an organization (such
as CCITT) standardizes on a particularly good poly ("polynomial"),
those in the real world can't leave the poly's reflection alone
either. They just HAVE to use it. As a result, the set of "standard"
poly's has a corresponding set of reflections, which are also in use.
To avoid confusion, we will call these the "reversed" polys.

   X25   standard: 1-0001-0000-0010-0001
   X25   reversed: 1-0000-1000-0001-0001

   CRC16 standard: 1-1000-0000-0000-0101
   CRC16 reversed: 1-0100-0000-0000-0011

Note that here it is the entire poly that is being reflected/reversed,
not just the bottom W bits. This is an important distinction. In the
reflected algorithm described in the previous section, the poly used
in the reflected algorithm was actually identical to that used in the
non-reflected algorithm; all that had happened is that the bytes had
effectively been reflected. As such, all the 16-bit/32-bit numbers in
the algorithm had to be reflected. In contrast, the ENTIRE poly
includes the implicit one bit at the top, and so reversing a poly is
not the same as reflecting its bottom 16 or 32 bits.

The upshot of all this is that a reflected algorithm is not equivalent
to the original algorithm with the poly reflected. Actually, this is
probably less confusing than if they were duals.

If all this seems a bit unclear, don't worry, because we're going to
sort it all out "real soon now". Just one more section to go before
that.


13. Initial and Final Values
----------------------------
In addition to the complexity already seen, CRC algorithms differ from
each other in two other regards:

   * The initial value of the register.

   * The value to be XORed with the final register value.

For example, the "CRC32" algorithm initializes its register to
FFFFFFFF and XORs the final register value with FFFFFFFF.

Most CRC algorithms initialize their register to zero. However, some
initialize it to a non-zero value. In theory (i.e. with no assumptions
about the message), the initial value has no affect on the strength of
the CRC algorithm, the initial value merely providing a fixed starting
point from which the register value can progress. However, in
practice, some messages are more likely than others, and it is wise to
initialize the CRC algorithm register to a value that does not have
"blind spots" that are likely to occur in practice. By "blind spot" is
meant a sequence of message bytes that do not result in the register
changing its value. In particular, any CRC algorithm that initializes
its register to zero will have a blind spot of zero when it starts up
and will be unable to "count" a leading run of zero bytes. As a
leading run of zero bytes is quite common in real messages, it is wise
to initialize the algorithm register to a non-zero value.


14. Defining Algorithms Absolutely
----------------------------------
At this point we have covered all the different aspects of
table-driven CRC algorithms. As there are so many variations on these
algorithms, it is worth trying to establish a nomenclature for them.
This section attempts to do that.

We have seen that CRC algorithms vary in:

   * Width of the poly (polynomial).
   * Value of the poly.
   * Initial value for the register.
   * Whether the bits of each byte are reflected before being processed.
   * Whether the algorithm feeds input bytes through the register or
     xors them with a byte from one end and then straight into the table.
   * Whether the final register value should be reversed (as in reflected
     versions).
   * Value to XOR with the final register value.

In order to be able to talk about particular CRC algorithms, we need
to able to define them more precisely than this. For this reason, the
next section attempts to provide a well-defined parameterized model
for CRC algorithms. To refer to a particular algorithm, we need then
simply specify the algorithm in terms of parameters to the model.


15. A Parameterized Model For CRC Algorithms
--------------------------------------------
In this section we define a precise parameterized model CRC algorithm
which, for want of a better name, we will call the "Rocksoft^tm Model
CRC Algorithm" (and why not? Rocksoft^tm could do with some free
advertising :-).

The most important aspect of the model algorithm is that it focusses
exclusively on functionality, ignoring all implementation details. The
aim of the exercise is to construct a way of referring precisely to
particular CRC algorithms, regardless of how confusingly they are
implemented. To this end, the model must be as simple and precise as
possible, with as little confusion as possible.

The Rocksoft^tm Model CRC Algorithm is based essentially on the DIRECT
TABLE ALGORITHM specified earlier. However, the algorithm has to be
further parameterized to enable it to behave in the same way as some
of the messier algorithms out in the real world.

To enable the algorithm to behave like reflected algorithms, we
provide a boolean option to reflect the input bytes, and a boolean
option to specify whether to reflect the output checksum value. By
framing reflection as an input/output transformation, we avoid the
confusion of having to mentally map the parameters of reflected and
non-reflected algorithms.

An extra parameter allows the algorithm's register to be initialized
to a particular value. A further parameter is XORed with the final
value before it is returned.

By putting all these pieces together we end up with the parameters of
the algorithm:

   NAME: This is a name given to the algorithm. A string value.

   WIDTH: This is the width of the algorithm expressed in bits. This
   is one less than the width of the Poly.

   POLY: This parameter is the poly. This is a binary value that
   should be specified as a hexadecimal number. The top bit of the
   poly should be omitted. For example, if the poly is 10110, you
   should specify 06. An important aspect of this parameter is that it
   represents the unreflected poly; the bottom bit of this parameter
   is always the LSB of the divisor during the division regardless of
   whether the algorithm being modelled is reflected.

   INIT: This parameter specifies the initial value of the register
   when the algorithm starts. This is the value that is to be assigned
   to the register in the direct table algorithm. In the table
   algorithm, we may think of the register always commencing with the
   value zero, and this value being XORed into the register after the
   N'th bit iteration. This parameter should be specified as a
   hexadecimal number.

   REFIN: This is a boolean parameter. If it is FALSE, input bytes are
   processed with bit 7 being treated as the most significant bit
   (MSB) and bit 0 being treated as the least significant bit. If this
   parameter is FALSE, each byte is reflected before being processed.

   REFOUT: This is a boolean parameter. If it is set to FALSE, the
   final value in the register is fed into the XOROUT stage directly,
   otherwise, if this parameter is TRUE, the final register value is
   reflected first.

   XOROUT: This is an W-bit value that should be specified as a
   hexadecimal number. It is XORed to the final register value (after
   the REFOUT) stage before the value is returned as the official
   checksum.

   CHECK: This field is not strictly part of the definition, and, in
   the event of an inconsistency between this field and the other
   field, the other fields take precedence. This field is a check
   value that can be used as a weak validator of implementations of
   the algorithm. The field contains the checksum obtained when the
   ASCII string "123456789" is fed through the specified algorithm
   (i.e. 313233... (hexadecimal)).

With these parameters defined, the model can now be used to specify a
particular CRC algorithm exactly. Here is an example specification for
a popular form of the CRC-16 algorithm.

   Name   : "CRC-16"
   Width  : 16
   Poly   : 8005
   Init   : 0000
   RefIn  : True
   RefOut : True
   XorOut : 0000
   Check  : BB3D


16. A Catalog of Parameter Sets for Standards
---------------------------------------------
At this point, I would like to give a list of the specifications for
commonly used CRC algorithms. However, most of the algorithms that I
have come into contact with so far are specified in such a vague way
that this has not been possible. What I can provide is a list of polys
for various CRC standards I have heard of:

   X25   standard : 1021       [CRC-CCITT, ADCCP, SDLC/HDLC]
   X25   reversed : 0811

   CRC16 standard : 8005
   CRC16 reversed : 4003       [LHA]

   CRC32          : 04C11DB7   [PKZIP, AUTODIN II, Ethernet, FDDI]

I would be interested in hearing from anyone out there who can tie
down the complete set of model parameters for any of these standards.

However, a program that was kicking around seemed to imply the
following specifications. Can anyone confirm or deny them (or provide
the check values (which I couldn't be bothered coding up and
calculating)).

   Name   : "CRC-16/CITT"
   Width  : 16
   Poly   : 1021
   Init   : FFFF
   RefIn  : False
   RefOut : False
   XorOut : 0000
   Check  : ?


   Name   : "XMODEM"
   Width  : 16
   Poly   : 8408
   Init   : 0000
   RefIn  : True
   RefOut : True
   XorOut : 0000
   Check  : ?


   Name   : "ARC"
   Width  : 16
   Poly   : 8005
   Init   : 0000
   RefIn  : True
   RefOut : True
   XorOut : 0000
   Check  : ?

Here is the specification for the CRC-32 algorithm which is reportedly
used in PKZip, AUTODIN II, Ethernet, and FDDI.

   Name   : "CRC-32"
   Width  : 32
   Poly   : 04C11DB7
   Init   : FFFFFFFF
   RefIn  : True
   RefOut : True
   XorOut : FFFFFFFF
   Check  : CBF43926


17. An Implementation of the Model Algorithm
--------------------------------------------
Here is an implementation of the model algorithm in the C programming
language. The implementation consists of a header file (.h) and an
implementation file (.c). If you're reading this document in a
sequential scroller, you can skip this code by searching for the
string "Roll Your Own".

To ensure that the following code is working, configure it for the
CRC-16 and CRC-32 algorithms given above and ensure that they produce
the specified "check" checksum when fed the test string "123456789"
(see earlier).

/******************************************************************************/
/*                             Start of crcmodel.h                            */
/******************************************************************************/
/*                                                                            */
/* Author : Ross Williams (ross@guest.adelaide.edu.au.).                      */
/* Date   : 3 June 1993.                                                      */
/* Status : Public domain.                                                    */
/*                                                                            */
/* Description : This is the header (.h) file for the reference               */
/* implementation of the Rocksoft^tm Model CRC Algorithm. For more            */
/* information on the Rocksoft^tm Model CRC Algorithm, see the document       */
/* titled "A Painless Guide to CRC Error Detection Algorithms" by Ross        */
/* Williams (ross@guest.adelaide.edu.au.). This document is likely to be in   */
/* "ftp.adelaide.edu.au/pub/rocksoft".                                        */
/*                                                                            */
/* Note: Rocksoft is a trademark of Rocksoft Pty Ltd, Adelaide, Australia.    */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* How to Use This Package                                                    */
/* -----------------------                                                    */
/* Step 1: Declare a variable of type cm_t. Declare another variable          */
/*         (p_cm say) of type p_cm_t and initialize it to point to the first  */
/*         variable (e.g. p_cm_t p_cm = &cm_t).                               */
/*                                                                            */
/* Step 2: Assign values to the parameter fields of the structure.            */
/*         If you don't know what to assign, see the document cited earlier.  */
/*         For example:                                                       */
/*            p_cm->cm_width = 16;                                            */
/*            p_cm->cm_poly  = 0x8005L;                                       */
/*            p_cm->cm_init  = 0L;                                            */
/*            p_cm->cm_refin = TRUE;                                          */
/*            p_cm->cm_refot = TRUE;                                          */
/*            p_cm->cm_xorot = 0L;                                            */
/*         Note: Poly is specified without its top bit (18005 becomes 8005).  */
/*         Note: Width is one bit less than the raw poly width.               */
/*                                                                            */
/* Step 3: Initialize the instance with a call cm_ini(p_cm);                  */
/*                                                                            */
/* Step 4: Process zero or more message bytes by placing zero or more         */
/*         successive calls to cm_nxt. Example: cm_nxt(p_cm,ch);              */
/*                                                                            */
/* Step 5: Extract the CRC value at any time by calling crc = cm_crc(p_cm);   */
/*         If the CRC is a 16-bit value, it will be in the bottom 16 bits.    */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Design Notes                                                               */
/* ------------                                                               */
/* PORTABILITY: This package has been coded very conservatively so that       */
/* it will run on as many machines as possible. For example, all external     */
/* identifiers have been restricted to 6 characters and all internal ones to  */
/* 8 characters. The prefix cm (for Crc Model) is used as an attempt to avoid */
/* namespace collisions. This package is endian independent.                  */
/*                                                                            */
/* EFFICIENCY: This package (and its interface) is not designed for           */
/* speed. The purpose of this package is to act as a well-defined reference   */
/* model for the specification of CRC algorithms. If you want speed, cook up  */
/* a specific table-driven implementation as described in the document cited  */
/* above. This package is designed for validation only; if you have found or  */
/* implemented a CRC algorithm and wish to describe it as a set of parameters */
/* to the Rocksoft^tm Model CRC Algorithm, your CRC algorithm implementation  */
/* should behave identically to this package under those parameters.          */
/*                                                                            */
/******************************************************************************/

/* The following #ifndef encloses this entire */
/* header file, rendering it indempotent.     */
#ifndef CM_DONE
#define CM_DONE

/******************************************************************************/

/* The following definitions are extracted from my style header file which    */
/* would be cumbersome to distribute with this package. The DONE_STYLE is the */
/* idempotence symbol used in my style header file.                           */

#ifndef DONE_STYLE

typedef unsigned long   ulong;
typedef unsigned        bool;
typedef unsigned char * p_ubyte_;

#ifndef TRUE
#define FALSE 0
#define TRUE  1
#endif

/* Change to the second definition if you don't have prototypes. */
#define P_(A) A
/* #define P_(A) () */

/* Uncomment this definition if you don't have void. */
/* typedef int void; */

#endif

/******************************************************************************/

/* CRC Model Abstract Type */
/* ----------------------- */
/* The following type stores the context of an executing instance of the  */
/* model algorithm. Most of the fields are model parameters which must be */
/* set before the first initializing call to cm_ini.                      */
typedef struct
  {
   int   cm_width;   /* Parameter: Width in bits [8,32].       */
   ulong cm_poly;    /* Parameter: The algorithm's polynomial. */
   ulong cm_init;    /* Parameter: Initial register value.     */
   bool  cm_refin;   /* Parameter: Reflect input bytes?        */
   bool  cm_refot;   /* Parameter: Reflect output CRC?         */
   ulong cm_xorot;   /* Parameter: XOR this to output CRC.     */

   ulong cm_reg;     /* Context: Context during execution.     */
  } cm_t;
typedef cm_t *p_cm_t;

/******************************************************************************/

/* Functions That Implement The Model */
/* ---------------------------------- */
/* The following functions animate the cm_t abstraction. */

void cm_ini P_((p_cm_t p_cm));
/* Initializes the argument CRC model instance.          */
/* All parameter fields must be set before calling this. */

void cm_nxt P_((p_cm_t p_cm,int ch));
/* Processes a single message byte [0,255]. */

void cm_blk P_((p_cm_t p_cm,p_ubyte_ blk_adr,ulong blk_len));
/* Processes a block of message bytes. */

ulong cm_crc P_((p_cm_t p_cm));
/* Returns the CRC value for the message bytes processed so far. */

/******************************************************************************/

/* Functions For Table Calculation */
/* ------------------------------- */
/* The following function can be used to calculate a CRC lookup table.        */
/* It can also be used at run-time to create or check static tables.          */

ulong cm_tab P_((p_cm_t p_cm,int index));
/* Returns the i'th entry for the lookup table for the specified algorithm.   */
/* The function examines the fields cm_width, cm_poly, cm_refin, and the      */
/* argument table index in the range [0,255] and returns the table entry in   */
/* the bottom cm_width bytes of the return value.                             */

/******************************************************************************/

/* End of the header file idempotence #ifndef */
#endif

/******************************************************************************/
/*                             End of crcmodel.h                              */
/******************************************************************************/


/******************************************************************************/
/*                             Start of crcmodel.c                            */
/******************************************************************************/
/*                                                                            */
/* Author : Ross Williams (ross@guest.adelaide.edu.au.).                      */
/* Date   : 3 June 1993.                                                      */
/* Status : Public domain.                                                    */
/*                                                                            */
/* Description : This is the implementation (.c) file for the reference       */
/* implementation of the Rocksoft^tm Model CRC Algorithm. For more            */
/* information on the Rocksoft^tm Model CRC Algorithm, see the document       */
/* titled "A Painless Guide to CRC Error Detection Algorithms" by Ross        */
/* Williams (ross@guest.adelaide.edu.au.). This document is likely to be in   */
/* "ftp.adelaide.edu.au/pub/rocksoft".                                        */
/*                                                                            */
/* Note: Rocksoft is a trademark of Rocksoft Pty Ltd, Adelaide, Australia.    */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Implementation Notes                                                       */
/* --------------------                                                       */
/* To avoid inconsistencies, the specification of each function is not echoed */
/* here. See the header file for a description of these functions.            */
/* This package is light on checking because I want to keep it short and      */
/* simple and portable (i.e. it would be too messy to distribute my entire    */
/* C culture (e.g. assertions package) with this package.                     */
/*                                                                            */
/******************************************************************************/

#include "crcmodel.h"

/******************************************************************************/

/* The following definitions make the code more readable. */

#define BITMASK(X) (1L << (X))
#define MASK32 0xFFFFFFFFL
#define LOCAL static

/******************************************************************************/

LOCAL ulong reflect P_((ulong v,int b));
LOCAL ulong reflect (v,b)
/* Returns the value v with the bottom b [0,32] bits reflected. */
/* Example: reflect(0x3e23L,3) == 0x3e26                        */
ulong v;
int   b;
{
 int   i;
 ulong t = v;
 for (i=0; i<b; i++)
   {
    if (t & 1L)
       v|=  BITMASK((b-1)-i);
    else
       v&= ~BITMASK((b-1)-i);
    t>>=1;
   }
 return v;
}

/******************************************************************************/

LOCAL ulong widmask P_((p_cm_t));
LOCAL ulong widmask (p_cm)
/* Returns a longword whose value is (2^p_cm->cm_width)-1.     */
/* The trick is to do this portably (e.g. without doing <<32). */
p_cm_t p_cm;
{
 return (((1L<<(p_cm->cm_width-1))-1L)<<1)|1L;
}

/******************************************************************************/

void cm_ini (p_cm)
p_cm_t p_cm;
{
 p_cm->cm_reg = p_cm->cm_init;
}

/******************************************************************************/

void cm_nxt (p_cm,ch)
p_cm_t p_cm;
int    ch;
{
 int   i;
 ulong uch  = (ulong) ch;
 ulong topbit = BITMASK(p_cm->cm_width-1);

 if (p_cm->cm_refin) uch = reflect(uch,8);
 p_cm->cm_reg ^= (uch << (p_cm->cm_width-8));
 for (i=0; i<8; i++)
   {
    if (p_cm->cm_reg & topbit)
       p_cm->cm_reg = (p_cm->cm_reg << 1) ^ p_cm->cm_poly;
    else
       p_cm->cm_reg <<= 1;
    p_cm->cm_reg &= widmask(p_cm);
   }
}

/******************************************************************************/

void cm_blk (p_cm,blk_adr,blk_len)
p_cm_t   p_cm;
p_ubyte_ blk_adr;
ulong    blk_len;
{
 while (blk_len--) cm_nxt(p_cm,*blk_adr++);
}

/******************************************************************************/

ulong cm_crc (p_cm)
p_cm_t p_cm;
{
 if (p_cm->cm_refot)
    return p_cm->cm_xorot ^ reflect(p_cm->cm_reg,p_cm->cm_width);
 else
    return p_cm->cm_xorot ^ p_cm->cm_reg;
}

/******************************************************************************/

ulong cm_tab (p_cm,index)
p_cm_t p_cm;
int    index;
{
 int   i;
 ulong r;
 ulong topbit = BITMASK(p_cm->cm_width-1);
 ulong inbyte = (ulong) index;

 if (p_cm->cm_refin) inbyte = reflect(inbyte,8);
 r = inbyte << (p_cm->cm_width-8);
 for (i=0; i<8; i++)
    if (r & topbit)
       r = (r << 1) ^ p_cm->cm_poly;
    else
       r<<=1;
 if (p_cm->cm_refin) r = reflect(r,p_cm->cm_width);
 return r & widmask(p_cm);
}

/******************************************************************************/
/*                             End of crcmodel.c                              */
/******************************************************************************/


18. Roll Your Own Table-Driven Implementation
---------------------------------------------
Despite all the fuss I've made about understanding and defining CRC
algorithms, the mechanics of their high-speed implementation remains
trivial. There are really only two forms: normal and reflected. Normal
shifts to the left and covers the case of algorithms with Refin=FALSE
and Refot=FALSE. Reflected shifts to the right and covers algorithms
with both those parameters true. (If you want one parameter true and
the other false, you'll have to figure it out for yourself!) The
polynomial is embedded in the lookup table (to be discussed). The
other parameters, Init and XorOt can be coded as macros. Here is the
32-bit normal form (the 16-bit form is similar).

   unsigned long crc_normal ();
   unsigned long crc_normal (blk_adr,blk_len)
   unsigned char *blk_adr;
   unsigned long  blk_len;
   {
    unsigned long crc = INIT;
    while (blk_len--)
       crc = crctable[((crc>>24) ^ *blk_adr++) & 0xFFL] ^ (crc << 8);
    return crc ^ XOROT;
   }

Here is the reflected form:

   unsigned long crc_reflected ();
   unsigned long crc_reflected (blk_adr,blk_len)
   unsigned char *blk_adr;
   unsigned long  blk_len;
   {
    unsigned long crc = INIT_REFLECTED;
    while (blk_len--)
       crc = crctable[(crc ^ *blk_adr++) & 0xFFL] ^ (crc >> 8));
    return crc ^ XOROT;
   }

Note: I have carefully checked the above two code fragments, but I
haven't actually compiled or tested them. This shouldn't matter to
you, as, no matter WHAT you code, you will always be able to tell if
you have got it right by running whatever you have created against the
reference model given earlier. The code fragments above are really
just a rough guide. The reference model is the definitive guide.

Note: If you don't care much about speed, just use the reference model
code!


19. Generating A Lookup Table
-----------------------------
The only component missing from the normal and reversed code fragments
in the previous section is the lookup table. The lookup table can be
computed at run time using the cm_tab function of the model package
given earlier, or can be pre-computed and inserted into the C program.
In either case, it should be noted that the lookup table depends only
on the POLY and RefIn (and RefOt) parameters. Basically, the
polynomial determines the table, but you can generate a reflected
table too if you want to use the reflected form above.

The following program generates any desired 16-bit or 32-bit lookup
table. Skip to the word "Summary" if you want to skip over this code.



/******************************************************************************/
/*                             Start of crctable.c                            */
/******************************************************************************/
/*                                                                            */
/* Author  : Ross Williams (ross@guest.adelaide.edu.au.).                     */
/* Date    : 3 June 1993.                                                     */
/* Version : 1.0.                                                             */
/* Status  : Public domain.                                                   */
/*                                                                            */
/* Description : This program writes a CRC lookup table (suitable for         */
/* inclusion in a C program) to a designated output file. The program can be  */
/* statically configured to produce any table covered by the Rocksoft^tm      */
/* Model CRC Algorithm. For more information on the Rocksoft^tm Model CRC     */
/* Algorithm, see the document titled "A Painless Guide to CRC Error          */
/* Detection Algorithms" by Ross Williams (ross@guest.adelaide.edu.au.). This */
/* document is likely to be in "ftp.adelaide.edu.au/pub/rocksoft".            */
/*                                                                            */
/* Note: Rocksoft is a trademark of Rocksoft Pty Ltd, Adelaide, Australia.    */
/*                                                                            */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "crcmodel.h"

/******************************************************************************/

/* TABLE PARAMETERS                                                           */
/* ================                                                           */
/* The following parameters entirely determine the table to be generated. You */
/* should need to modify only the definitions in this section before running  */
/* this program.                                                              */
/*                                                                            */
/*    TB_FILE  is the name of the output file.                                */
/*    TB_WIDTH is the table width in bytes (either 2 or 4).                   */
/*    TB_POLY  is the "polynomial", which must be TB_WIDTH bytes wide.        */
/*    TB_REVER indicates whether the table is to be reversed (reflected).     */
/*                                                                            */
/* Example:                                                                   */
/*                                                                            */
/*    #define TB_FILE   "crctable.out"                                        */
/*    #define TB_WIDTH  2                                                     */
/*    #define TB_POLY   0x8005L                                               */
/*    #define TB_REVER  TRUE                                                  */

#define TB_FILE   "crctable.out"
#define TB_WIDTH  4
#define TB_POLY   0x04C11DB7L
#define TB_REVER  TRUE

/******************************************************************************/

/* Miscellaneous definitions. */

#define LOCAL static
FILE *outfile;
#define WR(X) fprintf(outfile,(X))
#define WP(X,Y) fprintf(outfile,(X),(Y))

/******************************************************************************/

LOCAL void chk_err P_((char *));
LOCAL void chk_err (mess)
/* If mess is non-empty, write it out and abort. Otherwise, check the error   */
/* status of outfile and abort if an error has occurred.                      */
char *mess;
{
 if (mess[0] != 0   ) {printf("%s\n",mess); exit(EXIT_FAILURE);}
 if (ferror(outfile)) {perror("chk_err");   exit(EXIT_FAILURE);}
}

/******************************************************************************/

LOCAL void chkparam P_((void));
LOCAL void chkparam ()
{
 if ((TB_WIDTH != 2) && (TB_WIDTH != 4))
    chk_err("chkparam: Width parameter is illegal.");
 if ((TB_WIDTH == 2) && (TB_POLY & 0xFFFF0000L))
    chk_err("chkparam: Poly parameter is too wide.");
 if ((TB_REVER != FALSE) && (TB_REVER != TRUE))
    chk_err("chkparam: Reverse parameter is not boolean.");
}

/******************************************************************************/

LOCAL void gentable P_((void));
LOCAL void gentable ()
{
 WR("/*****************************************************************/\n");
 WR("/*                                                               */\n");
 WR("/* CRC LOOKUP TABLE                                              */\n");
 WR("/* ================                                              */\n");
 WR("/* The following CRC lookup table was generated automagically    */\n");
 WR("/* by the Rocksoft^tm Model CRC Algorithm Table Generation       */\n");
 WR("/* Program V1.0 using the following model parameters:            */\n");
 WR("/*                                                               */\n");
 WP("/*    Width   : %1lu bytes.                                         */\n",
    (ulong) TB_WIDTH);
 if (TB_WIDTH == 2)
 WP("/*    Poly    : 0x%04lX                                           */\n",
    (ulong) TB_POLY);
 else
 WP("/*    Poly    : 0x%08lXL                                      */\n",
    (ulong) TB_POLY);
 if (TB_REVER)
 WR("/*    Reverse : TRUE.                                            */\n");
 else
 WR("/*    Reverse : FALSE.                                           */\n");
 WR("/*                                                               */\n");
 WR("/* For more information on the Rocksoft^tm Model CRC Algorithm,  */\n");
 WR("/* see the document titled \"A Painless Guide to CRC Error        */\n");
 WR("/* Detection Algorithms\" by Ross Williams                        */\n");
 WR("/* (ross@guest.adelaide.edu.au.). This document is likely to be  */\n");
 WR("/* in the FTP archive \"ftp.adelaide.edu.au/pub/rocksoft\".        */\n");
 WR("/*                                                               */\n");
 WR("/*****************************************************************/\n");
 WR("\n");
 switch (TB_WIDTH)
   {
    case 2: WR("unsigned short crctable[256] =\n{\n"); break;
    case 4: WR("unsigned long  crctable[256] =\n{\n"); break;
    default: chk_err("gentable: TB_WIDTH is invalid.");
   }
 chk_err("");

 {
  int i;
  cm_t cm;
  char *form    = (TB_WIDTH==2) ? "0x%04lX" : "0x%08lXL";
  int   perline = (TB_WIDTH==2) ? 8 : 4;

  cm.cm_width = TB_WIDTH*8;
  cm.cm_poly  = TB_POLY;
  cm.cm_refin = TB_REVER;

  for (i=0; i<256; i++)
    {
     WR(" ");
     WP(form,(ulong) cm_tab(&cm,i));
     if (i != 255) WR(",");
     if (((i+1) % perline) == 0) WR("\n");
     chk_err("");
    }

 WR("};\n");
 WR("\n");
 WR("/*****************************************************************/\n");
 WR("/*                   End of CRC Lookup Table                     */\n");
 WR("/*****************************************************************/\n");
 WR("");
 chk_err("");
}
}

/******************************************************************************/

main ()
{
 printf("\n");
 printf("Rocksoft^tm Model CRC Algorithm Table Generation Program V1.0\n");
 printf("-------------------------------------------------------------\n");
 printf("Output file is \"%s\".\n",TB_FILE);
 chkparam();
 outfile = fopen(TB_FILE,"w"); chk_err("");
 gentable();
 if (fclose(outfile) != 0)
    chk_err("main: Couldn't close output file.");
 printf("\nSUCCESS: The table has been successfully written.\n");
}

/******************************************************************************/
/*                             End of crctable.c                              */
/******************************************************************************/

20. Summary
-----------
This document has provided a detailed explanation of CRC algorithms
explaining their theory and stepping through increasingly
sophisticated implementations ranging from simple bit shifting through
to byte-at-a-time table-driven implementations. The various
implementations of different CRC algorithms that make them confusing
to deal with have been explained. A parameterized model algorithm has
been described that can be used to precisely define a particular CRC
algorithm, and a reference implementation provided. Finally, a program
to generate CRC tables has been provided.

21. Corrections
---------------
If you think that any part of this document is unclear or incorrect,
or have any other information, or suggestions on how this document
could be improved, please context the author. In particular, I would
like to hear from anyone who can provide Rocksoft^tm Model CRC
Algorithm parameters for standard algorithms out there.

A. Glossary
-----------
CHECKSUM - A number that has been calculated as a function of some
message. The literal interpretation of this word "Check-Sum" indicates
that the function should involve simply adding up the bytes in the
message. Perhaps this was what early checksums were. Today, however,
although more sophisticated formulae are used, the term "checksum" is
still used.

CRC - This stands for "Cyclic Redundancy Code". Whereas the term
"checksum" seems to be used to refer to any non-cryptographic checking
information unit, the term "CRC" seems to be reserved only for
algorithms that are based on the "polynomial" division idea.

G - This symbol is used in this document to represent the Poly.

MESSAGE - The input data being checksummed. This is usually structured
as a sequence of bytes. Whether the top bit or the bottom bit of each
byte is treated as the most significant or least significant is a
parameter of CRC algorithms.

POLY - This is my friendly term for the polynomial of a CRC.

POLYNOMIAL - The "polynomial" of a CRC algorithm is simply the divisor
in the division implementing the CRC algorithm.

REFLECT - A binary number is reflected by swapping all of its bits
around the central point. For example, 1101 is the reflection of 1011.

ROCKSOFT^TM MODEL CRC ALGORITHM - A parameterized algorithm whose
purpose is to act as a solid reference for describing CRC algorithms.
Typically CRC algorithms are specified by quoting a polynomial.
However, in order to construct a precise implementation, one also
needs to know initialization values and so on.

WIDTH - The width of a CRC algorithm is the width of its polynomical
minus one. For example, if the polynomial is 11010, the width would be
4 bits. The width is usually set to be a multiple of 8 bits.

B. References
-------------
[Griffiths87] Griffiths, G., Carlyle Stones, G., "The Tea-Leaf Reader
Algorithm: An Efficient Implementation of CRC-16 and CRC-32",
Communications of the ACM, 30(7), pp.617-620. Comment: This paper
describes a high-speed table-driven implementation of CRC algorithms.
The technique seems to be a touch messy, and is superceded by the
Sarwete algorithm.

[Knuth81] Knuth, D.E., "The Art of Computer Programming", Volume 2:
Seminumerical Algorithms, Section 4.6.

[Nelson 91] Nelson, M., "The Data Compression Book", M&T Books, (501
Galveston Drive, Redwood City, CA 94063), 1991, ISBN: 1-55851-214-4.
Comment: If you want to see a real implementation of a real 32-bit
checksum algorithm, look on pages 440, and 446-448.

[Sarwate88] Sarwate, D.V., "Computation of Cyclic Redundancy Checks
via Table Look-Up", Communications of the ACM, 31(8), pp.1008-1013.
Comment: This paper describes a high-speed table-driven implementation
for CRC algorithms that is superior to the tea-leaf algorithm.
Although this paper describes the technique used by most modern CRC
implementations, I found the appendix of this paper (where all the
good stuff is) difficult to understand.

[Tanenbaum81] Tanenbaum, A.S., "Computer Networks", Prentice Hall,
1981, ISBN: 0-13-164699-0. Comment: Section 3.5.3 on pages 128 to 132
provides a very clear description of CRC codes. However, it does not
describe table-driven implementation techniques.


C. References I Have Detected But Haven't Yet Sighted
-----------------------------------------------------
Boudreau, Steen, "Cyclic Redundancy Checking by Program," AFIPS
Proceedings, Vol. 39, 1971.

Davies, Barber, "Computer Networks and Their Protocols," J. Wiley &
Sons, 1979.

Higginson, Kirstein, "On the Computation of Cyclic Redundancy Checks
by Program," The Computer Journal (British), Vol. 16, No. 1, Feb 1973.

McNamara, J. E., "Technical Aspects of Data Communication," 2nd
Edition, Digital Press, Bedford, Massachusetts, 1982.

Marton and Frambs, "A Cyclic Redundancy Checking (CRC) Algorithm,"
Honeywell Computer Journal, Vol. 5, No. 3, 1971.

Nelson M., "File verification using CRC", Dr Dobbs Journal, May 1992,
pp.64-67.

Ramabadran T.V., Gaitonde S.S., "A tutorial on CRC computations", IEEE
Micro, Aug 1988.

Schwaderer W.D., "CRC Calculation", April 85 PC Tech Journal,
pp.118-133.

Ward R.K, Tabandeh M., "Error Correction and Detection, A Geometric
Approach" The Computer Journal, Vol. 27, No. 3, 1984, pp.246-253.

Wecker, S., "A Table-Lookup Algorithm for Software Computation of
Cyclic Redundancy Check (CRC)," Digital Equipment Corporation
memorandum, 1974.

--<End of Document>--


Yes, it is possible. (Aside from the trivial observation that if you have the string as stated in the question, then you can simply compute the CRC of the prefix.)

To rephrase your question, I have two strings A and B, where their concatenation is AB. If I have only the CRC of AB and I have the string B, can I compute the CRC of A?

You can just reverse the process of computing the CRC, using the high byte of the CRC to determine which table entry was used. It's almost as fast as computing the CRC of B. Example code for the standard CRC-32 used in zip, gzip, etc.:

/* Given the CRC of the concatenated string AB and the string B, calculate the
   CRC of A.  Placed in the public domain by Mark Adler. */

#include <stdio.h>

#define local static

/* Byte-wise CRC table for CRC-32 */
local unsigned long crc_table[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
    0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
    0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
    0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
    0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
    0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
    0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
    0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
    0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
    0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
    0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
    0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
    0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
    0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
    0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
    0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
    0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
    0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
    0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
    0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
    0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
    0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
    0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
    0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
    0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
    0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
    0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
    0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
    0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
    0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
    0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
    0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
    0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
    0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
    0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
    0x2d02ef8d
};

local unsigned char rev[256];

local void revgen(void)
{
    unsigned k;

    for (k = 0; k < 256; k++)
        rev[crc_table[k] >> 24] = k;
}

#define ONES 0xffffffff

local unsigned long revcrc(unsigned long crc,
                           const unsigned char *buf, size_t len)
{
    unsigned k;

    crc = crc ^ ONES;
    while (len--) {
        k = rev[crc >> 24];
        crc = ((crc ^ crc_table[k]) << 8) | (k ^ buf[len]);
    }
    return crc ^ ONES;
}

int main(void)
{
    unsigned long crc = 0x9ef61f95;     /* CRC-32 of "foobar" */
                                        /* CRC-32 of "foo" is 0x8c736521 */

    revgen();
    printf("0x%08lx (should be 0x8c736521)\n",
           revcrc(crc, (unsigned char *)"bar", 3));
    return 0;
}
Say you want to transmit a polynomial 𝑀(𝑥), but the recipient receives a slightly different polynomial 𝑀′(𝑥)

. We can define the "error polynomial":

𝐸(𝑥)=𝑀′(𝑥)−𝑀(𝑥)

Call the CRC-polyomial 𝑃
. Then an error will go undetected iff 𝑃 divides 𝐸, or in other words if every divisor of 𝑃 is also a divisor of 𝐸. We can therefore guarantee that certain kinds of errors get caught by analysing the divisors of 𝑃 and 𝐸

. Here are a few simple examples:

𝐸(𝑥)=𝑥𝑛+𝑚+𝑥𝑛+𝑚−1+...+𝑥𝑛=𝑥𝑛(𝑥𝑚+𝑥𝑚−1+...+1)
, a burst error, ie. 𝑚+1 flipped bits in a row. This will be caught if 𝑃 has nonzero constant term (so it has no common divisors with 𝑥𝑛) and is of degree greater than 𝑚 (since it then cannot divide a polynomial of degree 𝑚

).

𝐸(1)=1
, ie. an odd number of flipped bits. This will be caught if 𝑥+1 divides 𝑃(𝑥), since then 𝑃(1)=0

.

𝐸(𝑥)=𝑥𝑛+𝑚+𝑥𝑚=𝑥𝑛(𝑥𝑚+1)
, ie. two flipped bits 𝑚 bits appart. This will be caught if 𝑃 is a multiple of a primitive polynomial of order greater than 𝑚. So a primitive polynomial of degree 𝑑 will catch two errors if they are less than 2𝑑−1

bits appart.

So in short, using a reducible polynomial may indeed be desirable if you want to guarantee certain kinds of errors get caught.
