#include "PeakFinder.H"
#include <stdio.h>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/fl_ask.H>

PeakFinder::PeakFinder(ImageWindow *p_iw, PeakFinder_Control *p_pfc)
{
  iw = p_iw;
  pfc = p_pfc;

  recalculate = true;
  peaks = NULL;

  // We will get the default values from the widgets
  // ie. change default values using fluid

  threashold = pfc->thresh->value();

  pfc->outlier_x->minimum(0);
  pfc->outlier_x->maximum(100);

  pfc->outlier_y->minimum(0);
  pfc->outlier_y->maximum(100);
  
  pfc->outlier_num->minimum(1);
  pfc->outlier_num->maximum(20);

  pfc->peak_color_box->color(fl_rgb_color(0,255,0));
  pfc->valley_color_box->color(fl_rgb_color(255,255,0));

  pfc->helptext->wrap(1);
  pfc->helptext->readonly(1);
  pfc->helptext->value("The peak detection works by looking for peaks in each row of the data. "
		       "If you want to find peaks in the columns, rotate the data by 90 degrees."
		       "Note that it will work best if your data is oriented with the sweep "
		       "variable along the horizontal axis.\n\n"
		       "The threashold is used for preventing noise in your data from giving " 
		       "false peaks. It should be set to a value a bit bigger than the value of "
		       "the noise. Peaks (and valleys) of height smaller that the threshold will not be "
		       "detected.\n\n"
		       "Peaks are detected in the following way:\n\n"
		       "(1) Look for a maximum followed by a drop of data value by at least threashold\n\n"
		       "(2) Now search for a minimum followed by increas of at least threashold\n\n"
		       "(3) Go back to (1)\n\n"
		       "The algorithm is from the matlab function \"peakdet\" from "
		       "http://www.billauer.co.il/peakdet.html\n\n"
		       "The user can also set a \"maximum valley data value\" that will ignore "
		       "identification of valley based on the data value. This is useful to avoid "
		       "picking out valleys that are just the zero background between two peaks, for example. "
		       "(There is also a similar \"minimum peak data value\" setting.)\n\n"
		       );
		  
}

void PeakFinder::calculate()
{
  // Copied from "peakdet" matlab function from
  // http://www.billauer.co.il/peakdet.html

  if (!recalculate) return; 

  clear();

  // Note: we should use iw->id values instead of iw values since we
  // are called from runQueue and the iw w and h may not be updated
  // yet.

  w = iw->id.width;
  h = iw->id.height;
  peaks = new char [w*h];

  double val;

  bool lookformax = true;
  int maxpos, minpos;
  double max, min;

  for (int j=0; j<h; j++)
    {
      min = INFINITY;
      max = -INFINITY;
      for (int i=0; i<w; i++)
	{
	  peaks[j*w+i] = 0;
	  val = iw->id.raw(i,j);
	  
	  if (val > max) 
	    { max = val; maxpos = i; }
	  if (val < min)
	    { min = val; minpos = i;}
	
	  if (lookformax) 
	    {
	      if (val < max - threashold)
		{
		  // We've now past the peak, and the last max was the real
		  // peak position and value
		  if (maxpos != 0)
		    if (!pfc->pmin_enable->value() || max > pfc->peak_min->value())
		      peaks[j*w+maxpos] = 1;
		  min = val;
		  minpos = i;
		  lookformax = false;
		}
	    }
	  else
	    {
	      if (val > min + threashold)
		{
		  if (minpos != 0)
		    if (!pfc->vmax_enable->value() || min < pfc->valley_max->value())
		      peaks[j*w+minpos] = -1;
		  max = val;
		  maxpos = i;
		  lookformax = true;
		}
	    }
	}
    }

  if (pfc->do_outliers->value())
    remove_outliers();
}

void PeakFinder::remove_outliers()
{
  int dx = pfc->outlier_x->value();
  int dy = pfc->outlier_y->value();
  int nearby; 

  int npout = 0; 
  int nvout = 0; 

  for (int j=0; j<h; j++)
    {
      for (int i=0; i<w; i++)
	{
	  if (peaks[j*w+i] == 1)
	    {
	      nearby = 0;
	      for (int m=-dx; m<=dx; m++)
		for (int n = -dy; n<= dy; n++)
		  if ((i-m)<=0 && (i+m)<w && (j-n)<=0 && (j+n)<h)
		    if (peaks[(j+n)*w+i+m] == 1) nearby++;
	      nearby--; // we find always at least one...
	      if (nearby < pfc->outlier_num->value())
		{
		  peaks[j*w+i] = 0;
		  npout++;
		}
	    }
	  if (peaks[j*w+i] == -1)
	    {
	      nearby = 0;
	      for (int m=-dx; m<=dx; m++)
		for (int n = -dy; n<= dy; n++)
		  if ((i-m)<=0 && (i+m)<w && (j-n)<=0 && (j+n)<h)
		    if (peaks[(j+n)*w+i+m] == -1) nearby++;
	      nearby--; // we find always at least one...
	      if (nearby < pfc->outlier_num->value())
		{
		  peaks[j*w+i] = 0;
		  nvout++;
		}
	    }

	}
    }

  info("removed %d outlier peaks and %d outlier valleys\n", npout, nvout);

}

void PeakFinder::save_peaks()
{
  string peak_name = iw->output_basename;
  string valley_name = iw->output_basename;
  string peak_name2 = iw->output_basename;
  string valley_name2 = iw->output_basename;
  peak_name += ".peaks.dat";
  valley_name += ".valleys.dat";
  peak_name2 += ".peaks.index.dat";
  valley_name2 += ".valleys.index.dat";

  FILE *fp1 = fopen(peak_name.c_str(), "w");
  FILE *fp2 = fopen(valley_name.c_str(), "w");
  FILE *fp3 = fopen(peak_name2.c_str(), "w");
  FILE *fp4 = fopen(valley_name2.c_str(), "w");

  fprintf(fp1, 
	  "# Basename: %s\n"
	  "# Threshold: %e\n"
	  "# X name: %s\n"
	  "# Y name: %s\n"
	  "# Zname: %s\n",
	  iw->output_basename, threashold,
	  iw->id.xname.c_str(), iw->id.xname.c_str(), iw->id.zname.c_str());

  fprintf(fp2, 
	  "# Basename: %s\n"
	  "# Threshold: %e\n"
	  "# X name: %s\n"
	  "# Y name: %s\n"
	  "# Zname: %s\n",
	  iw->output_basename, threashold, 
	  iw->id.xname.c_str(), iw->id.xname.c_str(), iw->id.zname.c_str());
	  
  double last_peak, last_valley;
  bool first_peak, first_valley;
  for (int j=0; j<iw->h; j++)
    {
      fprintf(fp1, "%e ", iw->id.getY(j));
      fprintf(fp2, "%e ", iw->id.getY(j));
      first_peak = first_valley = true;
      for (int i=0; i<iw->w; i++)
	{
	  if (peaks[j*iw->w+i] == 1)
	    {
	      fprintf(fp1, "%e ", iw->id.getX(i));
	      fprintf(fp3, "%e %e %e ", iw->id.getY(j), iw->id.getX(i), iw->id.raw(i,j));
	      if (!first_peak)
		  fprintf(fp3, "%e", iw->id.getX(i) - last_peak);
	      first_peak = false;
	      fprintf(fp3, "\n");
	      last_peak = iw->id.getX(i);
	    }
	  if (peaks[j*iw->w+i] == -1)
	    {
	      fprintf(fp2, "%e ", iw->id.getX(i));
	      fprintf(fp4, "%e %e %e ", iw->id.getY(j), iw->id.getX(i), iw->id.raw(i,j));
	      if (!first_valley)
		fprintf(fp4, "%e", iw->id.getX(i) - last_valley);
	      first_valley = false;
	      fprintf(fp4, "\n");
	      last_valley = iw->id.getX(i);
	    }
	}
      fprintf(fp1, "\n");
      fprintf(fp2, "\n");
      fprintf(fp3, "\n\n");
      fprintf(fp4, "\n\n");
    }
  fclose(fp1);
  fclose(fp2);
  fclose(fp3);
  fclose(fp4);

  fl_message("Data saved to files\n%s\n%s\n%s\n%s", 
	     peak_name.c_str(), valley_name.c_str(),
	     peak_name2.c_str(), valley_name2.c_str());

  // Let's also make a file for a gnuplot overlay
  // To do this, we need to output a ".rgb" file
  // and then plot it with the proper rotation, etc, in gnuplot

  string name;
  FILE *fp;
  string base = iw->output_basename;

  fp = fopen((base+".rgb").c_str(), "wb");
  unsigned char r,g,b;
#if 0
  for (int i = 0; i<iw->w*iw->h; i++)
    {
      iw->makergb(iw->data[i],r,g,b,i/iw->w,i%iw->w);
      fwrite(&r, 1, 1, fp);
      fwrite(&g, 1, 1, fp);
      fwrite(&b, 1, 1, fp);
    }
#else
  for (int i=0; i<iw->w; i++)
  //for (int i=iw->w-1; i>=0; i--)
    //for (int j=0; j<iw->h; j++)
    for (int j=iw->h-1; j>=0; j--)
      {
 	iw->getrgb(j,i,r,g,b);
 	fwrite(&r, 1, 1, fp);
 	fwrite(&g, 1, 1, fp);
 	fwrite(&b, 1, 1, fp);
       }
#endif
  fclose(fp);
  
  double xmin = iw->id.getX(0);
  double xmax = iw->id.getX(iw->w-1);
  double ymin = iw->id.getY(iw->h-1);
  double ymax = iw->id.getY(0);

  // how confusing is this...too bad gnuplot image can't handle a negative dx or dy...
  double origin_y = xmin;
  double origin_x = ymin;
  string flip = "";
  if (xmax < xmin) // extra confusing!!!! 
    {
      origin_y = xmax;
      flip += " flipy ";
    }
  if (ymax < ymin) 
    {
      origin_x = ymax;
      flip += " flipx ";
    }

  fp = fopen((base+".overlay.gnu").c_str(), "w");
  fprintf(fp, "set yrange [%e:%e]\n "
	  "set xrange [%e:%e]\n"
	  "unset key\n", xmin, xmax, ymin, ymax);
  fprintf(fp, 
	  "plot '%s' "
	  "binary format='%%uchar' "
	  "array=(%d,%d) "
	  "origin=(%e,%e) "
	  "dx=%e "
	  "dy=%e "
	  "%s "
	  "with rgbimage\n", 
	  (base+".rgb").c_str(), 
	  iw->h, iw->w,
	  origin_x, origin_y,
	  fabs((iw->id.getY(h-1)-iw->id.getY(0))/iw->h),
	  fabs((iw->id.getX(w-1)-iw->id.getX(0))/iw->w), 
	  flip.c_str());
  fprintf(fp, "replot '%s' with points, '%s' with points",
	  (base+".peaks.index.dat").c_str(), 
	  (base+".valleys.index.dat").c_str());
  fclose(fp);

}

