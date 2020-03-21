#pragma once
/*
 * buddy allocator oriented to parser:lexer.
 *
 * Remy tomasetto 2019.
 *
 */

#include <stdint>
#include <vector>

// template <typename ??>
class buddy_allocator {
  // Explicite construtor
  // No copy-constructors.
public:
 explicit buddy_allocator(const T& t_value) // explicit means no implicit conversion.
    : value(t_value) {}
  buddy_allocator(const buddy_allocator&)            = delete;
  buddy_allocator& operator=(const buddy_allocator&) = delete;
  // the destructor does not clean the pieces ; it is sometimes not necessary to do that if the process ends just after.
  // the clean must be explicit and checked using valgrind kind of tools.
  ~buddy_allocator()                                 = default;


public:
  void
  clean ()
    {
      std::for_each (pieces.rbegin(), pieces.rend(), /* lambda(x) delete x*/ &(::operator delete));
      pieces.clear ();
      begin_of_piece = nullptr;
      end_of_piece   = nullptr;
    }

  template <typename T>
  inline T *  get_mem()
  {
    return static_cast<T *>(buddy_get(sizeof (T)));
  }
  template <typename T>
  inline T *  get_array_mem(std::size_t dim)
  {
    return static_cast<T *>(buddy_get(sizeof (T) * dim));
  }
private:
  std::vector<std::uintptr_t> pieces         {};

  std::uintptr_t             *begin_of_piece {nullptr};
  std::uintptr_t                *end_of_piece   {nullptr};

  std::uintptr_t
  buddy_get(std::size_t t_size)
  {

    const diffptr_t amount_of_free_space = end_of_piece - begin_of_piece;
    if (amount_of_free_space > 0)
      {
	if (t_size <=  amount_of_free_space)
	  {
	    std::uintptr_t a_small_piece {begin_of_piece};
	    begin_of_piece += t_size;
	    return a_small_piece;
	  }
	else  if (t_size > BUDDY_SIZE)
	  {
	    std::byte *a_big_piece { new std::byte[t_size]};
	    pieces.push_back(a_big_piece);
	    return a_big_piece;
	  }
	else
	  {
	    std::byte *new_piece { new std::byte[BUDDY_SIZE]};
	    begin_of_piece = new_piece;
	    end_of_piece = new_piece +BUDDY_SIZE;
	    pieces.push_back(new_piece);
	  }
      }
    else
      {
	std::byte *new_piece { new std::byte[BUDDY_SIZE]};
	begin_of_piece = new_piece;
	end_of_piece = new_piece +BUDDY_SIZE;
	pieces.push_back(new_piece);
      }

A revoir les new.

};
