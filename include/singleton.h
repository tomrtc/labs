#pragma once

// most simple code does nothing
// no default compile generated

// template means generic ; a vay to construct other Type function
// typename means builtin types or user-defined types class
// convention formal parametters of template uses capital letters.
template <typename T>
// require T a semi-regular type or  regular type or totally-ordered type.
// Concepts : disjonction of concepts T is like thos or like that or even like this.
// the compiler generate code only when the functionality is used so it means that
// the disjunctive definition of the three T{semi,regular,ordered} are going to work perfetly
// singleton<T> operator== is defined only if T operator== is defined ; the same apply to ordered.

class singleton {
  T     value; // convention is value.
  // both equivalent:
  // typedef T value_type;
  // using value_type = T;
  // conversion from T and to T.
  explicit singleton(const T& t_value) // explicit means no implicit conversion.
    : value(t_value) {}

  explicit operator T() const  { return value;}
  // semi-regular type
  // operations : definition an almost regular type without equality.
  // idiomatic use of semiregular :
  //  T a{b}; (1) <=> T a = b; (2) <=> T a; a = b; (3)  b is of type T.
  //  1:  copy-constructor
  //  3:  default-constructor + assigment
  //  2:  naively like 3 but actually is 1.
  // copy-constructor
  singleton(const singleton& t_x) // x convention for variables
    : value(t_x.value) {}         // default implementation by compiler : explicit declaration 
  // default-constructor
  singleton()  {}                 // default value for T ?  no because default-constructor is meant to be folowed by assigment so minimal work
                                  // a no meaningful value is going to write anyway.
  // default implementation by compiler : explicit declaration
  // singleton() : value{} {}
  // WARNING : explicit declaration is done only if there is no other constructor.
  // class without members have no constructors by definition.
  
  // assigment
  // default implementation by compiler : explicit declaration
  singleton&  operator=(const singleton& t_x)
  {
    value = t_x.value; // assigment do the assigment ; assigment of singleton<T> is built upon assigment of T.
    return *this;      // support x = y = z;
  }
  // do you need a test that t_x and this has the same address which means that this is the same object. effective-c++ scott meyer
  // bad idea ; the common case is slow down by a conditional for a very rare condition.
  // side note : assigment is more and more efficient whereas conditional is more and more slow du to pipeline and cache evolution.
  // destructor
  ~singleton()  {}   // default implementation by compiler : explicit declaration
  // is the destructor must be virtual ? effective-c++ scott meyer
  // no because a virtual destructor is need only when another virtual is there.
  // If the need is not there and a virtual destructor is set up then the cost is vtable etc : enormous cost in size and runtime!!

  // regular type
  // if T is regular then the folowing code is defined 
  // equality operator  should be an non-member function with const parameters to be const-correct.
  friend          // by saying friend the non-member operator is defined ; friend are easy to write inside but not outside.
  bool operator==(const singleton& t_x, const singleton& t_y)
  {
    return  t_x.value == t_y.value;    // rely on T equality directly
  }
  // non-equality operator should be an non-member function with const parameters to be const-correct.
  // it MUST go with equality to ensure the semantic meaning of equality that imply inequality a==b => a!=b !!! 
  friend          // by saying friend the non-member operator is defined ; friend are easy to write inside but not outside.
  bool operator!=(const singleton& t_x, const singleton& t_y)
  {
    return !(t_x == t_y);  // call equality of singleton<T>
    // return  t_x.value == t_y.value; why not rely on T non-equality directly?
    // could be valid but it is less general and could be problematic.
  }
  
  // totally-ordered type : < , > ; <= ; >= .
  // if T is totally-ordered  then the folowing code is defined
  // in C++/STL the < operator/ std::less is the first one because this offer ascending ordering by default witch respect rule of less surprise.
  friend       
  bool operator<(const singleton& t_x, const singleton& t_y)
  {
    return  t_x.value < t_y.value;     // rely on T < directly
  }
   friend       
  bool operator>(const singleton& t_x, const singleton& t_y)
  {
    return  t_y < t_x;    // just swap y,x and done!
  }
   friend       
  bool operator<=(const singleton& t_x, const singleton& t_y)
  {
    return  !(t_y < t_x);    // just swap and negate!
  }
   friend       
  bool operator>=(const singleton& t_x, const singleton& t_y)
  {
    return !(t_x < t_y);    
  }
}; // this is the important semicolon ; it is used to terminate the class declaration ; for instance 'T' left the lexical scope .


// C++ is strongly typed.
// Strong typing : everything is typed and conversion transformation is required.
// But for historical reason; implicitly conversion was introduced in C because template was not available
// so a poor man templating by implicit conversion has been setup that avoid to write a bunch of conversion C function.
// if a conversion from user-defined type exist the compiler look a the first level
// in addition the C buildin conversion are applied if class is convertible to int for instance.
// std::cin << 42;
// std::cin user-convert to ptr ; builtin convert to bool ; bool to int 0 ; 0 shift by 42!!
// In C++ as a result the introduction of explicit keyword.
// the rule is to avoid implicit conversion the more you can with the help of 'explicit' keyword but
// this not enought ; so do not rely of implict converion.

// Conversion 
