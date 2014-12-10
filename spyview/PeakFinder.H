#ifndef __peakfinder_h__
#define __peakfinder_h__

#include <unistd.h>
#include <math.h>
#include "message.h"

#include "PeakFinder_Control.h"
#include "ImageWindow.H"

class PeakFinder
{
public:
  double threashold;
  bool recalculate;

  int w;
  int h;

  //This will be an array of the same size as the data, contaning a 1
  //if a datapoint is a peak, a -1 if it is a valley, and 0 if it is
  //neither.
  char *peaks;
  
  PeakFinder_Control *pfc;
  ImageWindow *iw;

  PeakFinder(ImageWindow *p_iw, PeakFinder_Control *p_pfc);
  ~PeakFinder() { clear(); };

  void calculate();
  void save_peaks();
  void remove_outliers();

  inline void clear() 
  {
    if (peaks != NULL)
      {
	delete [] peaks;
	peaks = NULL;
      }
  }

};


#endif
