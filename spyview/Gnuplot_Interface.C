#include "Gnuplot_Interface.H"
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include "myboost.h"
#include <boost/regex.hpp>
#include "../config.h"
#include "message.h"
#ifdef WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
using namespace boost;

#ifdef WIN32
std::string Gnuplot_Interface::gnuplot_cmd = "pgnuplot";
#else
std::string Gnuplot_Interface::gnuplot_cmd = "gnuplot 2>/dev/null";
#endif

#define DEBUG Gnuplot_Interface::debugMode
bool Gnuplot_Interface::debugMode = false;

Gnuplot_Interface::Gnuplot_Interface(bool bi) : bidirectional(bi)
{  
  out = NULL;
  in = NULL;
#ifndef WIN32
  child=0;
#endif
}


void Gnuplot_Interface::init() // Platform independent init
{
#ifdef WIN32
  // Bug in gnuplot 4.0 only; need set mouse to be able to use rulers.
  cmd("set mouse\n");
  // Bug in windows gnuplot 4.2 with overlaying of cursor
  // position with x label
  cmd("set size 1,.95; set origin 0,.05\n");
#endif
  cmd("gexp(x)=x<700?exp(x):exp(700)\n"); // Gnuplot calls exp(large number) NAN, so 1/exp(x) can be badly behaved.
}


#ifdef WIN32
bool Gnuplot_Interface::open()
{
  static int instance=0;
  close();
  out = popen(gnuplot_cmd.c_str(), "w");
  if(bidirectional)
    {
      snprintf(pname,sizeof(pname),"%s.%d.%d.gptalk",tmpdir(),getpid(),instance++);
      cmd("set print \'%s\'\n",pname);
      cmd("print \"1\"\n");
      for(int i = 0; i < 100; i++)
	if((in = fopen(pname,"r")) != NULL) // Give gnuplot some time to open the window.
	  break;
	else
	  Sleep(10);
      if(in == NULL)
	info("Unable to open bi-directional comms with gnuplot via %s: %s\n",pname,strerror(errno));
      eat();
    }
  init();
  return out != NULL;
}

void Gnuplot_Interface::close()
{
  info("gnuplot close called\n");
  if(in) 
    {
      fclose(in);
      unlink(pname);
    }
  if(out) 
    {
      info("closing file handle %d\n", out);
      pclose(out);
      in = NULL;
      out = NULL;
    }
}

#else // UNIX
  bool Gnuplot_Interface::open()
    {
      close();

      if(!bidirectional)
	{
	  in = NULL;
	  out = popen(gnuplot_cmd.c_str(), "w");
	}
      else
	{
	  // This is unix magic.  Windows works differently (brokenly).
	  int in_pipe[2];   // For reading from gnuplot
	  int out_pipe[2];  // For writing to gnuplot
	  assert(pipe(in_pipe) == 0); 
	  assert(pipe(out_pipe) == 0);
	  if((child = fork()) == 0) 
	    { // We're the child
	      assert(::close(0) == 0);
	      assert(dup(out_pipe[0]) == 0);
	      assert(::close(out_pipe[1]) == 0);
	      assert(::close(2) == 0); // Gnuplot prints all it's interesting output to stderr!
	      assert(dup(in_pipe[1]) == 2);
	      execlp("gnuplot","gnuplot", NULL); // This fork terminates here.
	      fprintf(stderr,"Error execing gnuplot: %s\n", strerror(errno));
	      exit(1);
	    }
	  out = fdopen(out_pipe[1],"w");
	  if(out == NULL)
	    {
	      fprintf(stderr,"Unable to open pipe for writing: %s\n",strerror(errno));
	      return false;
	    }		
	  ::close(out_pipe[0]);
	  in = fdopen(in_pipe[0],"r");
	  if(in == NULL)
	    {
	      fprintf(stderr,"Unable to open pipe for reading: %s\n",strerror(errno));
	    }		
	  ::close(in_pipe[1]);      
	  eat();
	}
      init();
      return out != NULL;
    }

  void Gnuplot_Interface::close()
    {
      if(child)
	{
	  if(in) fclose(in);
	  if(out) fclose(out);
	  kill(child,SIGTERM);
	  wait(child);
	}
      else
	if(out)
	  pclose(out);
      in = NULL;
      out = NULL;
    }
#endif

  Gnuplot_Interface::~Gnuplot_Interface()
    {
      close();
    }

  void Gnuplot_Interface::cmd(const char *str, ...)
    {
      if(out == NULL)
	return;
      va_list va;
      va_start(va,str);
      vfprintf(out,str,va);
      va_end(va);
      if(DEBUG)
	vfprintf(stderr,str,va);
      fflush(out);
    }

  char *Gnuplot_Interface::resp(char *buf, size_t size)
    {
      if(in == NULL)
	return NULL;
#ifdef WIN32 // Stupid windows fgets doesn't work on files that are growing.
      // Returns "file not found" at random.
      // Fixme -- should probably be a timeout here.
      char *p = buf;
      size_t i;
      char *ret = buf;
      for(i = 0; i < size-i; i++)
	{
	  int c = fgetc(in);
	  if(c == EOF)
	    {
	      Sleep(1);
	      continue;
	    }
	  switch(c)
	    {
	    case '\r':
	      continue;
	    case '\n':
	      *p++=0;
	      goto done;
	    default:
	      *p++=c;
	    }
	}
    done:
#else
      char *ret = fgets(buf,size,in);
      if(ret != NULL)
	{
	  int i = strlen(buf);
	  if(isspace(buf[i-1]))
	    buf[i-1] = 0;
	}
#endif
      if(DEBUG)
	if(buf != NULL)
	  fprintf(stderr,"<%s>\n",buf);
      return ret;
    }

  /* To guarantee we're caught up, give gnuplot a unique number to spit
     back to us.  Wait for it. */
  void Gnuplot_Interface::eat()
    {
      static int unique=0;
      if(!in)
	return;
      char number[64];
      snprintf(number,sizeof(number),"SYNC_%d",unique++);
      cmd("print \"%s\"\n",number);
      char buf[1024];
      while(1)
	{
	  if(resp(buf,sizeof(buf)) == NULL)
	    return;
	  if(strcmp(buf,number) == 0)
	    return;
	}
    }

  void Gnuplot_Interface::prepmouse()
    {
      cmd("set mouse mouseformat \" \" labels\n");
      cmd("unset label\n");
    }

  /*  We need to somehow get the list of label out of gnuplot.
      Under unix, we can just run show labels and capture the output.
      Under windows, we need to stick our fingers down gnuplot's throat
      and get it to barf -- we do this by having it save the plot to a file,
      then parsing that file */
  void Gnuplot_Interface::getmouse(std::vector<point> &p)
    {
#ifdef WIN32 // Stupid windows -- show labels goes to the window, not to
      static int instance=0;
      FILE *itmp = in;
      // the defined output!
      char pname[1024];
      snprintf(pname,sizeof(pname),"%s.%d.%d.gp.gptalk",tmpdir(),getpid(),instance++);
      cmd("save '%s'\n",pname);
      eat(); // Make sure gnuplot is done saving.
      for(int i = 0; i < 100; i++)
	if((itmp = fopen(pname,"r")) != NULL) // Give gnuplot some time to open the window.
	  break;
	else
	  Sleep(10);
      if(itmp == NULL)
	warn("Unable to open gnuplot save file %s: %s\n",pname,strerror(errno));
      else
	{
	  regex coordinates("set label .* at ([^,]*),([^,]*),.*");
	  char buf[1024];
	  while(fgets(buf,sizeof(buf),itmp) != NULL)
	    {
	      if(DEBUG)
		fprintf(stderr,"[%s]\n",buf);
	      cmatch what;
	      if(regex_match(buf,what, coordinates))
		{
		  double x = atof(what[1].str().c_str());
		  double y = atof(what[2].str().c_str());
		  p.push_back(point(x,y));
		}
	    }
	  fclose(itmp);
	}
      unlink(pname);
      return;
#else
      eat();
      cmd("show label; print \"SYNC\"\n");
      char buf[1024];
      // Regular expression that takes a gnuplot label string, and pulls out x,y,z as subexpression 1,2,3
      regex coordinates("[^()]*at \\(([^,]*),([^,]*),([^,]*)\\).*"); 
      while(resp(buf,sizeof(buf)) != NULL)
	{
	  cmatch what;
	  if(regex_match(buf,what, coordinates))
	    {
	      double x = atof(what[1].str().c_str());
	      double y = atof(what[2].str().c_str());
	      p.push_back(point(x,y));
	    }
	  if(strcmp(buf,"SYNC") == 0)
	    return;
	}
      fprintf(stderr,"Error getting labels (%s)\n",strerror(errno));
#endif
    }

  double Gnuplot_Interface::getvariable(const char *name)
    {
      cmd("print %s\n",name);
      char buf[1024];
      if(resp(buf,sizeof(buf)) == NULL)
	return NAN;
      char *e;
      double v = strtod(buf,&e);
      if(e == buf || *e != 0)
	{
	  printf("Warning: can't parse %s as double\n",buf);
	  v = NAN;
	}
      return v;
    }

  const char *Gnuplot_Interface::tmpdir() // Return a directory for temporary files with a trailing / or \, as appropriate.
    {
      static const char *t = NULL;
      if(t != NULL)
	return t;
#ifdef WIN32
      if((t = getenv("TEMP")) != NULL)
	{
	  char *p = (char *)malloc(strlen(t)+2);
	  sprintf(p,"%s%c",t,DIRECTORY_SEPARATOR);
	  t = p;
	  info("TEMP=\"%s\"\n",t);
	  return t;
	}
      if((t = getenv("TMP")) != NULL)
	{
	  char *p = (char *)malloc(strlen(t)+2);
	  sprintf(p,"%s%c",t,DIRECTORY_SEPARATOR);
	  t = p;
	  info("TEMP=\"%s\"\n",t);
	  return t;
	}
      t="C:\\Windows\\Temp\\";
      info("Unable to get temp dir out of environment; using \"%s\"\n",t);
      return t;
#else
      const char *user = getenv("USER"); 
      char *p = (char *)malloc(strlen("/tmp/spyview-/") + strlen(user)+1);
      sprintf(p, "/tmp/spyview-%s/", user);
      t = p;
      if (mkdir(p, 0755) == -1)
	if (errno != EEXIST)
	  error("Fatal error: Failed to create temporary directory %s\n", p);
      return t;
#endif
    }

std::string Gnuplot_Interface::escape(std::string s)
{
  static regex bs("\\\\");
  return regex_replace(s,bs,"\\\\\\\\");  // Yay escaping!
}
