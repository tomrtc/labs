// The spaceship operator <=>  return a scalar -1 iff < , 0 iff == , 1 iff >
// https://groups.google.com/forum/#!topic/comp.lang.c++.moderated/bgGrHG_Y_Pw
// https://groups.google.com/forum/#!topic/comp.lang.c++.moderated/skkmFpvFRtA
#include <functional>
template <typename T>
ptrdiff_t
compare<T>(const T& t_left_value, const T& t_right_value)
{
    if (std::less(t_left_value, t_right_value))
       return -1;
    else if (std::less(t_right_value, t_left_value))
       return 1;
    else
       return 0;
}


template<typename T>
ptrdiff_t
compare_default<T>(const T& t_left_value, const T& t_right_value)
// return a scalar -1 iff < , 0 iff == , 1 iff >
// use only <
{ 
  return (t_left_value < t_right_value) ? -1 : (t_right_value < t_left_value) ? 1 : 0; 
}

template<typename T>
ptrdiff_t
compare_alternative<T>(const T& t_left_value, const T& t_right_value)
// return a scalar -1 iff < , 0 iff == , 1 iff >
{ // use < and >
  return (t_left_value < t_right_value) ? -1 : (t_right_value > t_left_value); 
}

template<typename T>
ptrdiff_t
compare_generic<T>(const T& t_left_value, const T& t_right_value)
// return a scalar -1 iff < , 0 iff == , 1 iff >
{ 
  if  (t_left_value != t_right_value)
    return  (t_left_value < t_right_value) ? -1 : 1;
  else
    return 0;
}



template<typename T>
struct equality_operators
{
  friend bool operator==(const T& t_left_value, const T& t_right_value) { return compare(t_left_value, t_right_value) == 0; }
  friend bool operator!=(const T& t_left_value, const T& t_right_value) { return compare(t_left_value, t_right_value) != 0; }
};

//--------------------------------------------------------------------------
template<typename T>
struct ordered_operators : public equality_operators<T>
{
  friend bool operator< (const T& t_left_value, const T& t_right_value) { return compare(t_left_value, t_right_value) <  0; }
  friend bool operator<=(const T& t_left_value, const T& t_right_value) { return compare(t_left_value, t_right_value) <= 0; }
  friend bool operator> (const T& t_left_value, const T& t_right_value) { return compare(t_left_value, t_right_value) >  0; }
  friend bool operator>=(const T& t_left_value, const T& t_right_value) { return compare(t_left_value, t_right_value) >= 0; }
};




/// array_pod_sort - This sorts an array with the specified start and end extent.
/// This is just like std::sort, except that it calls qsort instead of using an inlined template.
/// qsort is slightly slower than std::sort, but most sorts are not performance critical in LLVM
/// and std::sort has to be template instantiated for each type, leading to significant measured
/// code bloat.  This function should generally be used instead of std::sort where possible.
////// This function assumes that you have simple POD-like types that can be compared with operator<
/// and can be moved with memcpy.  If this isn't true, you should use std::sort.
///c: strcmp (strcoll, strcasecmp...) qsortbsearch
// c++:
//std::char_traits<T>::compare
//std::basic_string<T>::compare
//std::collate<T>::compare
//std::lexicographical_compare
//std::sub_match<It>::compare

template <typename T>
void
array_pod_sort(T *start, T *end, int (*compare)(const T *, const T *))
{
  // Don't dereference start iterator of empty sequence.
  if (start == end) return;
  std::qsort(/* base   */ start,
             /* nelem  */ end - start,
             /* width  */ sizeof *start,
             /* compar */ reinterpret_cast<int (*)(const void *, const void *)>(compare));
}


class MyClass {
    int a, b, c, d;
public:
    auto tied() const {
        return std::tie(a,b,c,d);
    }
    bool operator< (const MyClass& rhs) const {
        return tied() < rhs.tied();
    }
};

... array_pod_sort_comparator<MyClass> ...

    template <typename T, typename U>
    int spaceship(const T&, const U&);

    template <typename... T, typename... U>
    int spaceship(const tuple<T...>&, const tuple<U...>&);


template <typename T, typename U>
    int spaceship(const T& x, const U& y)
    {
        return (x < y) ? -1 : (y < x) ? 1 : 0;
    }

int spaceship(const string& x, const string& y)
    {
        int r = x.compare(y);
        return (r < 0) ? -1 : (r > 0);
    }
template <class _Tp, class _Up, size_t ..._Ip>
constexpr int __tuple_spaceship(const _Tp& __x, const _Up& __y,
                                const index_sequence<_Ip...>&)
{
    int __r = 0;
    std::initializer_list<int> x = {
        (__r != 0) ? 0 : (__r = spaceship(get<_Ip>(__x), get<_Ip>(__y))) ...
    };
    return __r;
}

template <class ..._Tp, class ..._Up>
constexpr int spaceship(const tuple<_Tp...>& __x, const tuple<_Up...>& __y)
{
    static_assert(sizeof...(_Tp) == sizeof...(_Up), "");
    return __tuple_spaceship(__x, __y, make_index_sequence<sizeof...(_Tp)>{});
}

//Recommended reading: “Towards Painless Metaprogramming” http://ldionne.github.io/mpl11-cppnow-2014/
