
// first define the moste general mechanism.
template <typename T> // contrainte sur T  semi-regular
inline // mettre inline sur une ligne a part pour aider la recherche textuelle 
// sur ^swap( begin-of-line + name of function
void 
swap(T& t_l, T& t_r) // la plus generique 
{
  T temp {t_l};
  t_l = t_r;
  t_r = temp;
}
 
// swap is horribly slow when applied to any containers.
// so we do a kind of specialisation like std::vector<int> 
// an intermediate is partial specialisation std::vector<typename T>
template <typename T> // contrainte sur T  semi-regular
inline 
void 
swap(std::vector<T>& t_l, std::vector<T> t_r) // partial-specialisation
{
  // swap headers of t_l and t_r.
  // fix back pointers (if they exist).
}
 
 
 
// specialisation on unsigned integrale type.
template <typename T> // contrainte sur T   unsigned integrale type.
// signed int with xor is undefined behavior in C/C++ for the sign.
inline 
void 
swap_xor(T& t_l, T& t_r) 
{
  // t_l and t_r are different obejcts => zero!
  // expensive test!!
  if ( &t_l != &t_r) 
    { 
      t_l = t_l ^ t_r; 
      t_r = t_l ^ t_r;
      t_l = t_l ^ t_r;
    }
}



// min operator
/ first define the moste general mechanism.
template <typename T> // contrainte sur T  tottally-ordered
inline
void 
min(T& t_l, T& t_r) 
{
  // alwayes aplay natural order law 0 to 1 2 3  ... so only if t_r is the actal min then return it ! 
  // so the most common is to return t_l !
  // so in a sort context this reduce swap !!!
  // rule min must return the left value in preference.      
  if (t_r < t_l) 
    {return  t_r;}
  else 
    {
      return  t_l;
    }

}



//a generic assignment operator:
// There is a problem with this definition of assignment. If there is an
// exception during the construction, the object is going to be left in 
// an unacceptable “destroyed” state.
T& T::operator=(const T& t_rvalue)
{
  if (this != &t_rvalue) {
    this -> ~T(); // destroy object in place
    new (this) T(t_rvalue); // construct it in place
  }
  return *this;
}

// partial specialisation when contaner with size
// size is an internal information for constructionally complete
  T& T::operator=(const T& t_rvalue)
  {
    if (this != &t_rvalue) {
      if (this->length == t_rvalue.length)
        { // no exception no allocation
        }
      else 
        {
          this -> ~T(); // destroy object in place
          new (this) T(x); // construct it in place
        }
    }
    return *this;
  }
 
