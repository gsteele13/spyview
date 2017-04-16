#include <stdio.h>
#include <FL/Fl.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Window.H>
#include "Fl_Table.H"
#include <stdlib.h>
#include <string.h>
#include <FL/fl_draw.H>
#include <algorithm>
#include <assert.h>
#include "spypal_gradient.H"
#include "spypal_interface.H"
#include <fstream>
#include "message.h"

/*********
 * Spypal_Slider_Dragger
 *********/
void Spypal_Slider_Dragger::start_dragging(Fl_Spypal_GSlider *todrag, bool can_delete_p)
{
  assert(todrag);
  assert(dragging == NULL);
  can_delete = can_delete_p;
  dragging = todrag;
  colored = NULL;
  shown = find(g->sliders.begin(),g->sliders.end(), dragging) != g->sliders.end();
  if(!shown && (Fl::event_inside(g) || !can_delete))
    show_dragged();
  initial_location = todrag->loc;
  g->add(dragging);
  if(can_delete)
    g->spw->set_tooltip("Drag onto gradient bar to add/rearrange waypoints, or away to delete.  Drop on a waypoint to change its color.");
  else
    g->spw->set_tooltip("Drag onto gradient bar to rearrange waypoints.");
}

bool Spypal_Slider_Dragger::handle_mouse(int ev)
{
  if(!(ev == FL_PUSH || ev == FL_DRAG || ev == FL_RELEASE))
    return false;
  if(dragging == NULL) // We're not dragging anyway!
    return false;
  if(ev == FL_PUSH) // Somehow we missed a release. Weird.
    {
      fprintf(stderr,"Argh! Missed a mouse release!\n");
      dragging = NULL; // Might as well give up and hope.
      return false;
    }

  bool dirty = false;           // Do we need to recalculate?

  // Figure out what slider we're over.
  Fl_Spypal_GSlider *on = NULL; // What slider are we on?
  for(Fl_Spypal_Gradient::sliders_t::reverse_iterator i = g->sliders.rbegin(); i != g->sliders.rend(); i++)
    {
      if(*i == dragging) // We're trivially always over the slider we're dragging...
	continue;
      if((*i)->hit())
	{	  
	  on = *i;
	  break;
	}
    }

  if(on != NULL && can_delete) // We're over a slider; color it.
    {
      if(colored != on) // We may have already done the work.
	{
	  color_slider(on);
	  hide_dragged();
	  dirty = true;
	}
    }
  else // We're not over a slider;
    {
      if(colored != NULL)
	{
	  uncolor_slider();
	  dirty = true;
	}
      if(can_delete && !Fl::event_inside(g))
	{
	  if(shown)
	    {
	      dirty = true;
	      hide_dragged();
	    }
	}
      else // We're undeletable or inside the gradient area.
	{
	  if(!shown)
	    {
	      dirty = true;
	      show_dragged();
	    }
	  dirty |= dragging->reloc(Fl::event_x());
	  dragging->recalc(Fl::event_y());
	  g->redraw();
	}
    }

  if(dirty || ev == FL_RELEASE)
    {
      g->recalculate(ev == FL_RELEASE);
      g->redraw();
    }

  if(ev == FL_RELEASE)
    {
      if(can_delete && !shown)
	{
	  g->remove(dragging);
	  if(g->spw->cc->user_data() == (Spypal_Color_Selectable *)dragging)
	    (*g->sliders.begin())->color_select(g->spw->cc);
	  delete dragging;
	}
      else if(shown)
	dragging->color_select(g->spw->cc);
      dragging = NULL;
      g->spw->reset_tooltip();
    }
  return true;
}

// Copy the "dragging" color to another slider.
void Spypal_Slider_Dragger::color_slider(Fl_Spypal_GSlider *tocolor)
{
  if(colored != NULL)
    uncolor_slider();
  colored = tocolor;
  oldcolor = tocolor->c;
  tocolor->c = dragging->c;
}

void Spypal_Slider_Dragger::uncolor_slider()
{
  if(colored == NULL)
    return;
  colored->c = oldcolor;
  colored = NULL;
}

void Spypal_Slider_Dragger::hide_dragged()
{
  if(!shown)
    return;
  shown = false;
  Fl_Spypal_Gradient::sliders_t::iterator f = find(g->sliders.begin(),g->sliders.end(),dragging);
  assert(f != g->sliders.end());
  g->sliders.erase(f);
  dragging->invisible = true;
}

void Spypal_Slider_Dragger::show_dragged()
{
  if(shown)
    return;
  shown = true;
  dragging->show();
  g->sliders.push_back(dragging);
  dragging->invisible = false;
}

/**************
 * Fl_Spypal_Swatch
 **************/
Fl_Spypal_Swatch::Fl_Spypal_Swatch(int x, int y, int w, int h, const char *p) : Fl_Widget(x,y,w,h,p)
{
  cc_sRGB.set(c,0,0,0);
  g = NULL;
}

void Fl_Spypal_Swatch::setGradient(Fl_Spypal_Gradient *gp)
{
  g = gp;
  dragger.g = gp;
}

int Fl_Spypal_Swatch::handle(int event)
{
  switch(event)
    {
    case FL_PUSH:
      {
	color_select(g->spw->cc);  // We're now the editable color thingy.
	Fl_Spypal_GSlider *slider = new Fl_Spypal_GSlider(g);
	slider->c = c;
	dragger.start_dragging(slider,true);
	return true;
      }
    }
  if(dragger.handle_mouse(event))
    return true;
  return Fl_Widget::handle(event);
}

void Fl_Spypal_Swatch::draw()
{
  draw_box(highlighted ? fl_down(box()) : box(), !highlighted ? color() : FL_YELLOW);
  double R,G,B;
  cc_sRGB.get(c,R,G,B);
  fl_color(round(R*255),round(G*255),round(B*255));
  fl_rectf(x()+Fl::box_dx(box()),y()+Fl::box_dy(box()),
	   w()-Fl::box_dw(box()),h()-Fl::box_dh(box()));
  draw_label();

}

/**************
 * Fl_Spypal_GSlider
 **************/
Fl_Spypal_GSlider::Fl_Spypal_GSlider(Fl_Spypal_Gradient *gp) : Fl_Widget(0,0,1,1,""),  g(gp)
{
  box(FL_UP_BOX);
  cc_sRGB.set(c,0,0,0);
  loc = 0.0;
  stack = 0;
  last_x = -1;
  locked = false;
  dragger.g = g;
  invisible = false;
  editor = NULL;
  if(gp)
    recalc();
}

Fl_Spypal_GSlider::~Fl_Spypal_GSlider()
{
  if(editor)
    {
      editor->win->hide();
      delete editor->win;
      delete editor;
      editor = NULL;
    }
}

int Fl_Spypal_GSlider::handle(int event)
{
  switch(event)
    {
    case FL_PUSH:
      {
	color_select(g->spw->cc);
	if(Fl::event_button() == 3)
	  {
	    locked = !locked;
	    g->spw->recalculate(true);
	    return false;
	  }
	if(Fl::event_button() == 2)
	  {
	    if(!editor)
	      {
		editor = new spypal_slider_editor();
		editor->slider = this;
		update_editor();		
	      }
	    editor->win->show();
	    return false;
	  }
	else
	  {
	    dragger.start_dragging(this,g->sliders.size() > 2);
	    return true;
	  }
      }
    }
  if(dragger.handle_mouse(event))
    return true;
  return Fl_Widget::handle(event);
}

void Fl_Spypal_GSlider::update_editor()
{
  if(!editor)
    return;
  char buf[1024];
  snprintf(buf,sizeof(buf),"%g",loc);
  editor->position->value(buf);
  editor->locked->value(locked);
}

bool Fl_Spypal_GSlider::hit()
{
  return Fl::event_inside(this);
}

void Fl_Spypal_GSlider::color_changed()
{
  g->recalculate(true);
  g->redraw();
  update_editor();
}

bool Fl_Spypal_GSlider::reloc(int x)
{
  double x1 = g->grad->x()+Fl::box_dx(g->grad->box());
  double x2 = g->grad->x()+g->grad->w()-Fl::box_dw(g->grad->box());
  loc = (x-x1)/(x2-x1);
  bool moved = (x != last_x);
  last_x = x;
  return moved;
}

void Fl_Spypal_GSlider::recalc(int y)
{
  double x1 = g->grad->x()+Fl::box_dx(g->grad->box());
  double x2 = g->grad->x()+g->grad->w()-Fl::box_dw(g->grad->box());
  xg = x1+(x2-x1)*loc;
  double xt = xg - slider_w/2;
  if(xt < g->x())
    xt = g->x();
  if(xt + slider_w> g->x()+g->w())
    xt = g->x()+g->w()-slider_w;

  double yt = g->grad->y() - (stack+1.25)*slider_h; 
  resize(xt,y > 0 ? y : yt,slider_w,slider_h);
  update_editor();
}

void Fl_Spypal_GSlider::restack(int stackp)
{
  stack = stackp;
  double yt = g->grad->y() - (stack+1.25)*slider_h; 
  resize(x(), yt,w(),h());
}

void Fl_Spypal_GSlider::draw()
{
  if(invisible)
    return;

  // Draw the triangle beneath
  fl_color(highlighted ? FL_DARK1 : FL_DARK3);
  fl_polygon(x()+Fl::box_dx(box()),y()+h(),xg,g->grad->y(),x()+w()-Fl::box_dx(box()),y()+h());
  fl_color(FL_WHITE);
  fl_line(x()+Fl::box_dx(box()),y()+h(),xg,g->grad->y(),x()+w()-Fl::box_dx(box()),y()+h());

  draw_box(highlighted ? fl_down(box()) : box(), !highlighted ? color() : FL_YELLOW);
  double R,G,B;
  cc_sRGB.get(c,R,G,B);  
  fl_color(round(R*255),round(G*255),round(B*255));
  fl_rectf(x()+Fl::box_dx(box()),y()+Fl::box_dy(box()),
	   w()-Fl::box_dw(box()),h()-Fl::box_dh(box()));

  fl_color(FL_BLACK);
  if(locked)
    fl_draw("L",x()+5,y()+h()/2.0);
}

/*************
 * Fl_Spypal_Gradient
 *************/
Fl_Spypal_Gradient::Fl_Spypal_Gradient(int x, int y, int w, int h, 
				       const char *t) :
  Fl_Group(x,y,w,h,t)
{
  dragger.g = this;
  icache = NULL;
  img = NULL;

  begin();
  grad = new Fl_Box(x+padding,y+h-(grad_h+gamut_h+Fl::box_dh(grad_btype)),
		    w-padding*2,grad_h+gamut_h+Fl::box_dh(grad_btype),0);
  grad->box(grad_btype);
  grad->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
  // Set up an initial black->red gradient.
  Fl_Spypal_GSlider *c = new Fl_Spypal_GSlider(this); 
  cc_sRGB.set(c->c,0,0,0);
  c->recalc();
  sliders.push_back(c);
  c = new Fl_Spypal_GSlider(this); 
  cc_sRGB.set(c->c,1,0,0);
  c->loc=1.0;
  c->recalc();
  sliders.push_back(c);
  end();

  realloc_icache();
};

int Fl_Spypal_Gradient::handle(int ev)
{
  switch(ev)
    {
    case FL_PUSH:
      if(!Fl::event_inside(grad))
	break;
      else
	{
	  Fl_Spypal_GSlider *slider = new Fl_Spypal_GSlider(this);
	  double loc = (Fl::event_x()-(grad->x()+Fl::box_dx(grad->box())));
	  loc /= grad->w()-Fl::box_dw(grad->box());
	  assert(loc >= 0);
	  assert(loc <= 1.0);
	  slider->c = cmap[(cmap.size()-1)*loc];
	  slider->color_select(spw->cc);
	  slider->locked = true;
	  dragger.start_dragging(slider,true);
	  dragger.handle_mouse(FL_DRAG);
	  return true;
	}
    }
  if(dragger.handle_mouse(ev))
    return true;
  return Fl_Group::handle(ev);
}

void Fl_Spypal_Gradient::clear_sliders()
{
  while(sliders.size())
    {
      remove(sliders.front());
      delete sliders.front();
	    sliders.pop_front();
    }
}

void Fl_Spypal_Gradient::set(const SpypalWaypoints_t &wps)
{
  clear_sliders();
  begin();
  for(unsigned i = 0; i < wps.size(); i++)
    {
      sliders.push_front(new Fl_Spypal_GSlider(this));
      sliders.front()->loc = wps[i].loc;
      sliders.front()->c = wps[i].c;
      sliders.front()->locked = wps[i].locked;
    }  
  end();
}

void Fl_Spypal_Gradient::resize(int x, int y, int w, int h)
{
  Fl_Group::resize(x,y,w,h);
  realloc_icache();
  restack_sliders();
  update_image();
  spw->win->redraw();
}

void Fl_Spypal_Gradient::draw()
{
  fl_color(FL_BACKGROUND_COLOR);
  fl_rectf(x(),y(),w(),h());
  Fl_Group::draw();
}

void Fl_Spypal_Gradient::update_image()
{
  static Fl_RGB_Image *img = NULL;
  if(img != NULL)
    delete img;

  unsigned char *p = icache;
  bool clip;
  for(int i = 0; i < iwid; i++)
    {
      int x = i*cmap.size()/iwid;
      clip = spypal_clipped[x];
      p[0] = spypal_colormap[x*3+0];
      p[1] = spypal_colormap[x*3+1];
      p[2] = spypal_colormap[x*3+2];
      if(clip)
	{
	  p[iwid*(ihigh-1)*3+0]=255;
	  p[iwid*(ihigh-1)*3+1]=0;
	  p[iwid*(ihigh-1)*3+2]=0;
	}
      else
	{
	  p[iwid*(ihigh-1)*3+0]=0;
	  p[iwid*(ihigh-1)*3+1]=0;
	  p[iwid*(ihigh-1)*3+2]=0;
	}
      p += 3;

    }
  for(int i = 1; i < ihigh-gamut_h; i++)
    memcpy(icache+iwid*3*i,icache,iwid*3);
  for(int i = ihigh-gamut_h; i < ihigh-1; i++)
    memcpy(icache+iwid*3*i,icache+iwid*3*(ihigh-1),iwid*3);
  img = new Fl_RGB_Image(icache, iwid, ihigh, 3);
  grad->image(img);
  redraw();
}

void Fl_Spypal_Gradient::realloc_icache()
{
  iwid  = grad->w()-Fl::box_dw(grad->box());
  ihigh = grad->h()-Fl::box_dh(grad->box());
  printf("%d %d %d %d\n",iwid,grad->w(),ihigh, grad->h());
  if(icache != NULL)
    free(icache);
  icache = (unsigned char *)malloc(iwid*ihigh*3);
}

void Fl_Spypal_Gradient::recalculate(bool b)
{
  spw->recalculate(b);
}

// Move all sliders to wps positions
// Restack them for UI purposes
void Fl_Spypal_Gradient::reposition_sliders(SpypalWaypoints_t &wps)
{
  unsigned j = 0;

  for(sliders_t::iterator i = sliders.begin(); i != sliders.end(); i++)
      (*i)->loc = wps[j++].loc;
  restack_sliders();
}

void Fl_Spypal_Gradient::restack_sliders()
{
  for(sliders_t::iterator i = sliders.begin(); i != sliders.end(); i++)
    (*i)->recalc();
  std::vector<int> lastused;
  for(unsigned i = 0; i < sliders.size(); i++)
    lastused.push_back(-10000);
  unsigned maxstack = 0;
  for(sliders_t::iterator i = sliders.begin(); i != sliders.end(); i++)
    {
      for(unsigned j = 0; j < sliders.size(); j++)
	{
	  if(lastused[j] < (*i)->x()-(*i)->w())
	    {
	      (*i)->restack(j);
	      if(j > maxstack)
		maxstack = j;
	      lastused[j] = (*i)->x();
	      break;
	    }
	}	      
    }
  // Widgets should be in reverse of their stacking order to get the render right.  This is horribly inefficent.
  for(unsigned j = 1; j <= maxstack; j++)
    {
      for(sliders_t::iterator i = sliders.begin(); i != sliders.end(); i++)
	if((*i)->stack == j)
	  insert(**i,0);
    }
}
