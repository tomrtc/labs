#include "spy.h"

const char * spy_base::counters_labels[ops_size] { "nops", "copy_constructor", "assigment_operator", "destructor", "default_constructor",
    "equality_predicate", "comparaison_predicate", "constructor"};
double spy_base::counters[];

void
spy_base::init(size_t t_size)
{
  std::fill(counters, counters + spy_base::ops_size , 0.0);
  counters[0] = double{t_size};
}
