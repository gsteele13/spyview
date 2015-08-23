#ifndef __throttle_h__
#define __throttle_h__


/* If you do a quick drag on the linecut, it's possible to pile up hundreds of replots in a fraction
   of a second.  The gnuplot window ends up lagging far behind, especially on the title and axis labels.
   This code avoids that problem.  It works as follows:
   
   1) If no line cut has been plotting in the last plotInterval seconds, it plots immediately.
   2) If there is already a line cut scheduled to be drawn, we just leave it scheduled (it will turn
      into the new line cut we want automagically).
   3) If there isn't one scheduled, but a line cut happened within plotInterval seconds ago, we
      schedule a line cut update plotInterval seconds in the future.
 
   The added complication of 1 means the first line cut gets out with no delay at all; this code
   only delays line cut updates if they're happening really fast.  

   plotInterval should be long enough the computer can keep up, but short enough to not cause a 
   noticable lag (something on the order of 10 or 20 updates per second is faster than you can
   perceive anyway.)

   This is now abstracted away in a class that can be used to add timeouts all over the place.  
   They really seem to make spyview more usable over remote connections.  Note that because
   we use the FLTK timing functions, there are no concurrency issues -- delayed draws *only*
   happen during calls to Fl::Wait.

   */


/* This class is complete templatized, member-functionized gobbledy-gook.  However, the usage is simple.
   At the front of a member function you'd like to throttle, which returns void and takes on argument, add

   static Throttle<ClassName> throttle(this,&ClassName::FunctionName,time);
   if(!throttle.throttle())
     return;

   replace ClassName with your class name, FunctionName with your function name, and time
   with the minimum amount of time you want between calls.
*/
template <class host> class Throttle
{
protected:
  // These are used to keep track of how long it's been since we updated the plot
  static void recent(void *) {}; // Fake timeout used to figure out if we've recently plotted.
  // This is used to schedule a plot in the future.
  static void scheduled(void *tp) 
  { 
    Throttle<host> *t = (Throttle<host> *)tp;
    t->run = true;
    ((t->h)->*(t->f))();
  };

public:
  host *h;
  void (host::*f)();
  double tout;
  bool run;

  Throttle(host *hp, 
	   void (host::*fp)(), 
	   double timeout=0.05) : h(hp), f(fp), tout(timeout), run(false) 
  {
  };
  
  bool throttle() // Return true if we should run.
  {
    if(!run) // Run means we should force a run.
      {
	if(Fl::has_timeout(recent,this))
	  {
	    if(!Fl::has_timeout(scheduled,this))
	      Fl::add_timeout(tout,scheduled,this);
	    return false;
	  }
	if(Fl::has_timeout(scheduled,this))
	  return false;
      }
    run = false;
    Fl::add_timeout(tout,recent,this);
    return true;
  };
};

// Variant of throttle that takes an argument specifying whether or not throttle should run.
// Member function should take a single bool, nothrottle
/* This class is complete templatized, member-functionized gobbledy-gook.  However, the usage is simple.
   At the front of a member function you'd like to throttle, which returns void and takes on argument, add

   void ClassName::FunctionName(bool nothrottle)
   {
     static OptThrottle<ClassName> throttle(this,&ClassName::FunctionName,time);
     if(!(nothrottle || throttle.throttle()))
       return;
     ...
   }
   replace ClassName with your class name, FunctionName with your function name, and time
   with the minimum amount of time you want between calls.
*/

template <class host> class OptThrottle
{
protected:
  // These are used to keep track of how long it's been since we updated the plot
  static void recent(void *) {}; // Fake timeout used to figure out if we've recently plotted.
  // This is used to schedule a plot in the future.
  static void scheduled(void *tp) 
  { 
    OptThrottle<host> *t = (OptThrottle<host> *)tp;
    t->run = true;
    ((t->h)->*(t->f))(false);
  };

public:
  host *h;
  void (host::*f)(bool);
  double tout;
  bool run;

  OptThrottle(host *hp, 
	   void (host::*fp)(bool), 
	   double timeout=0.05) : h(hp), f(fp), tout(timeout), run(false) 
  {
  };
  
  bool throttle() // Return true if we should run.
  {
    if(!run) // Run means we should force a run.
      {
	if(Fl::has_timeout(recent,this))
	  {
	    if(!Fl::has_timeout(scheduled,this))
	      Fl::add_timeout(tout,scheduled,this);
	    return false;
	  }
	if(Fl::has_timeout(scheduled,this))
	  return false;
      }
    run = false;
    Fl::add_timeout(tout,recent,this);
    return true;
  };
};

#endif
