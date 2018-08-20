#pragma once


struct  spy_base
{
   enum class ops {
    nops, copy_constructor, assigment_operator, destructor, default_constructor,
      equality_predicate, comparaison_predicate, constructor
  };
  constexpr size_t ops_size = 8;  // there is no builtin method to have the number of enums ; think enum_value = 12.
  static const char * counters_labels[ops_size];
  static double counters[ops_size];

  static void init(size_t t_size);
};



template <typename T>
struct spy : spy_base
{
 
  

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
