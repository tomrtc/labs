#include <iostream>
#include <string.h>
#include <stdio.h>

/**
 * @brief Solution using a bitmask to mark the seen characters.
 * @return true if the input string has duplicate characters, false otherwise.
 * @note Complexity: O(n) in time, O(1) in space, where n is the string length.
 * @note This implementation assumes all characters are in the range [a-z].
 */
bool has_duplicates_1(const std::string& str)
{
    unsigned int chars_seen = 0;

    for (const char c : str)
    {
        /* if the character c has already been seen */
        if (chars_seen & (1 << (c - 'a')))
        {
            return true;
        }
        chars_seen |= 1 << (c - 'a');
    }

    return false;
}
/**
 * @brief Solution using a bitmask to track seen characters (the input string is
 *        allowed to contain any valid ASCII characters).
 * @return A reference to the input string with all duplicate characters
 *         removed.
 * @note Complexity: O(n) in time, O(1) in space, where n is the string length.
 */
std::string& remove_duplicates_2(std::string& str)
{
    if (str.size() <= 1)
    {
        return str;
    }

    std::bitset<128> chars_seen{};

    /* next writing position (the first character is skipped) */
    size_t i = 1;

    /* the first character is marked as seen and skipped */
    chars_seen[str[0]] = true;

    for (size_t j = 1; j < str.size(); ++j)
    {
        if (chars_seen[str[j]] == false)
        {
            chars_seen[str[j]] = true;

            str[i] = str[j];
            ++i;
        }
    }

    /* all characters in str[i..str.size()) are duplicates, discard them */
    str.resize(i);
    str.shrink_to_fit();

    return str;
}

/**
 * @brief Solution which checks if str2 appears in (str1 + str1).
 * @return true if str2 is a rotation of str1, false otherwise.
 * @note Complexity: O(n) in both time and space, where n is the length of
 *       the strings (if not the same, one cannot be a rotation of the other).
 */
bool is_rotation_1(const std::string& str1, const std::string& str2)
{
    if (str1.size() != str2.size())
    {
        return false;
    }

    return (str1 + str1).find(str2) != std::string::npos;
}
int
main (int argc, char **argv)
{
  unsigned int mR, mS, mB;

  mR = 1;
  mS = mB = 0;

  for (int i =0; i < 1024*1024*128; i++)
    {
      mR = mR * 5;
      mR =  mR & 0xfffffffc;
      mS = mR >> 4;
      mB = mS & 0x7f;
      if ((i % 0x8000) == 0)
        printf(" R[%d] = %08x -- S(%d) B(%d)\n", i, mR, mS, mB );
    }
}
