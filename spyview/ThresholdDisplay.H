#ifndef __THRESHOLD_DISPLAY_H__
#define __THRESHOLD_DISPLAY_H__

class ImageWindow;

#include "ImageWindow_Module.H"
#include "ThresholdDisplay_Control.h"
#include "ImageWindow.H"

class ThresholdDisplay : public ImageWindow_Module
{
public:
  ThresholdDisplay(ImageWindow *iwp);
  ~ThresholdDisplay() {};

  ThresholdDisplay_Control *tdc;

  uchar r,g,b;

  void reallocate();
  void clear();

  int event_callback(int event);
  void overlay_callback();
  void calculate();
};

#endif
