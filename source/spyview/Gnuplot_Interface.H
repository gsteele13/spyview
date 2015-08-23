#ifndef __GNUPLOT_INTERFACE__
#define __GNUPLOT_INTERFACE__
#include <stdio.h>
#include <string>
#include <assert.h>
#include <errno.h>
#include <vector>
#include "message.h"

/* With the need for bi-directional communications with gnuplot,
   it's nice to encapsulate away the added complexity.
*/
class Gnuplot_Interface
{
public:
  static bool debugMode; // Set to true to get a dump of gnuplot communications.
  static std::string gnuplot_cmd;
  Gnuplot_Interface(bool bidirectional=false);
  ~Gnuplot_Interface();

  bool bidirectional;
  FILE *out;   // For writing to gnuplot
  FILE *in;    // For reading from gnuplot
#ifdef WIN32
  char pname[1024];
#else
  pid_t child; // Child pid 
#endif
  void cmd(const char *str, ...) __attribute__((format (printf,2,3))); // Get type checking on cmd arguments
  void eat(); // Read everything that can be read on in and dump it into the bit bucket.
  char *resp(char *buf, size_t size); // Return as fgets

  bool open();
  void close(); 
  void init();
  inline bool isopen() { return out != NULL; };

  class point
  {
  public:
    double x,y;
    point(double xp=0, double yp=0) : x(xp), y(yp) {};
    point(const point &p) : x(p.x), y(p.y) {};
  };

  void prepmouse(); // Clear the label list, set up to capture mouse clicks.
  void getmouse(std::vector<point> &p); // Get the current list of labels.
  double getvariable(const char *name); // Return NAN if not defined

  static const char *tmpdir(); // This belongs elsewhere, but...

  static std::string escape(std::string st); // Escape a string so it gets into the gnuplot title safely.
};

#endif
