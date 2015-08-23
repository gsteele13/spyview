#include "message.h"
#include <FL/fl_ask.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef WIN32
void error(const char *str, ...)
{
  va_list va;
  va_start(va,str);
  char buf[4096];
  vsnprintf(buf,sizeof(buf),str,va);
  va_end(va);
  fl_alert(buf);
  while( Fl::first_window() )
    Fl::first_window()->hide();
  //exit(1);
}
void warn(const char *str, ...)
{
  va_list va;
  va_start(va,str);
  char buf[4096];
  vsnprintf(buf,sizeof(buf),str,va);
  va_end(va);
  fl_alert(buf);
}
void info(const char *str, ...)
{
  va_list va;
  va_start(va,str);
  vfprintf(stderr,str,va);
  va_end(va);
  fflush(stderr);
}
#else
void error(const char *str, ...)
{
  va_list va;
  va_start(va,str);
  vfprintf(stderr,str,va);
  va_end(va);
  while( Fl::first_window() )
    Fl::first_window()->hide();
  exit(1);
}
void warn(const char *str, ...)
{
  va_list va;
  va_start(va,str);
  vfprintf(stderr,str,va);
  va_end(va);
}
void info(const char *str, ...)
{
  va_list va;
  va_start(va,str);
  vfprintf(stderr,str,va);
  va_end(va);
}
#endif
