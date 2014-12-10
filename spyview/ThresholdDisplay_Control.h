// generated by Fast Light User Interface Designer (fluid) version 1.0107

#ifndef ThresholdDisplay_Control_h
#define ThresholdDisplay_Control_h
#include <FL/Fl.H>
#include <FL/Fl_Color_Chooser.H>
class ThresholdDisplay;
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Light_Button.H>

class ThresholdDisplay_Control {
public:
  ThresholdDisplay *td;
  ThresholdDisplay_Control(ThresholdDisplay *tdp);
  Fl_Double_Window *win;
private:
  void cb_dismiss_i(Fl_Button*, void*);
  static void cb_dismiss(Fl_Button*, void*);
public:
  Fl_Value_Input *low;
private:
  void cb_low_i(Fl_Value_Input*, void*);
  static void cb_low(Fl_Value_Input*, void*);
public:
  Fl_Value_Input *high;
private:
  void cb_high_i(Fl_Value_Input*, void*);
  static void cb_high(Fl_Value_Input*, void*);
public:
  Fl_Round_Button *image;
private:
  void cb_image_i(Fl_Round_Button*, void*);
  static void cb_image(Fl_Round_Button*, void*);
public:
  Fl_Round_Button *line;
private:
  void cb_line_i(Fl_Round_Button*, void*);
  static void cb_line(Fl_Round_Button*, void*);
public:
  Fl_Round_Button *col;
private:
  void cb_col_i(Fl_Round_Button*, void*);
  static void cb_col(Fl_Round_Button*, void*);
public:
  Fl_Check_Button *val;
private:
  void cb_val_i(Fl_Check_Button*, void*);
  static void cb_val(Fl_Check_Button*, void*);
public:
  Fl_Check_Button *percent;
private:
  void cb_percent_i(Fl_Check_Button*, void*);
  static void cb_percent(Fl_Check_Button*, void*);
public:
  Fl_Light_Button *enable;
private:
  void cb_enable_i(Fl_Light_Button*, void*);
  static void cb_enable(Fl_Light_Button*, void*);
  void cb_color_i(Fl_Button*, void*);
  static void cb_color(Fl_Button*, void*);
public:
  void refresh();
};
#endif
