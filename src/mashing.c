

bool
store(int key)
{
  constexpr int empty_slot{0};

  int    key_hash_value{hash(0, key)};

 
  if (table[key_hash_value] == empty_slot)  // empty slot.
    return store_element(key_hash_value, key);
  else
    {
      int key_in_busy_slot {table[key_hash_value]};
      int key_in_busy_slot_hash_value{hash(0, key_in_busy_slot)};
      if (key_in_busy_slot_hash_value != key_hash_value) // wrong hash so move to another location. brent??
        {
          // todo store(key_in_busy_slot);
          link[key_hash_value, g(key_in_busy_slot)] = 0;
          store_element(key_hash_value, key);
          key = key_in_busy_slot;
          key_hash_value = key_in_busy_slot_hash_value;
        }
      else
        {
          int g_value{g(key)};
          int link_hash_value{key_hash_value};
          int current_key{key};
          int current_key_hash_value;
          int inc_level{0};
        l1:
          current_level = link[link_hash_value,level];
          current_key_hash_value = current_key_slot;
          if(current_key_slot == 0)
            {
              do {
                current_level++;
                inc_level++;
                if (level > MAXLEVEL) return false; //table full
                else
                  current_key_hash_value = hash(level, current_key);
              } while (table[current_key_hash_value] != empty_slot);
            }
          else // get he trace of accessible links.
            {
            l2:
              if ((current_level + inc_level) > MAXLEVEL)
                return false; //table full 
              else
                {
                  current_key_hash_value = hash(current_level + inc_level, current_key);
                  if (hash(0, current_key))
                }
            }
        }
    }
  
}
