#ifndef SPYNAVIGATE_H
#define SPYNAVIGATE_H

int keyhandler(int event);
void save_files();
void update();
void load_file();
void load_mtx_cut();
void read_mtx();
void adjustCenterPeak();

void cmapch_cb(Fl_Widget *, void *);

struct mtx
{
  double *data;
  int size[3];
  double datamax;
  double datamin;
};

extern struct mtx mtxd;

#endif
