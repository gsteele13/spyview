#include "ThresholdDisplay.H"

ThresholdDisplay::ThresholdDisplay(ImageWindow *iwp) : ImageWindow_Module(iwp)
{
  tdc = new ThresholdDisplay_Control(this);
  r = 0; 
  g = 0; 
  b = 0;
}

int ThresholdDisplay::event_callback(int event)
{
  switch (event)
    {
    case FL_SHORTCUT:
      switch(Fl::event_key())
	{
	case 't':
	  if (tdc->win->shown())
	    tdc->win->hide();
	  else 
	    tdc->win->show();
	  return 1;
	}
    }
  return 0;
}

void ThresholdDisplay::overlay_callback()
{
  // This is actually increadibly slow!  It is faster just to redraw
  // the data, replacing the rejected pixels with black.  However,
  // that is a bit awkward to do with the current plugin interface...
 
  if (tdc->enable->value())
    {
      calculate();
      for (int j=0; j<iw->h; j++)
	for (int i=0; i<iw->w; i++)
	  if (iw->id.threshold_reject[j*iw->w+i])
	    iw->draw_overlay_pixel(i,j,r,g,b);
    }
  return;
}

void ThresholdDisplay::calculate()
{
  double low = tdc->low->value();
  double high = tdc->high->value();

  int type;

  if (tdc->val->value())
    type = 3;
  else if (tdc->image->value())
    type = 0;
  else if (tdc->line->value())
    type = 1;
  else //if (tdc->col->value());
    type = 2;
  
  iw->id.calculate_thresholds(type, low, high);
}
