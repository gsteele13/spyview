#ifndef testImageWindow_h
#define testImageWindow_h
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Widget.H>
#include <string.h>
#include <string>
#include <vector>
/*void minl_cb(Fl_Widget *, void *);
void maxl_cb(Fl_Widget *, void *);
void dminl_cb(Fl_Widget *, void *);
void dmaxl_cb(Fl_Widget *, void *);*/


void filech_cb(Fl_Widget *, void *);
void saveb_cb(Fl_Widget *, void *);
void cmapch_cb(Fl_Widget *o, void*);
void update_gui();
#endif
