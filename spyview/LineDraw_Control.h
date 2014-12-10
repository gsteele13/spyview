// generated by Fast Light User Interface Designer (fluid) version 1.0110

#ifndef LineDraw_Control_h
#define LineDraw_Control_h
#include <FL/Fl.H>
class ImageWindow;
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_File_Input.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Box.H>

class LineDraw_Control {
public:
  LineDraw_Control();
  Fl_Double_Window *win;
private:
  void cb_dismiss_i(Fl_Button*, void*);
  static void cb_dismiss(Fl_Button*, void*);
  void cb_Reload_i(Fl_Button*, void*);
  static void cb_Reload(Fl_Button*, void*);
public:
  Fl_File_Input *file;
  Fl_Light_Button *autodelete;
  Fl_Light_Button *autognu;
private:
  void cb_autognu_i(Fl_Light_Button*, void*);
  static void cb_autognu(Fl_Light_Button*, void*);
public:
  Fl_Check_Button *endpoints;
private:
  void cb_endpoints_i(Fl_Check_Button*, void*);
  static void cb_endpoints(Fl_Check_Button*, void*);
public:
  Fl_Light_Button *highlight_nl;
private:
  void cb_highlight_nl_i(Fl_Light_Button*, void*);
  static void cb_highlight_nl(Fl_Light_Button*, void*);
  void cb_Info_i(Fl_Button*, void*);
  static void cb_Info(Fl_Button*, void*);
  void cb_Delete_i(Fl_Button*, void*);
  static void cb_Delete(Fl_Button*, void*);
  void cb_Load_i(Fl_Button*, void*);
  static void cb_Load(Fl_Button*, void*);
  void cb_Save_i(Fl_Button*, void*);
  static void cb_Save(Fl_Button*, void*);
  void cb_Load1_i(Fl_Button*, void*);
  static void cb_Load1(Fl_Button*, void*);
  void cb_Save1_i(Fl_Button*, void*);
  static void cb_Save1(Fl_Button*, void*);
public:
  Fl_Light_Button *plot_nl;
private:
  void cb_plot_nl_i(Fl_Light_Button*, void*);
  static void cb_plot_nl(Fl_Light_Button*, void*);
public:
  Fl_Double_Window *nl_info_win;
  Fl_Round_Button *nl_pixels;
  Fl_Input *nl_fmt;
  Fl_Output *nl_x1;
  Fl_Output *nl_x2;
  Fl_Output *nl_y1;
  Fl_Output *nl_y2;
  Fl_Output *nl_slope;
  Fl_Output *nl_slope_unit;
  Fl_Output *nl_xunit;
  Fl_Output *nl_yunit;
  Fl_Browser *line_display;
private:
  void cb_line_display_i(Fl_Browser*, void*);
  static void cb_line_display(Fl_Browser*, void*);
  void cb_dismiss1_i(Fl_Button*, void*);
  static void cb_dismiss1(Fl_Button*, void*);
};
#endif
