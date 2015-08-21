#ifndef FL_LISTBOX_H
#define FL_LISTBOX_H
#include <FL/fl_draw.H>
#include "Fl_Table.H"
#include <string>
#include <vector>

using namespace std;

class Fl_Listbox : public Fl_Table
{
  enum ListboxType 
    {
      LISTBOX_HORIZONTAL = 0, 
      LISTBOX_VERTICAL = 1
    };

private:
  vector <int> my_selected;
  vector <string> my_label;
  int my_row_height;
  int my_col_width;
  int my_text_height;
  int my_type;
  
  int last_selected; // used for the shift-select

protected: 
  void draw_cell(TableContext contect, int=0, int=0, int=0, int=0, int=0, int=0);
  static void event_callback_static(Fl_Widget*, void*);
  void event_callback();  
  int handle(int event);
  void recalculate_rows();

public:
  Fl_Listbox(int x, int y, int w, int h, const char *l=0);
  ~Fl_Listbox() {};
  void add(const char *);
  void resize(int x, int y, int w, int h);
  void item_height(int h) {my_row_height = h; recalculate_rows();};
  int item_height() {return my_row_height;};
  void item_width(int w) {my_col_width = w; recalculate_rows();};
  int item_width() { return my_col_width; };
  void text_height(int h) { my_text_height = h; redraw();};
  int text_height() { return my_text_height; };
  int item_selected(int i) { return my_selected[i-1]; };
  int selected(int i) { return my_selected[i-1]; };
  void item_select(int i, int s) { my_selected[i-1] = s == 0 ? 0 : 1; }; \
  void remove(int i);
  const char *text(int i) { return my_label[i-1].c_str(); };
  void text(int i, const char *t) { my_label[i-1] = t; redraw();};
  int size() { return my_label.size(); };
  void clear() { my_selected.clear(); my_label.clear(); last_selected = -1; my_col_width = 1; recalculate_rows(); redraw();};
  int type() {return my_type;};
  void type(int t) {my_type = (t>0) ? 1 : 0; redraw();};
  const char* last_selected_contents() const 
  { if (last_selected == -1) return NULL; else return my_label[last_selected].c_str(); };
};

#endif
