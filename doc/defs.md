A user-defined type is a custom-made type that you define in order to represent things in the problem
domain

built-in types like int, double
New in C++11. A common case: you need more than one constructor function and have several
member variables to set up in the same way. How do you avoid the duplication and get a single
point of maintenance?
"Delegating constructors: - if you invoke another constructor of the same class in the constructor
initializer list, that runs on the new object, and intiialization continues with the constructor body. Only
the delegating constructor invocation can appear in the initializer list - you can't have any additional
constructor initializers in the list.

Const correctness with class members
Make member functions const if they don't change the state of the object.
Member variables can be const, but if so, they can only be initialized in the constructor initializer list.
a member function is logically const - looks const to the outside world - but does its
work by modifying certain private members. For example, saving computation time by caching a
result internally.
Mark member function as const
Mark modified member variable as mutable - can be modified by a const member function.

if LHS object is not of the class type, the operator function can't be a member function.
Must define as a non-member function.
non-member functions do not have access to private data
Friend Functions
in a class declaration, you can declare a function to be a "friend" of a class - grant access to
private members.
friend <function prototype> anywhere in the class declaration

Shared Ownership with shared_ptr
The shared_ptr class template is a referenced-counted smart pointer; a count is kept of how many smart
pointers are pointing to the managed object; when the last smart pointer is destroyed, the count goes to zero, and the
managed object is then automatically deleted. It is called a "shared" smart pointer because the smart pointers all
share ownership of the managed object - any one of the smart pointers can keep the object in existence; it gets
deleted only when no smart pointers point to it any more.

How to Access the C++11 Smart Pointers.
In a C++11 implementation, the following #include is all that is needed:
#include <memory>

Std Lib algorithms are all defined as function templates that take iterators as arguments
Consequence of iterator interface: an algorithm cannot directly insert or erase elements
from the container!
In other words, an algorithm can not change the number of elements in the container!
These operations have to be done by a container member function!
Some algorithms appear to remove items from sequence containers, but not really:
remove algorithm is easy to misunderstand - "removed" items moved to end of the sequence, and
returns an iterator to the new "end"
unique algorithm moves duplicates to end, and returns a new end.
Why there is no "insert" algorithm in the std lib algorithms
copy algorithm is often a problem about this - has to be room at the destination
Filling a container can't be done just with the regular iterators unless space has already been
created at every possible iterator position
can create a vector of a certain size, then iterator can be used to fill those already-created places.
But some cute tricks available using function objects.

Function objects
Definition: an object whose class overloads the function call operator: operator(), and so can be
used like a function.
class My_function_object_class {
public:
return-type operator() (parameter-list)
{ do whatever any normal function would do}
};
My_function_object_class fo;
x = fo(args);
// an instance of the class
// use like a function!
Otherwise, just a class - can have other member variables and functions.
Especially: a constructor with parameters to provide values for the function call operator to use
importance for STL algorithms is that they work equally well with function pointers and function
objects
for_each(x.begin(), x.end(), func_ptr);
for_each(x.begin(), x.end(), func_object); or
for_each(x.begin(), x.end(), func_object_class_name() ) ; // create an unnamed object of the class
Associative containers normally take a function object class name as an optional template
parameter in the declaration to specify the ordering relation
defaults to less<T>, which is a Std. Lib. class template for a function object class that defines an
operator() that applies T's operator< between two T objects
Examples:
set<int> si; // set of ints in default operator < order
struct RevInt { bool operator() (int i1, int i2) const {return i2 < i1;} // reverse order of integers
set<int, Revint> sri; // set of ints in reverse order
map<int, string, Revint> mri; // map of ints to strings, but with the ints in reverse order
REALLY BIG advantage over function pointers is that the object can have member variables and
other member functions!
a lot more sophisticated than function pointers
Another advantage: The function code is often inlined, meaning that code using a function object
will often be faster than code doing an ordinary function call, or a call using a function pointer.
Using algorithms and adapters with ordinary functions
Algorithm doesn't care whether Function parameter is a function pointer or function object
The algorithm calls your function with the dereferenced iterator as the only argument - what if you
need more arguments? What if the first argument is not the dereferenced iterator?
Calling an ordinary function that has two arguments - e.g. first is the dereferenced iterator, the second
is something else.
No place in the for_each or other algorithms to supply the second parameter:
Could have done:
for_each_with_1_additional_arg(my_list.begin(), my_list.end(), my_function, 42)
f(*first, function_second_arg)
Instead, keep the single "slot" for the function, but use function objects to get that second argument
in there.
This is what std::bind is for:
How to access bind
Assuming you have a C++11 compiler and Standard Library, the following #includes and using directives make
the facilities described in this handout available.
#include <functional>
// the following are for convenience in this handout code
using namespace std;
using namespace std::placeholders;! // needed for _1, _2, etc.
Creating a function object with bound arguments
The bind template is an elaborate(!) function template that creates and returns a function object that “wraps” a
supplied function in the form of a function pointer. Its basic form is:
!
bind(function pointer, bound arguments)
For example, let sum3 be a function that takes 3 integer arguments and returns an int that is the sum of its
arguments:
int sum3(int x, int y, int z)
{
! int sum = x+y+z;
! cout << x << '+' << y << '+' << z << '=' << sum << endl;
! return sum;
}
Let int1 , int2 , and int3 be three int variables. Then
!
bind(sum3, int1, int2, int3)
creates and returns a function object whose function call operator takes no arguments and calls sum3 with the values
in the three integer variables, and returns an int . We can invoke the function call operator with a call with no
additional arguments in the function call argument list, as in:
!
int result = bind(sum3, int1, int2, int3)();
The final effect is as if sum3 was called as:
nt result = sum3(int1, int2, int3);
When the function object is created, a pointer to the function is stored in a member variable, and the values in the
bound arguments are copied into the function object and stored as member variable values.When the function call
operator is executed, the saved pointer is used to call the function, and the saved values are supplied as arguments to
that function. Thus bind acts to “bind” a function to a set of argument values, and produces a function object that
packages the stored values together with a pointer to the function, and enables the function to be called with fewer
arguments - in this example, none because all of the function arguments are bound to the supplied values.
To try to avoid confusion in what follows, the actual arguments required by the wrapped function ( sum3 in this
case) will be called the function arguments to distinguish them from the bound arguments supplied as the additional
arguments to the bind function template. A basic rule is that the number of bound arguments has to equal the number
of function arguments, and the order of them corresponds - the first bound argument corresponds to the first function
argument, the second to the second, and so forth.
The bound arguments can also be literal values, as in:
!
!
bind(sum3, 10, int2, int3)();
bind(sum3, 10, 20, 30)();
The bound values are copied into the function object, so if the function takes a call-by-reference argument and
modifies it, the modification will be to internal copy stored in the function object, and not the original variable.
However, C++11 includes a nifty reference wrapper class, created with the ref function template, which produces
the effect of a copyable reference. If you wrap one of the bound arguments in a reference wrapper, and the function
modifies it, the actual bound variable will get modified.
For example, suppose function mod23 takes its second and third argument by reference and modifies them:
void mod23(int x, int& y, int& z)
{
! y = y + x;
! z = z + y;
}
Then this call
bind(mod23, int1, int2, ref(int3))();
results in unchanged values for int1 and int2 , but a different value for int3 .
As a further example, suppose we have a function that takes a stream reference argument:
void write_int(ostream& os, int x)
{
! os << x << endl;
}
We can use a reference wrapper to hand the stream in by reference to the wrapped function:
bind(write_int, ref(cout), int1)();
Mixing bound and call arguments with placeholders
Now we come to the most interesting part. At the time we use the function object in a call, the final function
arguments can taken from a mixture of bound arguments and the arguments supplied in the call, which will be
termed the call arguments. This is done with special placeholders in the list of bound arguments.
If one of the bound arguments is a placeholder, the corresponding function argument is taken from the call
arguments. For example, the following would result in a call to sum3 with the same input as the above examples.
!
bind(sum3, _2, int2, _1)(int3, int1);
The placeholders in the bound argument list are the special symbols _2 and _1 . To make it possible to avoid name
collisions, these are in the special namespace std::placeholders . The using directive above allows us to refer
to them directly, instead of say std::placeholders::_1 . The call arguments are listed in the second set of
parentheses, which is simply the normal syntax for an argument list in a function call,
The placeholder notation requires some care to understand. The placeholder indicates which function argument
should be filled with which value from the call argument list. The position of the placeholder in the bound argument
list corresponds to the same position in the function argument list, and the number of the placeholder corresponds to
a value in the call argument list.
Thus, in the above example, the placeholder _2 appears first in the bound argument list, which means it specifies
the first argument to be given to sum3 . Its number, 2, specifies the second argument in the call argument list. Thus
the second call argument item will be the first function argument. Likewise, _1 in the third position means that this
argument to sum3 should be taken from the first of the supplied values in the call arguments. In this case, bind
creates a function object that can be called with two integer values, the first of which is given to sum3 as its third
argument, the second of which is given to sum3 as its first argument, and the second argument given to sum3 is the
bound value of int2 , which is copied and stored when the function object is created.
Any mixture of placeholders and ordinary arguments can appear, as long as the number of items in the bound
argument list equals the number of function arguments. In contrast, the call argument list can include extra values -
they don't all have to be used, as long as the wrapped function gets all the arguments it needs. Likewise, it can
include fewer values if some of them get used more than once. To illustrate the extremes, we could write both of the
following:
bind(sum3, _2, int2, _5)(int3, int1, int4, int5, int6);
bind(sum3, _1, _1, _1)(int1);
The first call uses the second and fifth call arguments and ignores the others. The second call uses the same call
argument for all three function arguments.
Suppose the function takes a call-by-reference parameter. If the corresponding argument is a modifiable location
(an lvalue, like a non-const variable), the function can modify it. Since mod23 modifies its second and third
arguments, if we write:
cout << int1 << ' ' << int2 << ' ' << int3 << endl;
bind(mod23, int1, _1, _2)(int2, int3);
cout << int1 << ' ' << int2 << ' ' << int3 << endl;
Both int2 and int3 will have different values in the two output statements.
We can also use constants as the call arguments, as long as the function doesn't try to modify them:
result = bind(sum3, _1, _2, _3)(100, 200, 300);
bind(mod23, int1, _1, _2)(100, 200); // compile error!
We can also use rvalues such as expressions or function calls as the call arguments, as long as the function doesn't
try to modify them:
result = bind(sum3, _1, _2, _3)(foo(), int1+3, 300);
bind(mod23, _1, int2, _2)(foo(), int3);
bind(mod23, int1, _1, _2)(foo(), int1+3); // compile error!
Using bind in algorithms
All the background has now been presented for how to use bind in the context of an algorithm like for_each
running over a container of objects. When the algorithm is executed, the dereferenced iterator has a single value, and
this single value will constitute the single call argument to the bind function object.

We can use bind to bind a function that takes many arguments to values for all but one of the arguments, and
have the value for this one argument be supplied by the dereferenced iterator. This usually means that only the
placeholder _1 will appear in the bound arguments, because there is only one value in the call argument list. The
position of _1 in the bound argument list corresponds to which parameter of the wrapped function we want to come
from the iterator.
Suppose int_list is a std::list<int> ; then:
!
for_each(int_list.begin(), int_list.end(), bind(sum3, _1, 5, 9) );
will apply the sum3 function to each integer in the list, with the first argument being the dereferenced iterator value,
as shown by the placeholder, and the constants 5 and 9 being the second and third.
The following will apply the mod23 function, with the list item being the third parameter, and will result in the
modified values being copied both into the bind function object for the second parameter, and back into the list for
the third parameter.
!
for_each(int_list.begin(), int_list.end(), bind(mod23, 3, 5, _1) );
By using the reference wrapper, we can call a function that takes a stream object by reference as one argument:
for_each(int_list.begin(), int_list.end(), bind(write_int, ref(cout), _1) );

A handy short-cut for member functions when no binding is needed
C++11 also includes a function template called mem_fn (note the spelling) which replaces the the mem_fun and
mem_fun_ref adapters in the C++98 Standard Library. It uses the same sophisticated template programming as
bind , and so is “smarter” than its deprecated predecessors. This one adapter works for both containers of objects
and containers of pointers. Like bind , mem_fn creates and returns a function object that can be used with function
call syntax to call the wrapped function, both for a supplied object and a pointer to an object.
mem_fn(&Thing::print) (t1);
mem_fn(&Thing::print) (t1_ptr);
The first line calls Thing::print for the supplied object; the second for the object being pointed to by the
supplied pointer. Notice how mem_fn is able to figure out how to do the call from the type of the supplied argument,
so the syntax is identical in both cases. When used in an algorithm like for_each , the dereferenced iterator value
will be the argument suppled to the function object to play the role of “this” object:
for_each(obj_list.begin(), obj_list.end(), mem_fn(&Thing::print));
If you don’t need to bind any arguments, mem_fn will do the job more easily than bind .

Algorithms and member functions
The situation:
You have a container of objects or pointers to objects.
You use an algorithm to iterate over the container.
The dereferenced iterator is an object or pointer to the object
You want to call a member function of the object.
The situation is different beause the first function parameter is the (hidden) "this" pointer.
The call can't be f(the object) or f(the pointer), but has to be theobject.f() or thepointer->f();
A member function adapter does this - different flavors depending on whether the dereferenced
iterator is an object reference or an object pointer.
But like ordinary functions, the first step is a function pointer, but it is a pointer to member function,
which is different from an ordinary function pointer.
See the handout for a summary of what is presented here.
Pointer-to-member-functions
Pointers to member functions are not like regular pointers to functions, because member functions
have a hidden "this" parameter as the first parameter, and so can only be called if you supply an
object to play the role of "this", and use some special syntax to tell the compiler to set up the call
using the hidden “this” parameter.
Declaring pointers-to-member-functions
You declare a pointer-to-member-function just like a pointer-to-function, except that the syntax is a
tad different: it looks like the verbose form of ordinary function pointers, and you qualify the pointer
name with the class name, using some syntax that looks like a combination of scope qualifier and
pointer.
Declaring a pointer to an ordinary function:
return_type (*pointer_name) (parameter types)
Declaring a pointer to a member function:
return_type (class_name::*pointer_name) (parameter types)
The odd-looking "::*" is correct.
Setting a pointer-to-member-function
You set a pointer-to-member-function variable by assigning it to the address of the class-qualified
function name, similar to an ordinary function pointer.
Setting an ordinary function pointer to point to a function:
pointer_name = function_name; // simple form
pointer_name = &function_name; // verbose form
Setting a member function pointer to point to a member function:
pointer_name = &class_name::member_function_name;
Using a pointer-to-member-function to call a function
You call a function with a pointer-to-member-function with special syntax in which you supply the
object or a pointer to the object that you want the member function to work on. The syntax looks
like you are preceding the dereferenced pointer with an object member selection (the “dot
“operator) or object pointer selection (the “arrow” operator).
Calling an ordinary function using a pointer to ordinary function:
pointer_name(arguments); // short form, allowed
orCalling an ordinary function
a pointer to ordinary function:


(*pointer_name)(arguments);
// the more verbose form
Calling the member function on an object using a pointer-to-member-function
(object.*pointer_name)(arguments);
or calling with a pointer to the object
(object_ptr->*pointer_name)(arguments);
Again, the odd looking things are correct: ".*" and "->*". The parentheses around the whole
pointer-to-member construction are required because of the operator precedences.
Calling a member function from another member function using pointer to member
This seems confusing but actually is just an application of the pointer to member syntax with “this”
object playing the role of the hidden this parameter. If you want a member function f of Class A to
call another member function g of class A through a pointer to member function, it would look like
this:
class A {
void f();
void g();
};
void A::f()
{
// declare pmf as pointer to A member function,
// taking no args and returning void
void (A::*pmf)();
// set pmf to point to A's member function g
pmf = &A::g;
// call the member function pointed to by pmf points on this object
(this->*pmf)(); // calls A::g on this object
}
// using a typedef to preserve sanity - same as above with typedef
// A_pmf_t is a pointer-to-member-function of class A
typedef void (A::*A_pmf_t)();
void A::f()
{
A_pmf_t p = &A::g;
(this->*p)();
}
// calls A::g on this object
Using algorithms to call member functions
The function call in the for_each function won't work - you can't call a member function that way.
Need a function object that stores the pointer-to-member-function, and then the function call operator
parameter is used as "this" object in a call using the pointer to member function.
C++11 has std::mem_fn adapter for this (see the std::bind handout) if there are no ordinary parameters to
the member function.
std::bind will handle all cases - the first parameter is the "this" object, the remainder are the other
parameters. See the bind handout.
Using algorithms with map container is a pain
The dereference iterator is a pair; often you want to work with the .second of the pair, but the
function gets the pair anyway.
Ways to solve it
std::map<string, Thing> and Thing::print() is what you want to call.
note: map<string, Thing>::value_type is a typedef for type of the items in the container, namely
std::pair<const string, Thing>. Use this Standard typedef to save typing misery!
write a helper function
void Thing_print_helper(map<string, Thing>::value_type& thePair)
{the_pair.second.print();}
for_each (my_map.begin(), my_map.end(), Thing_print_helper);
write a function object class helper
struct Thing_printer {
void operator() (std::map<string, Thing>::value_type thePair)
{thePair.second.print();}
}
for_each (my_map.begin(), my_map.end(), Thing_printer());
Range for - new in C++11
you can write a for loop that iterates through a container or built-in array in a super-compact form:
for(type variable : container) {code using variable}
The compiler will generate the code corresponding to a for loop that iterates through the whole
container and assigns the dereferenced iterator value to the variable on each iteration.
The type of the variable needs to match the type of the dereferenced iterator.
auto can be used here to automatically declare the type
The container can also be a built-in array if the declaration of the array is in scope.
This is implemented in terms of two templates: std::begin() and std::end().
if the container is a built-in array, they return a pointer to the first cell of the array, and a pointer to one
past the last cell of the array.
if the container is a container class (like vector<>), they return the iterator provided by the .begin()
and .end() member functions.
This works for any container

GENERAL IDEA: PROVIDE A SEPARATE FLOW OF CONTROL FOR ERROR SITUATIONS
most of code can be written as if nothing would go wrong
separate flow of control if something does
Exception concept - an fairly old idea, developed and refined before in e.g. LISP, later C++
Exception Concept
Basic syntax:
class X {
... whatever you want
};
try {
bunch of statements
somewhere in here, or in the functions that are called:
throw X(); // create and throw an X object
}
catch (X& x) { // catch using a reference parameter is recommended
do something with an X exception
}
... continue processing
e.g. try again
What happens
Function calls proceed normally
but if a "throw" is executed
stack is "unwound" back up to try block that is followed by the matching catch
the catch block is executed
execution then continues after the final catch
unwinding the stack is equivalent to forcing a return from the function at the point of the throw, and for
every function in the calling stack up to the try block
like a return statement magically appears at the point of the throw, and after the call of each
function along the way.
control is transferred from the point of the throw to the matching catch, with all functions in between
immediately returning
In standard C++, memory allocation failure can be caught like this:
catch the bad_alloc exception
#include <new>
try {
code that might allocate too much memory
}
catch (bad_alloc& x) {
cout << "memory allocation failure" << endl;
catch (bad_alloc& x) {
cout << "memory allocation failure" << endl;
// do whatever you want
}a class hierarchy of exception types
catch by base class type, etc
Exceptions can be in class hierarchies, can catch with the base:
Tip: always catch an exception object by reference ...
not a bad idea: inherit from std::exception, override virtual char * what() const. Gives a uniform error
reporting facility for all exceptions:
try {
/* stuff */
}
catch (exception& x)
{
cout << x.what() << endl;
}
catch ( ... )
{
cout << "unknown exception caught" << endl;
}
derived classes can build an internal string having whatever info in it that you want, return the .c_str()
for what()
Standard library has some standard exception types that are thrown
What happens if something goes wrong with constructing an object?
e.g. if getting initialization data from a file, what if some of the data is invalid?
note that constructors have no return value, so no obvious way to signal that it didn't work.
without exceptions - only way to handle is to quit trying to construct the object and set some kind of
member variable to say the object is no good, and then insist that client code check it before using
the object.
object is actually a zombie - not fully initialized, but walks anyway? What do you do with it?
better approach is to throw an exception - forces an exit from the constructor function
What happens if an exception is thrown during construction of an object?
Any members that were successfully constructed are destroyed - their destructors are run
Any memory for the whole object that was allocated is deallocated.
Control leaves the constructor function at the point of the exception
if an exception is thrown from a constructor, object does not exist!
What is the status of Thing t and p's pointed-to Thing in the catch block and afterwards?
Thing t is out of scope now - can't refer to it anyway!
Guru Sutter describes this in terms of the Monty Python dead parrot sketch.
There never was a parrot - it was never alive!
Both t, and p's pointed-to thing never existed!
p does not point to a valid object, or even a usable object - actually no object at all - don't try to
use it in any way, shape, or form
What about throwing an exception in a destructor?
if an exception gets thrown out of a destructor, and we are already unwinding the stack, what is
system supposed to do with the TWO exceptions now going on? rule: shouldn't happen!
if happens, terminate - exception handling is officially broken!
if exceptions might get thrown during destruction, you must catch them and deal with them yourself
inside the destructor function before returning from it
memory leaks while unwinding the stack
class Thing {
public:
Thing ();
~Thing() {does something}
};
void foo ()
{
int * p = new int[10000];
// use p for stuff
// use p some more
Thing t;
Thing * t_ptr;
t_ptr = new Thing;
goo();
...
delete[] p; // we're done with the array
delete t_ptr;
//done with the Thing
}
Problem:
if goo throws an exception, foo is forced to return from the point of the call.
normal action on a return is to run the destructor function on local variables.
t is a Thing, so its destructor ~Thing() is run
t_ptr is a pointer to Thing - it is popped off the stack, but because POINTERS ARE A BUILT-IN
TYPE (like int) t_ptr doesn't have distruconstructor, so the memory it is pointing to won't get
deallocated. - can have a memory leak
same situation with p and the block of 10000 ints we allocated
Fixes
catch all exceptions in foo and deallocate as needed
better - put such pointers inside an object with a destructor - "smart pointer" - or even vector<int> and
make them safer, better - later
