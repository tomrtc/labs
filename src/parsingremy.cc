size_t
remy_scan(const std::string &input,const std::string &t_pattern)
{
  const size_t input_lenght {input.size()};
  const size_t pattern_lenght {t_pattern.size()};

  size_t input_index {0};
  size_t j,k;
  bool found {false};

  
  size_t pattern_index {0};

do {	
  while ((pattern_index < pattern_lenght)
	 && input.at(input_index) == t_pattern.at(pattern_index))
    { input_index++; pattern_index++;}
  if (pattern_index == pattern_lenght)
    {
      found = true; /*do stuff*/
      return input_index;
    }
  
 
  
  
    // the character t_pattern.at(pattern_index) is a mismatch
    input_index++;
 size_t input_sentinel {(input_lenght - pattern_lenght + pattern_index)};
   
    while ((input_index < input_sentinel)
	   && input.at(input_index) != t_pattern.at(pattern_index))
      { input_index++;}
    if ((input_index > input_sentinel) && !found)
      return input_index ; // not found at all

    //  input.at(input_index) == t_pattern.at(pattern_index)
    // so check the pattern on left and right.
    size_t subpattern_index {0};
    while ((subpattern_index < pattern_lenght )
	   && input.at(input_index - pattern_index + subpattern_index) == t_pattern.at(subpattern_index))
      {  subpattern_index++;}

    if (subpattern_index == pattern_lenght)
      {
	input_index =  input_index - subpattern_index + pattern_lenght;
	found = true; /*do stuff*/
	return input_index;
      }
  } while (input_index >= input_sentinel);
  return input_index;

}
