#include "spyrotate_ui.h"
#include <math.h>
#include <stdio.h>
#include <FL/fl_ask.H>
#include <FL/filename.H>

extern "C"  {
#include <pgm.h>
}

// Data arrays

int *origdata1;
int *origdata2;
int *data1;
int *data2;
int wid, hgt;

char *fname1, *fname2;

int nscans;
int scannum;
char **arglist;

int keyhandler(int event);
void save_files();

void usage()
{
  fprintf(stderr, "spyrotate scan1x scan1y scan2x scan2y ...");
  exit(0);
}  

int main(int argc, char **argv)
{
  arglist = argv;

  if (argc < 2)
    usage();

  Fl::visual(FL_RGB);
  make_window();
  iw1->setGamma(1.0,0.0);
  iw2->setGamma(1.0,0.0);

  nscans = (argc-1)/2;
  scannum = 1;

  load_data();

  controls->show();
  Fl::add_handler(keyhandler);
  iw1->show();
  iw2->show();
  
  Fl::run();
}


void load_data()
{
  //I'm lazy, so I'll use ImageWindow's loadData instead of directly
  //using Aaron's Image2D class. So much unneccessary memory
  //allocation and deallocation...
  
  int n1 = scannum*2-1;
  int n2 = scannum*2;
  
  iw1->loadData(arglist[n1]);
  iw2->loadData(arglist[n2]);

  fname1 = arglist[n1];
  fname2 = arglist[n2];

  iw1->label(arglist[n1]);
  iw2->label(arglist[n2]); 

  if ((iw1->w != iw2->w) || (iw1->h != iw2->h))
    {
      fprintf(stderr, "Images %s and %s are not the same size!\n", arglist[n1], arglist[n2]);
      exit(-1);
    }

  wid = iw1->w;
  hgt = iw1->h;

  origdata1 = new int[wid*hgt];
  origdata2 = new int[wid*hgt];
  data1 = new int[wid*hgt];
  data2 = new int[wid*hgt];
  
  for (int i=0; i<wid*hgt; i++)
    {
      origdata1[i] = iw1->data[i];
      origdata2[i] = iw2->data[i];
    }
  
  update_images();
}

void update_images()
{
  double angle = angle_value->value() / 180 * M_PI;
  double d1, d2, d1r, d2r;

  for (int i=0; i<wid*hgt; i++)
    {
      	d1 = (double) origdata1[i] - (double)LMAX / 2.0;
	d2 = (double) origdata2[i] - (double)LMAX / 2.0;
	d1r =  d1*cos(angle) + d2*sin(angle);
	d2r = -d1*sin(angle) + d2*cos(angle);
	data1[i] = (int) (round(d1r) + (double)LMAX/2.0);
	data2[i] = (int) (round(d2r) + (double)LMAX/2.0);
    }
  
  iw1->loadData(data1, wid, hgt);
  iw2->loadData(data2, wid, hgt);
  iw1->normalize();
  iw2->normalize();
}

int keyhandler(int event)
{
  int key;
  switch (event)
    {
    case FL_SHORTCUT:
      key = Fl::event_key();
      if (key == 's')
	{
	  save_files();
	  return 1;
	}
      if (key == 'q')
	exit(0);
    }
  return 0;
}

void save_files()
{
  char fn1[255], fn2[255];
  FILE *f1, *f2;
  gray **image1, **image2;
  
  image1 = pgm_allocarray(wid, hgt);
  image2 = pgm_allocarray(wid, hgt);
  

  for (int i=0; i<wid*hgt; i++)
    {
      image1[i/wid][i%wid] = (gray) data1[i];
      image2[i/wid][i%wid] = (gray) data2[i];
    }

  snprintf(fn1, 256, "%s_%+.1f_.pgm", fname1, angle_value->value());
  snprintf(fn2, 256, "%s_%+.1f_.pgm", fname2, angle_value->value());
  
  f1 = fopen(fn1, "w");
  f2 = fopen(fn2, "w");

  pgm_writepgm(f1, image1, wid, hgt, 65535, false);
  pgm_writepgm(f2, image2, wid, hgt, 65535, false);

  fclose(f1);
  fclose(f2);

  pgm_freearray(image1, hgt);
  pgm_freearray(image2, hgt);

  fl_beep(FL_BEEP_MESSAGE);
}
  
