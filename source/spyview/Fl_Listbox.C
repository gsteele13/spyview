#include "Fl_Listbox.H"
#include <stdio.h>
Fl_Listbox::Fl_Listbox(int x, int y, int w, int h, const char *l) : Fl_Table(x,y,w,h,l) 
{ 
  callback(&event_callback_static, (void*)this);
  my_row_height = 20;
  my_col_width = 1;
  my_text_height = 14;
  //my_type = LISTBOX_VERTICAL;
  my_type = LISTBOX_HORIZONTAL;
  last_selected = -1;
  when(FL_WHEN_RELEASE);
  color(FL_WHITE);
  end(); 
}

void Fl_Listbox::recalculate_rows()
{
  if (my_type == LISTBOX_HORIZONTAL)
    {
      int sb_size = hscrollbar->visible() ? hscrollbar->h() : 0;
      row_height_all(my_row_height);
      col_width_all(my_col_width);
      int nr = (h()-2-sb_size)/my_row_height;
      int nc = my_label.size()/nr+1;
      rows(nr);
      cols(nc);
    }
  else if (my_type == LISTBOX_VERTICAL)
    {
      int sb_size = vscrollbar->visible() ? vscrollbar->w() : 0;
      row_height_all(my_row_height);
      col_width_all(my_col_width);
      int nc = (w()-2-sb_size)/my_col_width;
      int nr = my_label.size()/nc+1;
      rows(nr);
      cols(nc);
    }
}

void Fl_Listbox::resize(int x, int y, int w, int h)
{
  Fl_Table::resize(x,y,w,h);
  recalculate_rows();
}

void Fl_Listbox::add(const char *l)
{
  my_label.push_back(l);
  my_selected.push_back(0);
  int w, h;
  fl_font(FL_HELVETICA, my_text_height);
  fl_measure(l, w, h);
  if (w > my_col_width) my_col_width = w+30;
  recalculate_rows();
}

void Fl_Listbox::remove(int i)
{
  if (i<my_label.size())
    {
      my_label.erase(my_label.begin()+i-1); 
      my_selected.erase(my_selected.begin()+i-1); 
    }  
  redraw();
}

void Fl_Listbox::draw_cell(TableContext context, int R, int C, int X, int Y, int W, int H)
{
  switch (context)
    {
    case CONTEXT_CELL:
      fl_push_clip(X, Y, W, H);
      if (C*rows()+R < my_label.size())
	{
	  if (C*rows()+R == last_selected)
	    fl_draw_box(FL_BORDER_BOX, X, Y, W, H, my_selected[C*rows()+R] ? FL_DARK_BLUE : FL_WHITE);
	  else
	    fl_draw_box(FL_FLAT_BOX, X, Y, W, H, my_selected[C*rows()+R] ? FL_DARK_BLUE : FL_WHITE);
	  fl_color(my_selected[C*rows()+R] ? FL_WHITE : FL_BLACK);
	  fl_font(FL_HELVETICA, my_text_height);
	  fl_draw(my_label[C*rows()+R].c_str(), X+1, Y+1, W-2, H-2, FL_ALIGN_LEFT);
	}
      else 
	fl_draw_box(FL_FLAT_BOX, X, Y, W, H, FL_WHITE);
      fl_pop_clip();
      return;
    default:
      return;
    }
}

int Fl_Listbox::handle(int event)
{
  int retv = 0; 

  int shiftstate = (Fl::event_state() & FL_CTRL) ? FL_CTRL :
    (Fl::event_state() & FL_SHIFT) ? FL_SHIFT : 0;
  	  
  // Find which row/col we're over
  int R, C;  				// row/column being worked on
  ResizeFlag resizeflag;		// which resizing area are we over? (0=none)
  TableContext context = cursor2rowcol(R, C, resizeflag);

  switch (event)
    {
    case FL_PUSH:
      if ((C*rows()+R >= my_label.size() || context == CONTEXT_TABLE) && shiftstate == 0)
	{
	  //printf("Row %d Col %d Context %d\n", R,C,context);
	  for (int i=0; i<my_label.size(); i++)
	    my_selected[i] = 0;
	  fprintf(stderr, "clicks %d\n", Fl::event_clicks());
	  redraw();
	  return 1;
	}
      if (Fl::event_button() == 1)
	{
	  if (context == CONTEXT_CELL)
	    {
	      switch (shiftstate)
		{
		case FL_CTRL:
		  my_selected[C*rows()+R] = !my_selected[C*rows()+R];
		  last_selected = C*rows()+R;
		  redraw();
		  retv = 1;
		  break;
		case FL_SHIFT:
		  if (last_selected == -1)
		    {
		      my_selected[C*rows()+R] = 1;
		      last_selected = C*rows()+R;
		      redraw();
		      retv = 1;
		    }
		  else
		    {
		      int start = (last_selected < C*rows()+R) ? last_selected : C*rows()+R;
		      int end = (last_selected >= C*rows()+R) ? last_selected : C*rows()+R;
		      for (int i=start; i<=end; i++)
			my_selected[i] = 1;
		      last_selected = C*rows()+R;
		      redraw();
		      retv = 1;
		    }
		  break;
		case 0:
		  last_selected = C*rows()+R;
		  for (int i=0; i<my_label.size(); i++)
		    {
		      if (i == C*rows()+R) my_selected[i] = 1;
		      else my_selected[i] = 0;
		      redraw();
		    }
		  retv = 1;
		  break;
		}
	    }
	}
    }
  Fl_Table::handle(event); // Fl_Table seems to eat all events, so we'll ignore it's return value.
  return retv;
}

// Callback whenever someone clicks on different parts of the table
void Fl_Listbox::event_callback_static(Fl_Widget*, void *data)
{
    Fl_Listbox *o = (Fl_Listbox*)data;
    o->event_callback();
}

void Fl_Listbox::event_callback()
{
  return; 

  int R = callback_row();
  int C = callback_col();
  TableContext context = callback_context();
  int e = Fl::event();

  switch ( context )
    {
    case CONTEXT_CELL:
      {
	if (e == FL_PUSH)
	  {
	    my_selected[C*rows()+R] = !my_selected[C*rows()+R];
	    return;
	  }
      }
    default:
      return;
    }
}
