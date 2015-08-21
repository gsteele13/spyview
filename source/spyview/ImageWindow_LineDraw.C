#include <stdio.h>
#include <list>
#include "ImageWindow.H"
#include "ImageWindow_LineDraw.H"
#include "LineDraw_Control.h"

static LineDraw_Control ctl;
using namespace std;

// ============== LINEDRAW EXTENSION; allow drawing of lines on images, store lines in file ==================== 
//static const char *line_file = "lines.out";
LineDraw *LineDrawer = NULL;

bool LineDraw::on() 
{ 
  return ctl.win->visible(); 
}

// Gnuplot table auto-update logic
// Once a second, check for changes.
// If there was no valid file (valid is false), any file is considered a change
// if there is a change, set tracking to true, and every 50 msec check for 
// changes.  If 50 msec goes by with no change, update the file.
static void updateTimer(void *)
{
  if(!ctl.autognu->value())
    return;
  
  Fl::add_timeout(LineDrawer->autoLoadGnuplotLines(),updateTimer,NULL);
}

static const double trackTime=0.05;
static const double watchTime=0.25;
double LineDraw::autoLoadGnuplotLines()
{
  const char *fname = ctl.file->value();
  struct stat gps;
  if(stat(fname,&gps) != 0)
    {
      tracking = false;
      valid = false;
      return watchTime;
    }
  if(!valid)
    {
      valid = true;
      tracking = true;
      last_gp_update = gps;
      return trackTime;
    }
  bool changed = last_gp_update.st_mtime < gps.st_mtime;
  changed = changed || (last_gp_update.st_size != gps.st_size);
  last_gp_update = gps;
  if(changed)
    {
      tracking = true;
      return trackTime;
    }
  if(tracking)
    {
      loadGnuplotLines(fname);
      if(ctl.autodelete->value())
	{
	  fprintf(stderr,"Autodelete\n");
	  fclose(fopen(fname,"w")); // Truncate the file
	  stat(fname,&last_gp_update); // Make sure we don't confuse ourselves
	}
      tracking=false;
      return watchTime;
    }
  return watchTime;
}

void LineDraw::watchGnuplot()
{
  Fl::remove_timeout(updateTimer,NULL);
  Fl::add_timeout(1.0,updateTimer,NULL);
  valid = false;
  tracking = false;
}

bool LineDraw::toggle()
{
  if(ctl.win->visible())
    ctl.win->hide();
  else
    ctl.win->show();
  if(first_on)
    {
      loadOldLines();
      first_on = false;
    }
  fprintf(stderr,"Linecut handler %s\n", on() ? "enabled" : "disabled");
  if(on())
    fl_cursor(FL_CURSOR_CROSS,FL_BLACK,FL_WHITE);
  else
    fl_cursor(FL_CURSOR_DEFAULT,FL_BLACK,FL_WHITE);  

}

void LineDraw::update_info_win()
{
  const char *fmt = ctl.nl_fmt->value();
  char buf[256];
  if (nearest_line != NULL)
    {
      double x1=nearest_line->x1;
      double x2=nearest_line->x2;
      double y1=nearest_line->y1;
      double y2=nearest_line->y2;

      if (ctl.nl_pixels->value())
	{
	  x1=iw->id.getX_inv(x1,iw->xzoom);
	  x2=iw->id.getX_inv(x2,iw->xzoom);
	  y1=iw->id.getX_inv(y1,iw->xzoom);
	  y2=iw->id.getX_inv(y2,iw->xzoom);
	}

      snprintf(buf, 256, fmt, x1); 
      ctl.nl_x1->value(buf);
      snprintf(buf, 256, fmt, x2); 
      ctl.nl_x2->value(buf);
      snprintf(buf, 256, fmt, y1); 
      ctl.nl_y1->value(buf);
      snprintf(buf, 256, fmt, y2); 
      ctl.nl_y2->value(buf);
      snprintf(buf, 256, fmt, (y2-y1)/(x2-x1)); 
      ctl.nl_slope->value(buf);
      
      if (ctl.nl_pixels->value())
	{
	  ctl.nl_xunit->value("");
	  ctl.nl_yunit->value("");
	  ctl.nl_slope_unit->value("");
	}
      else 
	{
	  ctl.nl_xunit->value(iw->id.xname.c_str());
	  ctl.nl_yunit->value(iw->id.yname.c_str());
	  ctl.nl_slope_unit->value((iw->id.yname + " / " + iw->id.xname).c_str());
	}
    }

  // Update the lines in the browser, and highlight the nearest line
  double a, b, c;
  ctl.line_display->clear();
  int n = 0;
  for(lines_t::iterator i = lines.begin(); i != lines.end(); i++)
    {
      n++;
      a = (i->y2-i->y1)/(i->x2-i->x1); // slope
      b = i->y2 - a*i->x2; // y intercept
      c = -b / a; // xintercept 
      char buf2[256];
      snprintf(buf2, 256, "%d\t%s\t%s\t%s", n, fmt, fmt, fmt);
      snprintf(buf, 256, buf2, a, b, c);
      // how do i send line "i" to void* data? 
      //ctl.line_display->add(buf, 0); 
      line *tmp = &(*i);
      ctl.line_display->add(buf, (void *) tmp); 
      if (nearest_line == &(*i))
	ctl.line_display->select(n);
    }
}

LineDraw::LineDraw(ImageWindow *iwp) : ImageWindow_Module(iwp) 
{
  LineDrawer = this;
  editing = NULL;
  edit_x = NULL;
  edit_y = NULL;
  mode = LINE_OFF;
  first_on = true;
  nearest_line = NULL;

  FILE *fp;
  snprintf(xsection_fn, sizeof(xsection_fn), "%sxsection_ovl.dat.%d", Gnuplot_Interface::tmpdir(), getpid());
  if ((fp = fopen(xsection_fn, "w")) == NULL)
    { error("Failed to create tempfile %s: %s\n", xsection_fn, strerror(errno)); }
  else fclose(fp);
}

LineDraw::~LineDraw()
{
  unlink(xsection_fn);
}

void LineDraw::loadGnuplotLines(const char *fname)
{
  static const double eps=5e-5;
  fprintf(stderr,"Opening %s\n",fname);
  FILE *in = fopen(fname, "r");
  if(!in)
    return; // \fixme ; should be warning here.
  char buf[1024];
  double x1,y1,x2,y2;
  int scount = 0;
  bool restart=true;
  x1=NAN; y1=NAN;
  lines.clear();
  while(fgets(buf,sizeof(buf),in) != NULL)
    {
      if(sscanf(buf," %lg %lg",&x2,&y2) == 2)
	{
	  if(!isnan(x2) && !isnan(y2) && !isnan(x1) && !isnan(y1))
	    {	      
	      line newline(x1,y1,x2,y2);
	      if(restart)
		{
		  lines.push_back(newline);
		  restart = false;
		}
	      else 
	       {
		 // If this line is along the last, just keep drawing.
		 double dt = fabs(lines.back().theta() - newline.theta());
		 while(dt > 2.0 * M_PI)
		   dt -= 2.0 * M_PI;
		 if(fabs(dt) > eps)			      
		   lines.push_back(newline);
		 else
		   {
		     scount++;
		     lines.back().x2 = newline.x2;
		     lines.back().y2 = newline.y2;
		   }
	       }
	    } 
	}
      else
	{
	  restart=true;
	  x2 = NAN;
	  y2 = NAN;
	}
      x1 = x2;
      y1 = y2;
    }
  fclose(in);
  fprintf(stderr,"Loaded " _STF " lines (%d combined away)\n",lines.size(),scount);
  iw->redraw_overlay();
}
void LineDraw::loadOldLines()
{
  loadLines("current_lines.lines");
}

void LineDraw::loadLines(const char *name)
{
  string filename = name;
  info("loading lines from %s\n", filename.c_str());
  FILE *in = fopen(filename.c_str(),"r");
  if(!in)
    {
      info("error opening file: %s\n", filename.c_str());
      return;
    }
  char buf[1024];
  double x1, y1;
  double x2, y2;
  lines.clear();
  int count = 0;
  while((fgets(buf,sizeof(buf),in)))
    {
      if(count == 0)
	{
	  if(sscanf(buf,"%lg %lg",&x1,&y1) == 2)
	    count++;
	  else 
	    count = 0;
	}
      else
	{
	  if(sscanf(buf,"%lg %lg",&x2,&y2) == 2)
	    {
	      lines.push_back(line(x1,y1,x2,y2));
	      x1 = x2;
	      y1 = y2;
	      count++;
	    }
	  else
	    count = 0;
	    
	}
    }
  fclose(in);
    
  fprintf(stderr,"Loaded " _STF " lines\n",lines.size());
  iw->redraw_overlay();
}

void LineDraw::update_file()
{
  saveLines("current_lines");
}

void LineDraw::saveLines(const char* name) 
{ 
  string filename = name;
  if (strstr(name, ".lines") == NULL)
    filename += ".lines";
  info("saving lines to %s\n", filename.c_str());
  FILE *f = fopen(filename.c_str(),"w");
  if(f == NULL) // We probably don't have write permission here.
    {
      info("error opening file: %s\n", filename.c_str());
      return;
    }
  fprintf(f,"x y theta(radians) tan(theta) abs(tan(theta)) theta(degrees) y-intercept\n");
  for(lines_t::iterator i = lines.begin(); i != lines.end(); i++)
    {
      double theta = atan2(i->y2-i->y1,i->x2-i->x1);
      fprintf(f,"%g %g %g %g %g %g\n%g %g\n\n", 
	      i->x1,i->y1, 
	      theta , tan(theta), fabs(tan(theta)), theta/M_PI*180.0,
	      i->x2,i->y2);
    }
  fclose(f);     
  filename = name;
  if (strstr(name, ".lines") == NULL)
    filename += ".lines";
  filename += ".slopes_and_intercepts.dat";
  info("saving slopes to %s\n", filename.c_str());
  f = fopen(filename.c_str(), "w");
  if(f == NULL) // We probably don't have write permission here.
    {
      info("error opening file: %s\n", filename.c_str());
      return;
    }
  fprintf(f, "# slope yintercept xintercept\n");
  int n=0;
  for(lines_t::iterator i = lines.begin(); i != lines.end(); i++)
    {
      n++;
      double a, b, c;
      a = (i->y2-i->y1)/(i->x2-i->x1); // slope
      b = i->y2 - a*i->x2; // y intercept
      c = -b / a; // xintercept 
      fprintf(f, "%d %e %e %e\n", n, a, b, c);
    }
  fclose(f);
};
  
// This finds not only the closest line, but also assigns the pointers
// edit_x and edit_y to the nearest endpoint of the nearest line.
// 
// 9 Jan 2010: we are somehow getting the wrong nearest point (always
// picking the point on the nearest line that has a smaller x value?)
// Simple fix: it was always just taking x1,y1. Now check which is
// closer.

LineDraw::line *LineDraw::find_closest_line(double x, double y)
{
  line *best_line = NULL;
  double best_dist = 1e100;

  // The position where the user clicked is x,y
  x = iw->id.getX_inv(x,iw->xzoom); // Convert to window coordinates to give the user a good ui experience.  Distances in pixels;
  y = iw->id.getY_inv(y,iw->yzoom);

  for(lines_t::iterator i = lines.begin(); i != lines.end(); i++)
    {
      // Modify to include calculation of d_perp, but only to consider
      // it if d1,d2 < length of line

      // For y = mx + b, shortest distance from point (x,y) is
      //
      // d = (y - mx - b)^2 / (m^2+1)
    
      double x1,x2,y1,y2;
      double d1,d2,d_perp,l,dmin;
      double m,b;

      // x1, y1, x2, y2 are the positions of the endpoints of the
      // current line we're considering
      x1 = iw->id.getX_inv(i->x1,iw->xzoom);  
      y1 = iw->id.getY_inv(i->y1,iw->yzoom);      

      x2 = iw->id.getX_inv(i->x2,iw->xzoom);  
      y2 = iw->id.getY_inv(i->y2,iw->yzoom);      
      
      // d1, d2 are the square distances from the click point to the
      // line endpoints
      d1 = (x-x1)*(x-x1)+(y-y1)*(y-y1);
      d2 = (x-x2)*(x-x2)+(y-y2)*(y-y2);

      // d_perp is the perpendicular distance to the line
      m = (y2-y1) / (x2-x1);
      b = y1 - m*x1;
      d_perp = (y-m*x-b)*(y-m*x-b)/(m*m+1);

      // l is the length of the line
      l = (x2-x1)*(x2-x1)+(y2-y1)*(y2-y1);

      // Find the minimum distance to the endpoints
      dmin = d1;
      if (d2 < dmin) dmin = d2;
      // also consider d_perp if the it is smaller than dmin and if
      // the point lies "between" the two endpoints
      if (fabs(d2-d1)<l && d_perp < dmin)
	dmin = d_perp;
      
      if (dmin < best_dist)
	{
	  best_dist = dmin;
	  best_line = &(*i);
	  // now choose the nearest endpiont:
	  if (d1 < d2)
	    {
	      edit_x = &(i->x1);
	      edit_y = &(i->y1);
	    }
	  else 
	    {
	      edit_x = &(i->x2);
	      edit_y = &(i->y2);
	    }
	}
    }
  if(best_line)
    {
      edit_x_orig = *edit_x;
      edit_y_orig = *edit_y;
    }
  return best_line;
};
  
int LineDraw::event_callback(int event)
{
  switch(event)
    {
    case FL_KEYDOWN:
      switch(Fl::event_key())
	{
	case 'l':
	  if (!(Fl::event_state() & FL_CTRL))
	    {
	      toggle();
	      return 1;
	    }
	}
    }
  if(!on())
    return 0;
  switch(event)
    {
    case FL_KEYDOWN:
      switch(Fl::event_key())
	{
	case 'i': // toggle highlighting
	  if (Fl::event_ctrl())
	    {
	      if (ctl.nl_info_win->shown()) ctl.nl_info_win->hide();
	      else ctl.nl_info_win->show();
	    }
	  else
	    ctl.highlight_nl->value(!ctl.highlight_nl->value());
	  iw->redraw_overlay();
	  return 1;
	case FL_Delete:
	case 'd': // Delete the closest line
	  if(Fl::event_shift())
	    {
	      while(!lines.empty())
		lines.pop_front();
	      iw->redraw_overlay();
	    }
	  else
	    {
	      editing = find_closest_line(iw->id.getX(iw->get_event_x_d()),iw->id.getY(iw->get_event_y_d()));
	      if(editing)
		{
		  for(lines_t::iterator i = lines.begin(); i != lines.end(); i++)
		    if(&(*i) == editing)
		      {
			lines.erase(i);
			iw->redraw_overlay();
			update_file();
			break;
		      }
		}
	    }
	  return 1;
	case 'x': // Extend the line to cross the entire window
	  {
	    // How does this work? and what does "big" do?
	    bool big=Fl::event_shift();
	    editing = find_closest_line(iw->id.getX(iw->get_event_x_d()), iw->id.getY(iw->get_event_y_d()));

	    if(!editing)
	      return 1;
	    // a vertical line: this code seems to work
	    if(editing->x1 == editing->x2)
	      {
		editing->y1 = iw->id.getY(big ? -iw->h : 0);
		editing->y2 = iw->id.getY(big ? 2 * iw->h : iw->h);
	      }
	    // otherwise, code does not work.
	    else
	      {
		double m = (editing->y2-editing->y1)/(editing->x2-editing->x1);
		// We need a "b" now that we are working in real units! (y=mx+b)
		double b = editing->y1-m*editing->x1;
		info("initial line: x1 %f x2 %f y1 %f y2 %f\n",
		     editing->x1, editing->x2, editing->y1, editing->y2);
		info("slope is %f\n", m);		
		if(big)
		  {
		    // I'm not sure why this is here: maybe for image printing?
		    editing->y1 = iw->id.getY(-iw->h);
		    editing->y2 = iw->id.getY(iw->h*2.0);
		    editing->x1 = (editing->y1-b)/m;
		    editing->x2 = (editing->y2-b)/m;
		  }
		else
		  {
		    // I am assuming there is drawing code that
		    // properly handles shallow lines where X(getY(0))
		    // is outside the window
		    editing->y1 = iw->id.getY(0);
		    editing->y2 = iw->id.getY(iw->h);
		    editing->x1 = (editing->y1-b)/m;
		    editing->x2 = (editing->y2-b)/m;
		  }
		info("window size:  x1 %f x2 %f y1 %f y2 %f\n",
		     iw->id.getX(0), iw->id.getX(iw->w), iw->id.getY(0), iw->id.getY(iw->h));
		info("final line:   x1 %f x2 %f y1 %f y2 %f\n",
		     editing->x1, editing->x2, editing->y1, editing->y2);
		info("new slope: %f\n", 
		     (editing->y2-editing->y1)/(editing->x2-editing->x1));
	      }
	    editing = NULL;
	    iw->redraw_overlay();
	    update_file();
	    return 1;
	  }
	}
      break;    
    case FL_PUSH:
      {
	if(mode != LINE_OFF)
	  return 1;
	switch(Fl::event_button())
	  {
	  case 1:
	    ltmp.x1=iw->id.getX(iw->get_event_x_d());
	    ltmp.y1=iw->id.getY(iw->get_event_y_d());
	    mode = LINE_DRAWING;
	    return 1;
	  case 3:
	    editing = find_closest_line(iw->id.getX(iw->get_event_x_d()), iw->id.getY(iw->get_event_y_d()));
	    if(editing)
	      edit_original = *editing;
	    push_x = iw->id.getX(iw->get_event_x_d());
	    push_y = iw->id.getY(iw->get_event_y_d());
	    mode = LINE_DRAGGING;
	    iw->redraw_overlay();
	    return 1;
	  case 2:
	    editing = find_closest_line(iw->id.getX(iw->get_event_x_d()), iw->id.getY(iw->get_event_y_d()));
	    if(!editing)
	      return 1;
	    push_x = iw->id.getX(iw->get_event_x_d());
	    push_y = iw->id.getY(iw->get_event_y_d());
	    mode = LINE_STRETCHING;
	    iw->redraw_overlay();
	    return 1;
	  }

      }
    case FL_DRAG:
    case FL_RELEASE:
      switch(mode)
	{
	case LINE_OFF:
	  return 1;
	case LINE_DRAWING:
	  switch(event)
	    {
	    case FL_DRAG:
	      ltmp.x2 = iw->id.getX(iw->get_event_x_d());
	      ltmp.y2 = iw->id.getY(iw->get_event_y_d());
	      iw->redraw_overlay();
	      return 1;
	    case FL_RELEASE:
	      ltmp.x2 = iw->id.getX(iw->get_event_x_d());
	      ltmp.y2 = iw->id.getY(iw->get_event_y_d());
	      lines.push_back(ltmp);
	      mode = LINE_OFF;
	      iw->redraw_overlay();
	      update_file();
	      return 1;
	    }
	  break;
	case LINE_DRAGGING:
	  switch(event)
	    {
	    case FL_DRAG:
	      if(!editing)
		return 1;
	      *editing = edit_original;
	      editing->offset(iw->id.getX(iw->get_event_x_d())-push_x, iw->id.getY(iw->get_event_y_d())-push_y);
	      iw->redraw_overlay();
	      return 1;
	    case FL_RELEASE:
	      if(editing)
		{
		  *editing = edit_original;
		  editing->offset(iw->id.getX(iw->get_event_x_d())-push_x, iw->id.getY(iw->get_event_y_d())-push_y);
		}
	      editing = NULL;
	      mode = LINE_OFF;
	      iw->redraw_overlay();
	      update_file();
	      return 1;
	    }
	  break;

	case LINE_STRETCHING:
	  switch(event)
	    {
	    case FL_DRAG:
	      if(!editing)
		return 1;
	      *edit_x = edit_x_orig + iw->id.getX(iw->get_event_x_d()) - push_x;
	      *edit_y = edit_y_orig + iw->id.getY(iw->get_event_y_d()) - push_y;
	      iw->redraw_overlay();
	      return 1;
	    case FL_RELEASE:
	      if(editing)
		{
		  *edit_x = edit_x_orig + iw->id.getX(iw->get_event_x_d()) - push_x;
		  *edit_y = edit_y_orig + iw->id.getY(iw->get_event_y_d()) - push_y;
		}
	      editing = NULL;
	      mode = LINE_OFF;
	      iw->redraw_overlay();
	      update_file();
	      return 1;
	    }
	  break;
	}
    case FL_ENTER:
      return 1;
    case FL_MOVE:
      line *onl = nearest_line;
      nearest_line = find_closest_line(iw->id.getX(iw->get_event_x_d()),iw->id.getY(iw->get_event_y_d()));
      //info("nearest line x1 %f", nearest_line->x1);
      if (nearest_line != onl)
	{
	  if (ctl.highlight_nl->value())
	    iw->redraw_overlay();
	  if (ctl.nl_info_win->shown())
	    update_info_win();
	}
      return 0; // return 0 so that the cursor position update also gets this event
    }
  return 0;
}

void LineDraw::linecut_callback(bool init)
{
  bool needplot = false;
  if(iw->line_cut_type != VERTLINE)
    return;

  FILE *f = fopen(xsection_fn,"w");
  if(f == NULL)
    {
      warn("Unable to open crossection overlay file\n");
      return;
    }
  double x = iw->id.getX(iw->line_cut_xp);
  for(lines_t::iterator i = lines.begin(); i != lines.end(); i++)
    {
      if((i->x1 <= x && i->x2 >= x) || (i->x1 >= x && i->x2 <= x))
	{
	  if(i->x1 == i->x2)
	    continue;
	  double y = ((i->y2-i->y1)/(i->x2-i->x1))*(x-i->x1) + i->y1;
	  if(iw->id.getY_inv(y) < 0 || iw->id.getY_inv(y) > iw->h)
	    continue;
	  needplot = true;
	  fprintf(f,"%g %g\n",y,iw->dataval(iw->line_cut_xp, iw->id.getY_inv(y)));
	}
    }
  fclose(f);
  
  if(!needplot)
    return;
  Gnuplot_Interface &gp = iw->gplinecut; // Handy, but not important.
  gp.cmd(", \"%s\" u 1:2 w points ps 1.5 pt 2 lt 3 ti \"\"",xsection_fn);
}

void LineDraw::overlay_callback()
{
  if(mode == LINE_DRAWING)
    {
      fl_color(FL_GREEN);
      fl_line((int)iw->id.getX_inv(ltmp.x1,iw->xzoom),(int)iw->id.getY_inv(ltmp.y1,iw->yzoom), 
	      (int)iw->id.getX_inv(ltmp.x2,iw->xzoom),(int)iw->id.getY_inv(ltmp.y2,iw->yzoom));
    }

  if (ctl.nl_info_win->shown())
    update_info_win();

  for(lines_t::iterator i = lines.begin(); i != lines.end(); i++)
    {
      if(editing == &(*i))
	fl_color(FL_GREEN);
      else if (ctl.highlight_nl->value() && nearest_line != NULL)
	{
	  if (nearest_line == &(*i))  // note: &(*i) == ... doesn't work, but ... == &(*i) does.
	    fl_color(FL_YELLOW);
	  else
	    fl_color(FL_BLUE);
	}
      else
	fl_color(FL_BLUE);
      int x1 = (int)iw->id.getX_inv((*i).x1,iw->xzoom);
      int x2 = (int)iw->id.getX_inv((*i).x2,iw->xzoom);
      int y1 = (int)iw->id.getY_inv((*i).y1,iw->yzoom);
      int y2 = (int)iw->id.getY_inv((*i).y2,iw->yzoom);

      fl_line(x1,y1,x2,y2);
      if(ctl.endpoints->value())
	{
	  fl_color(FL_RED);
	  fl_line(x1-1,y1-1,x1+1,y1+1);
	  fl_line(x1-1,y1+1,x1+1,y1-1);
	  fl_line(x2-1,y2-1,x2+1,y2+1);
	  fl_line(x2-1,y2+1,x2+1,y2-1);
	}

    }
};



