size_t
remy_scan(const std::string &input,const std::string &t_pattern)
{
  const size_t input_lenght {input.size()};
  const size_t pattern_lenght {t_pattern.size()};

  size_t input_index {0};
  size_t j,k;
  bool found {false};


  size_t pattern_index {0};

do {
  while ((pattern_index < pattern_lenght)
	 && input.at(input_index) == t_pattern.at(pattern_index))
    { input_index++; pattern_index++;}
  if (pattern_index == pattern_lenght)
    {
      found = true; /*do stuff*/
      return input_index;
    }




    // the character t_pattern.at(pattern_index) is a mismatch
    input_index++;
 size_t input_sentinel {(input_lenght - pattern_lenght + pattern_index)};

    while ((input_index < input_sentinel)
	   && input.at(input_index) != t_pattern.at(pattern_index))
      { input_index++;}
    if ((input_index > input_sentinel) && !found)
      return input_index ; // not found at all

    //  input.at(input_index) == t_pattern.at(pattern_index)
    // so check the pattern on left and right.
    size_t subpattern_index {0};
    while ((subpattern_index < pattern_lenght )
	   && input.at(input_index - pattern_index + subpattern_index) == t_pattern.at(subpattern_index))
      {  subpattern_index++;}

    if (subpattern_index == pattern_lenght)
      {
	input_index =  input_index - subpattern_index + pattern_lenght;
	found = true; /*do stuff*/
	return input_index;
      }
  } while (input_index >= input_sentinel);
  return input_index;

}
One or more arguments are not correct.
// @see https://raw.githubusercontent.com/jaysi/jlib/3f7a190e1e80b270c9a07cac46aeab960a802cd1/src/memmem.c
// @see http://sourceware.org/ml/libc-alpha/2007-12/msg00000.html

/* Return the first occurrence of NEEDLE in HAYSTACK. */
inline void*
memmem(const void* haystack,
       size_t haystack_len,
       const void* needle,
       size_t needle_len) {
    /* not really Rabin-Karp, just using additive hashing */
    char* haystack_ = (char*)haystack;
    char* needle_ = (char*)needle;
    int hash = 0;       /* this is the static hash value of the needle */
    int hay_hash = 0;   /* rolling hash over the haystack */
    char* last;
    size_t i;

    if (haystack_len < needle_len) {
        return nullptr;
    }

    if (!needle_len) {
        return haystack_;
    }

    /* initialize hashes */
    for (i = needle_len; i; --i) {
        hash += *needle_++;
        hay_hash += *haystack_++;
    }

    /* iterate over the haystack */
    haystack_ = (char*)haystack;
    needle_ = (char*)needle;
    last = haystack_ + (haystack_len - needle_len + 1);
    for (; haystack_ < last; ++haystack_) {
        if (__builtin_expect(hash == hay_hash, 0) &&
                *haystack_ == *needle_ &&   /* prevent calling memcmp, was a optimization from existing glibc */
                !memcmp(haystack_, needle_, needle_len)) {
            return haystack_;
        }

        /* roll the hash */
        hay_hash -= *haystack_;
        hay_hash += *(haystack_ + needle_len);
    }

    return nullptr;
}
