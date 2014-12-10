#ifndef MISC_C
#define MISC_C

#include <string>

using namespace std;

//Some useful functions

// Why is this not part of the STL? It would be so handy...
string search_replace(const string& source, 
		      const string target, 
		      const string replacement);

// This is also notably lacking in STL. Note: this has a maximum
// string length of 4096 characters...
string str_printf(const char *str, ...)  __attribute__ ((format (printf,1,2)));  

// Strip newlines and \r
void strip_newlines(string &string);

#endif
