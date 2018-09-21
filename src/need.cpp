#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>

using namespace std;

struct Foo {
  int x;
  // constexpr constructor
  constexpr Foo(int x_) : x(x_) { } 
  // decltype; constexpr methods
  constexpr auto just_foo_it()
  -> decltype(1 + 1) {
    return x - x;
  }
};

// constexpr and auto-typed variables; initializer lists
constexpr auto f = Foo { 42 };

// constexpr overloaded operator; initializer lists
constexpr Foo operator+(const Foo& lhs, const Foo& rhs) {
  return Foo { lhs.x + rhs.x };
}

// lambda
auto func = [](int x){ return x; };

int main() {
  // auto-typed variables; initializer lists
  auto v = vector<int> { 1, 2, 3, 4, 5 };
  // ranged for-loops
  for (auto elem : v) {
    cout << elem << endl;
  }
  cout << endl;

  // auto-typed variables; unordered maps
  auto table = unordered_map<string, string> { };
  table["name"] = string { "Aaron" };
  table["pet"] = string { "lizard" };
  table["car"] = string { "Volvo" };
  // ranged for-loops
  for (auto& kv : table) {
    cout << kv.first << ":\t" << kv.second << endl;
  }
  cout << endl;

  cout << "Just fooing it!" << endl;
  cout << f.just_foo_it() << endl << endl;

  // constexpr overloaded operator application
  constexpr auto g = f + Foo { 10 };
  cout << "Just fooing it again!" << endl;
  cout << g.just_foo_it() << endl;
  // return g.just_foo_it();

  if (std::regex_match ("subject", std::regex("(sub)(.*)") ))
    std::cout << "string literal matched\n";

  std::string s ("subject");
  std::regex e ("(sub)(.*)");
  if (std::regex_match (s,e))
    std::cout << "string object matched\n";

  if ( std::regex_match ( s.begin(), s.end(), e ) )
    std::cout << "range matched\n";

  std::cmatch cm;    // same as std::match_results<const char*> cm;             
  std::regex_match ("subject",cm,e);
  std::cout << "string literal with " << cm.size() << " matches\n";

  std::smatch sm;    // same as std::match_results<string::const_iterator> sm;  
  std::regex_match (s,sm,e);
  std::cout << "string object with " << sm.size() << " matches\n";

  std::regex_match ( s.cbegin(), s.cend(), sm, e);
  std::cout << "range with " << sm.size() << " matches\n";

  // using explicit flags:                                                      
  std::regex_match ( "subject", cm, e, std::regex_constants::match_default );

  std::cout << "the matches were: ";
  for (unsigned i=0; i<sm.size(); ++i) {
    std::cout << "[" << sm[i] << "] ";
  }

  std::cout << std::endl;
  return 0;

}