Fibonacci Hashing

The final variation of hashing to be considered here is called the Fibonacci hashing method  . In fact, Fibonacci hashing is exactly the multiplication hashing method discussed in the preceding section using a very special value for a. The value we choose is closely related to the number called the golden ratio.

The golden ratio  is defined as follows: Given two positive numbers x and y, the ratio tex2html_wrap_inline62424 is the golden ratio if the ratio of x to y is the same as that of x+y to x. The value of the golden ratio can be determined as follows:

eqnarray11253

There is an intimate relationship between the golden ratio and the Fibonacci numbers . In Section gif it was shown that the tex2html_wrap_inline58453 Fibonacci number is given by

displaymath62416

where tex2html_wrap_inline60483 and tex2html_wrap_inline60485!

In the context of Fibonacci hashing, we are interested not in tex2html_wrap_inline62440, but in the reciprocal, tex2html_wrap_inline62442, which can be calculated as follows:

eqnarray11271

The Fibonacci hashing method is essentially the multiplication hashing method in which the constant a is chosen as the integer that is relatively prime to W which is closest to tex2html_wrap_inline62448. The following table gives suitable values of a for various word sizes.

W 	a
16 	40503
32 	2654435769
64 	11400714819323198485

Why isspecial? It has to do with what happens to consecutive keys when they are hashed using the multiplicative method. As shown in Figure gif, consecutive keys are spread out quite nicely. In fact, when we use tex2html_wrap_inline62462 to hash consecutive keys, the hash value for each subsequent key falls in between the two widest spaced hash values already computed. Furthermore, it is a property of the golden ratio, tex2html_wrap_inline62440, that each subsequent hash value divides the interval into which it falls according to the golden ratio!
 Let’s say our hash table is 1024 slots large, and we want to map an arbitrarily large hash value into that range. The first thing we do is we map it using the above trick into the full 64 bit range of numbers. So we multiply the incoming hash value with 2^{64}/\phi \approx 11400714819323198485. (the number 11400714819323198486 is closer but we don’t want multiples of two because that would throw away one bit) Multiplying with that number will overflow, but just as we wrapped around the circle in the flower example above, this will wrap around the whole 64 bit range in a nice pattern, giving us an even distribution across the whole range from 0 to 2^{64}. To illustrate, let’s just look at the upper three bits. So we’ll do this:
1
2
3
4

size_t fibonacci_hash_3_bits(size_t hash)
{
    return (hash * 11400714819323198485llu) >> 61;
}
What we see here is that fastrange throws away the lower bits of the input range. It only uses the upper bits. I had used it before and I had noticed that a change in the lower bits doesn’t seem to make much of a difference, but I had never realized that it just completely throws the lower bits away. This picture totally explains why I had so many problems with fastrange. Fastrange is a bad function to map from a large range into a small range because it’s throwing away the lower bits.

Going back to Fibonacci hashing, there is actually one simple change you can make to improve the bad pattern for the top bits: Shift the top bits down and xor them once. So the code changes to this:
1
2
3
4
5

size_t index_for_hash(size_t hash)
{
    hash ^= hash >> shift_amount;
    return (11400714819323198485llu * hash) >> shift_amount;
}

It’s almost looking more like a proper hash function, isn’t it? This makes the function two cycles slower, but it gives us the following picture:
inline int8_t log2(size_t value)
{
    static constexpr int8_t table[64] =
    {
        63,  0, 58,  1, 59, 47, 53,  2,
        60, 39, 48, 27, 54, 33, 42,  3,
        61, 51, 37, 40, 49, 18, 28, 20,
        55, 30, 34, 11, 43, 14, 22,  4,
        62, 57, 46, 52, 38, 26, 32, 41,
        50, 36, 17, 19, 29, 10, 13, 21,
        56, 45, 25, 31, 35, 16,  9, 12,
        44, 24, 15,  8, 23,  7,  6,  5
    };
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return table[((value - (value >> 1)) * 0x07EDD5E59A4E28C2) >> 58];
}
inline size_t next_power_of_two(size_t i)
{
    --i;
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    i |= i >> 16;
    i |= i >> 32;
    ++i;
    return i;
}
struct fibonacci_hash_policy
{
    size_t index_for_hash(size_t hash, size_t /*num_slots_minus_one*/) const
    {
        return (11400714819323198485ull * hash) >> shift;
    }
    size_t keep_in_range(size_t index, size_t num_slots_minus_one) const
    {
        return index & num_slots_minus_one;
    }

    int8_t next_size_over(size_t & size) const
    {
        size = std::max(size_t(2), detailv3::next_power_of_two(size));
        return 64 - detailv3::log2(size);
    }
    void commit(int8_t shift)
    {
        this->shift = shift;
    }
    void reset()
    {
        shift = 63;
    }

private:
    int8_t shift = 63;
};
