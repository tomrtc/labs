#pragma once


struct  spy_base
{
};

template <typename T>
struct spy : spy_base
{
  enum class ops {
    nops, copy_constructor, assigment_operator, destructor, default_constructor,
      equality_predicate, comparaison_predicate, constructor
  };
  constexpr size_t ops_size = 8;
  static double counters[ops_size];
  static const char * counters_labels[ops_size];
  static void init(size_t t_size);

  spy(const spy& t_x) 
    : value(t_x.value) {
    counters[ops::copy_constructor]++;
  }      
  spy()  {
    counters[ops::default_constructor]++;
  }
  ~spy()  {}
  
  spy&  operator=(const spy& t_x)
  {
    counters[ops::assigment_operator]++;
    value = t_x.value; 
    return *this;    
  }

  friend         
  bool operator==(const spy& t_x, const spy& t_y)
  {
    counters[ops::equality_predicate]++;
    return  t_x.value == t_y.value;   
  }
  
  friend        
  bool operator!=(const spy& t_x, const spy& t_y)
  {
    return !(t_x == t_y);  
  }
};
