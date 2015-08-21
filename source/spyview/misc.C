#include "misc.h"
#include <stdarg.h>
#include <stdio.h>

// cut and paste this guy from a website (http://www.gammon.com.au/forum/?id=2891)
string search_replace(const string& source, const string target, const string replacement)
{
  string str = source;
  string::size_type pos = 0,   // where we are now
    found;     // where the found data is
  
  if (target.size () > 0)   // searching for nothing will cause a loop
    {
    while ((found = str.find (target, pos)) != string::npos)
      {
      str.replace (found, target.size (), replacement);
      pos = found + replacement.size ();
      }
    }

  return str;
}

string str_printf(const char *str, ...)
{
  //This is limited to 4096 characters: is there a better way to do this?

  string s;
  va_list va;
  va_start(va,str);
  char buf[4096];
  vsnprintf(buf,sizeof(buf),str,va);
  va_end(va);
  s = buf;
  return s;
}

void strip_newlines(string &str)
{
  int n = str.find_first_of("\n\r");

  if (n != string::npos) 
    str[n] = 0;
}


  
