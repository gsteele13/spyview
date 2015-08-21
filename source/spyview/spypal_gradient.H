#ifndef __spypal_gradient_h__
#define __spypal_gradient_h__
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Output.H>
#include <FLTK_Serialization.H>
#include <list>
#include <functional>
#include <algorithm>
#include <assert.h>
#include "myboost.h"
#include "cclass.H"
#include "spypal.h"
// These functions define a nice "gradient editor."

class Fl_Spypal_Gradient;
class Fl_Spypal_GSlider;


// This class allows objects of several types to nicely share a color chooser.
// It sets the highlighted flag in one-and-only-one object.
// Derived classes should use color_select to give focus to color chooser,
// Color_unselect to lose focus,
class Spypal_Color_Selectable 
{
public:
  ccolor c;
  Fl_Color_Chooser *_scs_cc; // Cache the current color chooser for "unselect"
  bool highlighted;

  virtual void color_changed() = 0;

  Spypal_Color_Selectable()
  {
    _scs_cc = NULL;
    highlighted=false;
  }

  void color_select(Fl_Color_Chooser *cc)
  {
    if(highlighted) // We can only be in one chooser at a time.
      color_unselect();
    highlighted = true;
    if(cc->user_data() != NULL && cc->callback() == &Spypal_Color_Selectable::_cc_callback)
      {
	Spypal_Color_Selectable *p = (Spypal_Color_Selectable *)(cc->user_data());
	p->color_unselect();
      }
    cc->callback(&Spypal_Color_Selectable::_cc_callback, this);
    double r,g,b;
    cc_sRGB.get(c,r,g,b);
    cc->rgb(r,g,b);
    color_changed();
    _scs_cc = cc;
  }

  void color_unselect()
  {
    if(highlighted)
      {
	highlighted=false;
	assert(_scs_cc);
	_scs_cc->callback(Fl_Widget::default_callback,NULL);
	color_changed();
      }
  }

  static void _cc_callback(Fl_Widget *w, void *p) 
  { 
    Fl_Color_Chooser *cc = dynamic_cast<Fl_Color_Chooser*>(w);
    assert(cc);
    ((Spypal_Color_Selectable *)p)->cc_callback(cc); 
  };

  // Override one or both of these.  If you override the version with no arguments, the color
  // has already been snarfed from the chooser.
  virtual void cc_callback(Fl_Color_Chooser *cc)
  {    
    assert(cc == _scs_cc);
    cc_sRGB.set(c,cc->r(),cc->g(),cc->b());
    cc_callback();
  }  
  virtual void cc_callback() 
  {
    color_changed();
  };

  virtual ~Spypal_Color_Selectable()
  {
    if(highlighted) // If we're highlighted, better unselect ourselves to avoid seg-fault.
      {
	assert(_scs_cc);
	_scs_cc->callback(Fl_Widget::default_callback,NULL);
      }
  }
};

/* This class encapsulates the logic for dragging around spypal sliders */
class Spypal_Slider_Dragger
{
public:
  Fl_Spypal_Gradient *g;
  Fl_Spypal_GSlider *dragging; // What slider are we dragging?

  double initial_location;     // Where was this slider initially (in case the user tries to delete)
  bool can_delete;             // Are we allowed to delete the slider we're dragging?
  bool shown;                  // Is the slider we're dragging shown?
  Spypal_Slider_Dragger(Fl_Spypal_Gradient *gp = NULL)  : g(gp)
  {
    dragging = NULL;
  }

  void start_dragging(Fl_Spypal_GSlider *todrag, bool can_delete_p);
  bool handle_mouse(int ev); // Return true if the event was eaten.
protected:
  // For behaviour; if the dragged slider is dropped on a current slider, "color" it in.
  Fl_Spypal_GSlider *colored;  // What slider have we currently "colored in"?
  ccolor oldcolor;             // What color was it?

  void color_slider(Fl_Spypal_GSlider *tocolor);
  void uncolor_slider();

  // Manipulate the "dragged" slider.
  void show_dragged(); 
  void hide_dragged();
};

// A swatch is a colored box that can a) be edited, b) dragged onto a slider
//  c) dragged onto gradient bar.
class Fl_Spypal_Swatch : public Spypal_Color_Selectable, public Fl_Widget
{
public:  
  // Drag information
  Spypal_Slider_Dragger dragger;
  
  Fl_Spypal_Swatch(int x, int y, int w, int h, const char *p);
  void setGradient(Fl_Spypal_Gradient *gp); // Must call before using.
  
  void draw();
  void color_changed() { redraw(); };
  int handle(int ev);

  Fl_Spypal_Gradient *g;
};

class spypal_slider_editor;

// A slider along a gradient bar.
class Fl_Spypal_GSlider : public Fl_Widget, public Spypal_Color_Selectable
{
public:
  // Drag information
  Spypal_Slider_Dragger dragger;

  // Editor information
  spypal_slider_editor *editor;
  void update_editor();  // Change the values in the editor.
  
  Fl_Spypal_GSlider(Fl_Spypal_Gradient *g=0);
  virtual ~Fl_Spypal_GSlider();

  int handle(int event); 
  void draw();
  bool reloc(int x); // Drag the marker to pixel x.  Return true if this is a move.
  int last_x; // Last x coordinate for drag.
  void recalc(int y=-1); // Recompute the slider position.  Use stack if y is default.
  void restack(int s); // Change the stack of this slider.
  bool hit(); // Return true if the event is a "hit"

  double loc;
  double xg;        // Location of this on the gradient slider, in pixels.
  unsigned stack;   // How far up to draw this.  Used to pop up sliders if they'd otherwise overlap.
  bool invisible;   // If true, skip drawing. Necessary to have invisible widgets get events.
  bool locked;
  Fl_Spypal_Gradient *g;

  static const int slider_h = 30; // Size of slider
  static const int slider_w = 30; 
  static const int slider_c = 8; // Close button size

  // Needed for list sort
  //  struct cmp { bool operator()(Fl_Spypal_GSlider *c1, Fl_Spypal_GSlider *c2) 
  static bool cmp(Fl_Spypal_GSlider *c1, Fl_Spypal_GSlider *c2) 
    { return c1->loc < c2->loc; };

  virtual void color_changed();

  template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
  {
    ar & locked;
    ar & loc;
    ar & c;
  };

};

class spypal_wizard;
// A gradient bar.
class Fl_Spypal_Gradient : public Fl_Group
{
public:
  typedef std::list<Fl_Spypal_GSlider *> sliders_t;
  sliders_t sliders;

  Fl_Spypal_Gradient(int x, int y, int w, int h, const char *t="");
  
  virtual void draw();
  virtual void resize(int x, int y, int w, int h);
  void draw_slider(double loc, ccolor c, int yoff = 0);
  void reposition_sliders(SpypalWaypoints_t &wpts);
  void restack_sliders();
  int handle(int ev);

  // Drag information
  Spypal_Slider_Dragger dragger;

  // Create and move sliders to wps positions.
  void set(const SpypalWaypoints_t &wps);
  void clear_sliders(); // Remove all sliders

  template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
  {
    begin();
    if(!Archive::is_saving::value)
      clear_sliders();

    ar & sliders;
    if(!Archive::is_saving::value)
      {
	for(sliders_t::iterator i = sliders.begin(); i != sliders.end(); i++)
	  {
	    (*i)->g = this;
	    (*i)->dragger.g = this;
	  }
      }
    end();
  }

  spypal_wizard *spw;

  void recalculate(bool repso);

protected:
  // Geometry
  static const int grad_h=30;  // Height of gradient bar
  static const int gamut_h=0;  // No longer needed -- we always force gamut.
  static const int padding=30; // To make it easy to put sliders at the ends. 

  static const Fl_Boxtype grad_btype = FL_DOWN_BOX;
  Fl_Box *grad;
  Fl_RGB_Image *img;

  // Image cache for gradient bar  
  void update_image(); // Spypal pallete must already be generated.
  void realloc_icache();
  unsigned char *icache;
  int iwid, ihigh;

  friend class Fl_Spypal_GSlider;
  friend class spypal_wizard;
};

#endif
