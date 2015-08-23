#include <stdio.h>
#include "spybrowse_ui.h"
#include "spybrowse.h"
#include "message.h"
#include <Fl/filename.H>
#include <Fl/fl_ask.H>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

string spyview_cmd;

int main(int argc, char **argv)
{
  spyview_cmd="spyview ";

  Fl::visual(FL_RGB);
  Fl::get_system_colors();
  
  // If we do not have spyview in the default path, we need to give a
  // full path to spyview.

  // The only time that spyview is not in the default path is if it is
  // being run under win32 not from the command line.

  // In this case, argv[0] will contain the full path to where
  // spybrowse was executed, and thus, also to where the spyview
  // executable is.

#ifdef WIN32
  if (getenv("SPYVIEW_DIR") != NULL) // otherwise get a windows crash?
    spyview_cmd = getenv("SPYVIEW_DIR");
  else
    {
      // Spyview is not in the path...probably some dumb windows
      // user...fortunately, they probably clicked on the
      // spybrowse.exe, so we can get the path from argv[0];
      spyview_cmd = argv[0];
      int c2 = spyview_cmd.find_last_of("\\");
      if(c2 >= 0)
	spyview_cmd.replace(c2,string::npos, "");
      spyview_cmd += "\\";
      info("spyview not in path: guessing %s as path from argv[0]\n", spyview_cmd.c_str());
    }
  if (spyview_cmd.size() == 0)
    {
      warn("Could not find good spyview_cmd: reverting to '.'\n");
      spyview_cmd = ".";
    }
  spyview_cmd = "start " + spyview_cmd + "spyview_console.exe ";
  info("spyview_cmd is %s\n", spyview_cmd.c_str());
#endif

  make_window();
  update_pattern();
  patternbox->value("*.{Stm,pgm,TFR,TRR,FRR,FFR,dat,mtx}");
  string title = "spybrowse ";
  char buf[1024*4]; 
  if (getcwd(buf,1024) == NULL)
    {
      fprintf(stderr, "directory longer than 4kbytes?");
      exit(-1);
    }
  directory->value(buf);
  title += directory->value();
  refreshb->do_callback();
  win->label(title.c_str());
  win->show();
  Fl::run();
}

void update_pattern()
{
  string pattern = "*.{";
  if (dat->value())
    pattern += "dat,";
  if (mtx->value())
    pattern += "mtx,";
  if (pgm->value())
    pattern += "pgm,";
  if (other->value())
    pattern += "Stm,TFR,TRR,FRR,FFR";
  pattern += "}";
  patternbox->value(pattern.c_str());
  refreshb->do_callback();
}

 
void refreshb_cb(Fl_Widget *, void *)
{
  dirent **list;
  int n;
  
  n = fl_filename_list(directory->value(), &list);
  
  fb->clear();
  
  for (int i = 0; i<n; i++)
    if (fl_filename_match(list[i]->d_name, patternbox->value()))
      fb->add(list[i]->d_name);
}

void fb_cb(Fl_Widget *, void*)
{
  int nselected = 0;
  for (int i=1; i<=fb->size(); i++)
    {
      if (fb->selected(i))
	nselected++;
    }
}

void viewb_cb(Fl_Widget *, void*)
{
  int n = 0;
  string cmd = spyview_cmd;
  for (int i = 1; i<=fb->size(); i++)
    if (fb->selected(i))
      {
	cmd += fb->text(i);
	cmd += " ";
	n++;
      }
  cmd += "&";
  if (n == 0)
    fl_alert("No files selected.");
  else
    system(cmd.c_str());
}

void view2b_cb(Fl_Widget *, void*)
{
  int n = 0;
  string cmd = spyview_cmd;
  string tmp;
  for (int i = 1; i<=fb->size(); i++)
    if (fb->selected(i))
      {
	tmp = cmd;
	tmp += fb->text(i);
	tmp += "&";
	info("command %s\n", tmp.c_str());
	system(tmp.c_str());
#ifndef WIN32
	sleep(1);
#else
	Sleep(1000);
#endif
      }
}
