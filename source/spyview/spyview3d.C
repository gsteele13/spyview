#include <stdio.h>
#include "spyview3d.H"
#include "ImageWindow3d.H"
#include "spyview3d_ui.h"

#include <FL/fl_ask.H>
#include <string.h>
#include <FL/Fl_File_Chooser.H>
#include <FL/filename.H>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
// STL stuff
//# include <string.h>
//#include <string>
//#include <vector>

#include "mypam.h"

using namespace std;

int *data;
int *databuf;
int w,h;
vector<string> filenames;
vector<string> cmapfiles;

int global_shortcuts(int event);

int main(int argc, char **argv)
{
  if (argc == 1)
    {
      fprintf(stderr, "Please specify filename\n");
      exit(-1);
    }

  Fl::visual(FL_RGB);
  make_window();
  
  // Construct a list of valid filenames that were given on the input

  int num;
  int fd;
  string fn;
  for (int i=1; i < argc; i++)
    {
      if ( (fd = open(argv[i], O_RDONLY)) != -1)
	{
	  // Make a vector of filenames using the STL
	  fn = argv[i];
	  filenames.push_back(fn);
	  num = filech->add(" ", 0, filech_cb);
	  filech->replace(num, argv[i]);
	  //fprintf(stderr, "adding %s as %s\n", argv[i], (filenames[i-1]).c_str());
	  close(fd);
	}
    }

  if (filech->size() == 0)
    {
      fprintf(stderr, "No valid files found\n");
      exit(-1);
    }

  // Construct a list of colormap files
  // First scan them from /usr/share/spyview/cmaps, then look in ~/cmaps/

  int n;
  struct dirent **namelist;

  string share_path = "/usr/share/spyview/cmaps";
  n = fl_filename_list(share_path.c_str(), &namelist, fl_casealphasort);
  for (int i = 0; i<n ; i++)
    {
      if (strstr(namelist[i]->d_name, ".ppm") != NULL)
	
	{
	  cmapch->add(namelist[i]->d_name, 0, 0);
	  fn = namelist[i]->d_name;
	  cmapfiles.push_back(share_path + "/" + fn);
	}
    }

  int m;
  char *home = getenv("HOME"); 
  string home_path = home;
  home_path += "/cmaps";
  n = fl_filename_list(home_path.c_str(), &namelist, fl_casealphasort);
  for (int i = 0; i<n ; i++)
    {
      if (strstr(namelist[i]->d_name, ".ppm") != NULL)
	
	{
	  fn = namelist[i]->d_name;	  
	  m = cmapch->add(("~ "+fn).c_str(), 0, 0);
	  cmapfiles.push_back(home_path + "/" + fn);
	}
    }

  options->add("fft");
  options->add("ac");
  options->add("log");
  options->add("sub lbl");

  iw3d->external_update = update_gui;
  update_gui();
  imagewindow->show();
  iw3d->show();
  control->show();

  // Load the file
  filech->value(0);
  filech->do_callback(filech, (void *)argv[1]);
  cmapch->value(17); //mountain-lakes colormap
  cmapch->do_callback();
  Fl::add_handler(&global_shortcuts);
  Fl::run();
}

int global_shortcuts(int event)
{
  int n;
  switch (event) 
    {
    case FL_SHORTCUT:
      n = Fl::event_key();
      switch (n) 
	{
	case 'c':
	  if (control->shown())
	    control->hide();
	  else
	    control->show();
	  return 1;
	  break;
	case 'q':
	  exit(0);
	case FL_Right:
	case ' ':
	case 'f':
	  n = filech->value();
	  if (n == filech->size() - 2)
	    {
	      filech->value(0);
	    }
	  else
	    filech->value(n+1);
	  filech->do_callback();
	  return 1;
	  break;
	case FL_Left:
	case FL_BackSpace:
	case 'd':
	  n = filech->value();
	  if (n == 0)
	    filech->value(filech->size()-2);
	  else
	    filech->value(n-1);
	  filech->do_callback();
	  return 1;
	  break;
	case FL_Down:
	case 'j':
	  n = cmapch->value();
	  if (n == cmapch->size() - 2)
	    cmapch->value(0);
	  else
	    cmapch->value(n+1);
	  cmapch->do_callback();
	  return 1;
	  break;
	case FL_Up:
	case 'k':
	  n = cmapch->value();
	  if (n == 0)
	    cmapch->value(cmapch->size()-2);
	  else
	    cmapch->value(n-1);
	  cmapch->do_callback();
	  return 1;
	  break;
	}
    }
  return 0;
}

void filech_cb(Fl_Widget *o, void *)
{
  const char *filename = filenames[filech->value()].c_str();
  iw3d->loadData(filename);
  char label[1024];
  snprintf(label, 1024, "%s", filech->text());
  imagewindow->label(label);
  char buf[1024];
  char *p;
  strncpy(buf, filename, 1024);
  p = strrchr(buf, '.');
  if (p == NULL)
    p = buf + strlen(buf);
  strcpy(p, ".out");
  savebox->value(buf);
  iw3d->redraw();
}

void saveb_cb(Fl_Widget *, void *)
{
  GLint format;
  if (postscriptbutton->value()) 
    format = GL2PS_PS;
  else if (epsbutton->value()) 
    format = GL2PS_EPS;
  else if (latexbutton->value())
    format = GL2PS_TEX;
  iw3d->saveFile(savebox->value(), format);
  fl_message("File was saved to %s", savebox->value());
}


void cmapch_cb(Fl_Widget *o, void*)
{
  FILE *fp;
  const char *filename = cmapfiles[cmapch->value()].c_str();
  fp = fopen(filename, "r");
  if (fp == NULL)
    {
      perror(filename);
      exit(-1);
    }
  
  int rows, cols;
  pixel **image;
  pixval maxval;

  image = ppm_readppm(fp, &cols, &rows, &maxval);
  fclose(fp);

  if (cols > 1)
    {
      fprintf(stderr, "Invalid colormap %s: must contain only one column!\n", filename);
      exit(-1);
    }

  if (rows > 256)
    {
      fprintf(stderr, "Invalid colormap %s: must contain 256 rows!\n", filename);
      exit(-1);
    }
  
  if (maxval != 255)
    {
      fprintf(stderr, "Invalid colormap %s: color depth must be 24 bit (255 maxval)\n", filename);
      exit(-1);
    }

  for (int i=0; i<rows; i++)
    {
      iw3d->colormap[0][i] = image[0][i].r;
      iw3d->colormap[1][i] = image[0][i].g;
      iw3d->colormap[2][i] = image[0][i].b;
    }

  ppm_freearray(image, rows);
  iw3d->redraw();
  char label[1024];
  snprintf(label, 1024, "%s - %s", filech->text(), cmapch->text());
  iw3d->label(label);
}

void update_gui()
{
  psival->value(iw3d->getPsi());
  thetaval->value(iw3d->getTheta());
  zoomval->value(iw3d->getScaleX());
  zscaleval->value(iw3d->getScaleZ()/iw3d->getScaleX());
  xtranval->value(iw3d->getTranslateX());
  ytranval->value(iw3d->getTranslateY());
  //fprintf(stderr, "% 6.2f % 6.2f % 6.2f\n", iw3d->getScaleX(), iw3d->getScaleY(), iw3d->getScaleZ());
}
