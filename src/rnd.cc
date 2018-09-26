#include <iostream>
#include <string.h>
#include <stdio.h>



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
