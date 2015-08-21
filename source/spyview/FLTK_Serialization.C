#include <vector>
#include "FLTK_Serialization.H"

static std::vector<chooser_equality_t> eqs;
chooser_equality_t chooser_equality = chooser_equality_exact;
void push_chooser_equality(chooser_equality_t n)
{
  eqs.push_back(chooser_equality);
  chooser_equality = n;
}
void pop_chooser_equality()
{
  chooser_equality = eqs.back();
  if(eqs.size() > 1)
    eqs.pop_back();
}

bool chooser_equality_exact(std::string s1, std::string s2)
{
  return s1 == s2;
}

bool chooser_equality_noextension(std::string s1, std::string s2)
{
  bool res;
  res=s1.substr(0,s1.find_last_of('.')) == s2.substr(0,s2.find_last_of('.'));
#if FLTK_SERIALIZATION_DEBUG
  printf("Comparing %s to %s without extensions: %s\n",
	 s1.c_str(),s2.c_str(),res ? "true" : "false");
#endif
  return res; 
}
