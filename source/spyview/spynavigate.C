#include "spynavigate_ui.h"
#include <math.h>
#include <stdio.h>
#include <FL/fl_ask.H>
#include <string.h>
#include <FL/filename.H>

using namespace std;

extern "C"  {
#include <pgm.h>
}

double xstart,xinc,xend;
double ystart,yinc,yend;
double xcurrent, ycurrent;
char filename[256];
char pattern[256];

char output_filename[256];

struct mtx mtxd;

vector<string> cmapfiles;

void usage()
{
  fprintf(stderr, "spynavigate xstart,xinc,xend ystart,yinc,yend pattern (test_%03.f_%03.f)\n ");
  exit(0);
}  

int main(int argc, char **argv)
{
  mtxd.data = NULL;
  
  output_counter=0;

  if (argc < 4)
    usage();

  if (sscanf(argv[1], "%lf,%lf,%lf", &xstart, &xinc, &xend) != 3)
    usage();

  if (sscanf(argv[2], "%lf,%lf,%lf", &ystart, &yinc, &yend) != 3)
    usage();
  
  strncpy(pattern, argv[3], 256);

  Fl::visual(FL_RGB);
  make_window();

  // Construct a list of colormap files
  // First scan them from /usr/share/spyview/cmaps, then look in ~/cmaps/

  int n;
  int fd;
  struct dirent **namelist;
  string fn;
  int default_colormap=0;

  string share_path = "/usr/share/spyview/cmaps";
  n = fl_filename_list(share_path.c_str(), &namelist, fl_casealphasort);
  for (int i = 0; i<n ; i++)
    {
      if (strstr(namelist[i]->d_name, ".ppm") != NULL)
	{
	  cmapch->add(namelist[i]->d_name, 0, 0);
	  fn = namelist[i]->d_name;
	  if (fn == "blue-green-pink.ppm")
	    default_colormap = i-2; // fl_filename_list will include . and ..
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

  xcurrent=xstart;
  ycurrent=ystart;

  iw->setGamma(1.0,0.0);

  xstartbox->value(xstart);
  xincbox->value(xinc);
  xendbox->value(xend);

  ystartbox->value(ystart);
  yincbox->value(yinc);
  yendbox->value(yend);

  update();

  iw->show();
  controls->show();
  Fl::add_handler(keyhandler);

  cmapch->value(default_colormap);
  cmapch->do_callback();
  
  Fl::run();
}

void cmapch_cb(Fl_Widget *o, void*)
{
  FILE *fp;
  const char *name = cmapfiles[cmapch->value()].c_str();

  pixel **image;
  pixval maxval;
  int rows, cols;
  
  fp = fopen(name, "r");
  if (fp == NULL)
    {
      perror(filename);
      exit(-1);
    }

  image = ppm_readppm(fp, &cols, &rows, &maxval);
  fclose(fp);

  uchar newcmap[3*rows];
  
  if (cols > 1)
    {
      fprintf(stderr, "Invalid colormap %s: must contain only one column!\n", filename);
      exit(-1);
    }
  
  if (maxval != 255)
    {
      fprintf(stderr, "Invalid colormap %s: color depth must be 24 bit (255 maxval)\n", filename);
      exit(-1);
    }
  
  for (int i=0; i<rows; i++)
    {
      newcmap[i*3] = image[0][i].r;
      newcmap[i*3+1] = image[0][i].g;
      newcmap[i*3+2] = image[0][i].b;
    }

  ppm_freearray(image, rows);

  iw->setColormap(newcmap, rows);
}


int keyhandler(int event)
{
  int key;
  switch (event)
    {
    case FL_SHORTCUT:
      key = Fl::event_key();
      switch (key)
	{
	case 'q':
	  exit(0);
	case FL_Right:
	  xcurrent = (xcurrent+xinc > xend) ? xend : xcurrent+xinc;
	  update();
	  return 1;
	case FL_Left:
	  xcurrent = (xcurrent-xinc < xstart) ? xstart : xcurrent-xinc;
	  update();
	  return 1;
	case FL_Down:
	  ycurrent = (ycurrent+yinc > yend) ? yend : ycurrent + yinc;
	  update();
	  return 1;
	case FL_Up:
	  ycurrent = (ycurrent-yinc < ystart) ? ystart : ycurrent - yinc;
	  update();
	  return 1;
	case 's':
	  sprintf(iw->output_basename, "%03.0f", output_counter->value());
	  iw->saveFile();
	  fprintf(stderr, "Saved file %s\n", output_filename);
	  output_counter->value(output_counter->value()+1);
	  return 1;
	}	  
    }
  return 0;
}

void update()
{
  FILE *fp;

  snprintf(filename, 256, pattern, xcurrent, ycurrent);

  x_box->value(xcurrent);
  y_box->value(ycurrent);
  pattern_box->value(pattern);
  filename_box->value(filename);

  if ( (fp = fopen(filename, "r")) == NULL)
    {
      fprintf(stderr, "Error opening file %s:", filename);
      perror(NULL);
    }
  else
    {
      fclose(fp);
      load_file();
    }
}
  
void load_file()
{
  
  if (strcmp(fl_filename_ext(filename), ".mtx") == 0)
    {
      iw->square = 0; // turn off "squaring" the image for MTX data.
      xsecwin->show();
      read_mtx();
    }
  else
    {
      iw->loadData(filename);
      xsecwin->hide();
      // Perform the requested histogram adjustments:

      iw->normalize();
      adjustCenterPeak();

      dim->deactivate();
      indexroller->deactivate();
      indexbox->deactivate();
      indexslider->deactivate();
    }

  char label[1024];
  iw->label(filename);
  char buf[1024];
  buf[0] = 0;
  strncat(buf, filename, 1024);
  strncat(buf, ".ppm", 1024-sizeof(filename));
}

void read_mtx()
{
  FILE *fp = fopen(filename, "r");
  char buf[256];
  int i,j,k;
  fgets(buf, 256, fp);
  int bytes = 8;
  int oldsize = mtxd.size[0]*mtxd.size[1]*mtxd.size[2];

  double oldmax, oldmin;
  oldmax = mtxd.datamax;
  oldmin = mtxd.datamin;
  
  if (sscanf(buf, "%d %d %d", &mtxd.size[0], &mtxd.size[1], &mtxd.size[2]) != 3)
    { fprintf(stderr, "Malformed mtx header: %s", filename); exit(-1); }
  if (sscanf(buf, "%*d %*d %*d %d", &bytes) != 1)
    { fprintf(stderr, "Legacy mtx file found (%s): assuming double data (bytes = %d)\n", filename, bytes);}
  if (mtxd.size[0]*mtxd.size[1]*mtxd.size[2] != oldsize)
    mtxd.data = (double *) realloc(mtxd.data, mtxd.size[0]*mtxd.size[1]*mtxd.size[2]*sizeof(double));
  mtxd.datamax = -1e100;
  mtxd.datamin = 1e100;
  for (i=0; i<mtxd.size[0]; i++)
    for (j=0; j<mtxd.size[1]; j++)
      for (k=0; k<mtxd.size[2]; k++)
	{
	  if (bytes == 4)
	    {
	      float tmp;
	      if (fread(&tmp, bytes, 1, fp) != 1) 
		{ fprintf(stderr, "Short read on mtx file: %s", filename); exit(-1); }
	      if (tmp < mtxd.datamin) mtxd.datamin = tmp;
	      if (tmp > mtxd.datamax) mtxd.datamax = tmp;
	      mtxd.data[i*mtxd.size[1]*mtxd.size[2]+j*mtxd.size[2]+k] = tmp;
	    }
	  else if (bytes == 8)
	    {
	      double tmp;
	      if (fread(&tmp, bytes, 1, fp) != 1) 
		{ fprintf(stderr, "Short read on mtx file: %s", filename); exit(-1); }
	      if (tmp < mtxd.datamin) mtxd.datamin = tmp;
	      if (tmp > mtxd.datamax) mtxd.datamax = tmp;
	      mtxd.data[i*mtxd.size[1]*mtxd.size[2]+j*mtxd.size[2]+k] = tmp;
	    }
	  else 
	    { fprintf(stderr, "Unsupported number of bytes %d", bytes); exit(0);}
	}

  if (!mtx_adjust->value())
    { mtxd.datamax = oldmax; mtxd.datamin = oldmin; }

  datamax_box->value(mtxd.datamax);
  datamin_box->value(mtxd.datamin);

  dim->activate();
  indexroller->activate();
  indexbox->activate();
  indexslider->activate();
  load_mtx_cut();
}

void load_mtx_cut()
{
  int d = dim->value();

  indexbox->maximum(mtxd.size[d]-1);
  indexslider->maximum(mtxd.size[d]-1);
  indexroller->maximum(mtxd.size[d]-1);

  // Double the width of the transfer function to give us some room to play with the histogram
  double center, width, mapmin, mapmax;
  center = (mtxd.datamax+mtxd.datamin)/2;
  width = mtxd.datamax - mtxd.datamin;
  mapmin = center - width;
  mapmax = center + width;

  int index = indexbox->value();

  double tmp;

  if (d == 0) // d2 = 1, d3 = 2
    {
      int data[mtxd.size[1]*mtxd.size[2]];
      for (int j=mtxd.size[1]-1; j>=0; j--)
	for (int k=mtxd.size[2]-1; k>=0; k--)
	  {
	    tmp = mtxd.data[index*mtxd.size[1]*mtxd.size[2] + j*mtxd.size[2] + k];
	    data[k*mtxd.size[1]+j] = 
	      (int) round((1.0*(tmp - mapmin)/(mapmax-mapmin)*65535.0));
	  }
      iw->loadData(data, mtxd.size[1], mtxd.size[2]);
    }	   
  else if (d == 1) // d2 = 2, d3 = 0
    {
      int data[mtxd.size[2]*mtxd.size[0]];
      for (int k=0; k<mtxd.size[2]; k++)
	for (int i=0; i<mtxd.size[0]; i++)
	  {
	    tmp = mtxd.data[i*mtxd.size[1]*mtxd.size[2] + index*mtxd.size[2] + k];
	    data[k*mtxd.size[0]+i] = 
	      (int) round((1.0*(tmp - mapmin)/(mapmax-mapmin)*65535.0));
	  }
      iw->loadData(data, mtxd.size[0], mtxd.size[2]);
    }	   
  else if (d == 2) // d2 = 0, d3 = 1
    {
      int data[mtxd.size[0]*mtxd.size[1]];

      for (int i=0; i<mtxd.size[0]; i++)
	for (int j=0; j<mtxd.size[1]; j++)
	  {
	    tmp = mtxd.data[i*mtxd.size[1]*mtxd.size[2] + j*mtxd.size[2] + index];
	    data[i*mtxd.size[1]+j] = 
	      (int) round((1.0*(tmp - mapmin)/(mapmax-mapmin)*65535.0));
	  }
      iw->loadData(data, mtxd.size[1], mtxd.size[0]);
    }	   
  
  iw->zunit.scale = (mapmax-mapmin)/65535;
  iw->zunit.offset = -mapmin/iw->zunit.scale;

  adjustCenterPeak();
}


void adjustCenterPeak()
{
  // We will set the center of the mapping function to the peak in the
  // histogram. The width of the histogram will be set as the larger
  // of the distance to the lowest black point or white point.

  int datamin, datamax, datapeak;
  int nblack = 0; int nwhite = 0;
  int halfwidth;

  for (datamax = LMAX; datamax >=0; datamax--)
    {
      nwhite += iw->datahist[datamax];
      if (nwhite > iw->wpercent*iw->w*iw->h/100) break;
    }
  for (datamin = 0; datamin <= LMAX; datamin++)
    {
      nblack += iw->datahist[datamin];
      if (nblack > iw->wpercent*iw->w*iw->h/100) break;
    }
  datapeak = datamin;
  for (int i = datamin; i <= datamax; i++)
    {
      if (iw->datahist[i] > iw->datahist[datapeak])
	datapeak = i;
    }

  halfwidth = ((datamax-datapeak) > (datapeak-datamin)) ? (datamax-datapeak) : (datapeak-datamin);
  datapeak = datapeak - halfwidth/128;
  iw->setMax(datapeak+halfwidth);
  iw->setMin(datapeak-halfwidth);

  iw->adjustHistogram();
}
