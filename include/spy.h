#pragma once

struct spy {
  enum class ops {
    nop, copy_constructor, assigment_operator, destructor, default_constructor, equality_predicate, comparaison_predicate, constructor
  };
  constexpr size_t ops_size = 8;
  static double counters[ops_size];
  static const char * counters_labels[ops_size];
  static void init(size_t t_size);
};
