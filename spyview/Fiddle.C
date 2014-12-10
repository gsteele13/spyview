#include "Fiddle.H"

Fiddle::Fiddle(ImageWindow *iwp) : ImageWindow_Module(iwp)
{
  enabled = false;
}

int Fiddle::event_callback(int event)
{
  int button;
  if (Fl::event_state() & FL_BUTTON1)
    button = 1;
  else if (Fl::event_state() & FL_BUTTON2)
    button = 2;
  else if (Fl::event_state() & FL_BUTTON3)
    button = 3;

  switch (event)
    {
    case FL_SHORTCUT:
      switch(Fl::event_key())
	{
	case 'f':
	  if (Fl::event_state() & FL_CTRL)
	    {
	      enabled = !enabled;
	      info("fiddle mode %s\n", enabled ? "enabled" : "disabled");
	      return 1;
	    }
	  else
	    return 0;
	  break;
	default:
	  return 0;
	}
      break;
    case FL_PUSH:
      if (enabled && button != 3)
	{
	  x1 = Fl::event_x();
	  y1 = Fl::event_y();
	  min0 = iw->hmin;
	  max0 = iw->hmax;
	  step_size = 1.0*(iw->hmax - iw->hmin)/250;
	  //info("fiddle step size %.2f per pixel\n", step_size);
	  return 1;
	}
      else
	return 0;
      break;
    case FL_DRAG:
      if (enabled && button != 3)      
	{
	  x2 = Fl::event_x(); 
	  y2 = Fl::event_y();
	  
	  double d_center = (x1-x2)*step_size;
	  double d_width = -(y1-y2)*step_size;
	  
	  iw->hmin = min0 - d_center - d_width/2;
	  iw->hmax = max0 - d_center + d_width/2;
	  
	  if (iw->hmin < 0) iw->hmin = 0;
	  if (iw->hmax > LMAX) iw->hmax = LMAX;
	  if (iw->hmin > iw->hmax) iw->hmin = iw->hmax;
	  iw->damage(FL_DAMAGE_ALL);
	  iw->external_update();
	  return 1;
	}
      else
	return 0;
      break;
    case FL_RELEASE:
      if (enabled && button != 3)
	return 1;
      else
	return 0;
      break;
    default:
      return 0;
    }
}
