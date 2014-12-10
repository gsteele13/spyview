#ifndef __FIDDLE_H__
#define __FIDDLE_H__

#include "FL/Fl.H"

class ImageWindow;

#include "ImageWindow_Module.H"
#include "ImageWindow.H"

class Fiddle : public ImageWindow_Module
{
public:
  Fiddle(ImageWindow *iwp);
  ~Fiddle() {};

  bool enabled;
  int x1,y1,x2,y2;

  int min0;
  int max0;

  double step_size;

  int event_callback(int event);
};

#endif
