#ifndef __IMAGEWINDOW_MODULE__
#define __IMAGEWINDOW_MODULE__
/* Derive your own modules from this class by overloading one or more of the callbacks.
   This provides a way to add funcitonality to spyview without unduly complicating the
   main code base, with fairly minimal overhead. 

   Make sure you declare a virtual destructor!
*/
 
class ImageWindow;
class ImageWindow_Module
{
public:
  ImageWindow *iw;
  ImageWindow_Module(ImageWindow *iwp);
  virtual ~ImageWindow_Module() {};

  /* This callback is called whenever the linecut is updated.  It's called twice -- once
     before the plot command, with init=true, then a second during the plot command, when the
     handler can output a series of lines like sin(x), cos(x), "foo.dat" u 3:4 ti "foo", etc... */
  virtual void linecut_callback(bool init) {};

  /* This callback is called whenever an event is generated.  It should return 1 if it ate the event. */
  virtual int event_callback(int ev) { return 0; };

  /* This callback is called whenever the overlay is redrawn. */
  virtual void overlay_callback() {};
};

#endif

