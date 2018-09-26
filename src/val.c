/*2.1. Variable-Byte Coding
  With integers of varying magnitudes, a simple variable-
  byte integer scheme provides some compression.
  Variable-byte schemes are particularly suitable for stor-
  ing short arrays of integers that may be interspersed

Figure 1: C++ code for vByte's encoding (left) and decoding (right) procedure. In each case, n postings are
processed. Uncompressed postings are stored in the integer array uncompressed, compressed postings in the
byte array compressed. The actual implementation used in our experiments is slightly diferent, using pointer
arithmetic for every array access.
*/ 

int outPos = 0, previous = 0;
for (int inPos = 0; inPos < n; inPos++) {
  int delta = uncompressed[inPos] - previous;
  while (delta >= 128) {
    compressed[outPos++] = (delta & 127) | 128;
    delta = delta >> 7;
  }
  compressed[outPos++] = delta;
 }

int outPos = 0, previous = 0;
for (int outPos = 0; outPos < n; outPos++) {
  for (int shift = 0; ; shift += 7) {
    int temp = compressed[inPos++];
    previous += ((temp & 127) << shift);
    if (temp < 128) break;
  }
  uncompressed[outPos] = previous;
 }
