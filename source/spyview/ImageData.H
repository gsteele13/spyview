#ifndef ImageData_H
#define ImageData_H

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Output.H>

#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <math.h>
#include "message.h"

#define QUANT_MAX 65535

// Checking array limits will help debugging but will increase the
// execution time. I have benchmarked this by calling load_file() on a
// 1x801x601 mtx (which load the YZ cut by default), and then
// reloading the YZ cut. Without limit checking, the excution time as
// 0.299s. With limit checking, it was 0.370s.

#define CHECK_ARRAY_LIMITS 0

using std::string;
using std::vector;

typedef enum { YZ = 0, XY = 2, XZ = 1 }  mtxcut_t;
typedef enum { COLUMNS = 0, INDEX = 1}  gpload_t;
typedef enum { DROP = 'd', FILL_MEAN = 'm', FILL_ZERO = 'z', EXIT = 'x', COPY_ADJACENT = 'a'} incorrect_column_t;
typedef enum { MATRIX = 0, GNUPLOT = 1, DELFT_LEGACY = 2, DAT_META = 3}  datfile_t;
typedef enum { PGM = 0, MTX = 1, DAT = 2}  fallbackfile_t;

class MTX_Data
{

public:

  string filename; 
  double *data;
  string axisname[3];
  string dataname;
  int size[3];
  double axismin[3];  // the ranges for the x, y, and z axes
  double axismax[3];  // the ranges for the x, y, and z axes
  bool data_loaded;
  bool data3d;
  bool parse_txt;
  bool delft_raw_units;
  bool progress_gui;
  bool delft_settings;

  string settings;

  MTX_Data();
  ~MTX_Data();
  
  int load_file(const char *name);
  int load_gp_index(const char *name, int col);
  int load_gp_cols(const char *name);
  int load_dat_meta(const char *name, int col);
  void parse_delft_txt(const char *name, bool columns);
  void parse_comments(char *comments, const char *ident, int *dac, int *size, char *name, double *range);
  void parse_var2_comments(char *comments, const char *ident, int *dac, char *name, double *range);
  void get_settings(const char *name);
  
  inline double get_coordinate(int axis, int i)
  { 
    axis = axis%3;
    if (size[axis] == 1)
      return (axismin[axis]+axismax[axis])/2.0;
    else
      return axismin[axis] + 1.0*i*(axismax[axis]-axismin[axis])/(size[axis]-1); 
      //return axismin[axis] + 1.0*i*(axismax[axis]-axismin[axis])/(size[axis]); // why not minus one? 
  };

  inline double &getData(int i, int j, int k) 
  {
#if CHECK_ARRAY_LIMITS
    if (!data_loaded)
      { fprintf(stderr, "MTX data not yet loaded!\n"); exit(-1);}
    if (i>=size[0] || i < 0)
      { fprintf(stderr, "attempt to access i = %d, size[0] = %d\n", i, size[0]); exit(-1);}
    if (j>=size[1] || j < 0)
      { fprintf(stderr, "attempt to access j = %d, size[1] = %d\n", j, size[1]); exit(-1);}
    if (k>=size[2] || k < 0)
      { fprintf(stderr, "attempt to access k = %d, size[2] = %d\n", k, size[2]); exit(-1);}
#endif 
    return data[k*size[1]*size[0] + j*size[0] + i]; 
  };
  void clear() { if (data_loaded) delete [] data; data_loaded = 0;};

  void open_progress_gui();
  void close_progress_gui();

  Fl_Double_Window *win;
  Fl_Output *msg;
  
private: 
};

class ImageData 
{

public:

  double *orig_data;
  double *raw_data;
  int *quant_data;
  int width, height;
  bool *threshold_reject;

  MTX_Data mtx;

  // Keep track of which mtx index and gp column are loaded
  mtxcut_t mtx_cut_type;
  int mtx_index; 
  bool do_mtx_cut_title; 
  gpload_t gpload_type; 
  int gp_column;

  bool auto_quant; 
  double auto_quant_percent;

  bool data_loaded;  
  bool data3d;

  datfile_t datfile_type;
  fallbackfile_t fallbackfile_type;


  double rawmin, rawmax;  // the actual min and max of the raw dataset
  double qmin, qmax; // the raw values corresponding to 0 and QUANT_MAX
  double xmin, xmax; // the values of i=0 and i=width-1
  double ymin, ymax; // the values of j=0 and j=height-1
  
  string xname;
  string yname;
  string zname;

  // We need to keep track of the orignal stuff for the reset() function
  int orig_width, orig_height;
  double orig_xmin;
  double orig_xmax;
  double orig_ymin;
  double orig_ymax;
  string orig_xname;
  string orig_yname;

  // Action on incorrect number of columns when reading DAT files
  incorrect_column_t incorrect_column;

  // Constructor will not do much by default
  ImageData();
  ~ImageData();

  // Functions for loading new data

  // Copy data from an existing fp matrix 
  void load_raw(double *data, 
		int w, int h,
		double x1=NAN, double x2=NAN, 
		double y1=NAN, double y2=NAN);

  // Copy data from an existing int matrix
  void load_int(int *data, 
		int w, int h, 
		double x1=NAN, double x2=NAN, 
		double y1=NAN, double y2=NAN, 
		double z1=NAN, double z2=NAN);
  // Reset the data back to the original (for reprocesing data without
  // reloading from file)
  void reset();

  // Memory allocation
  void clear();
  // reallocated space for arrays according to current width and height
  // this is used when we load new data (will clear all arrays)
  void reallocate(); 

  // So far, we've tried to keep memory allocation to a minimum.  This
  // has worked so far because all of the image processing stuff that
  // changes the size of the datasets _shrink_ the dataset.  However,
  // with the introduction of the interpolate function, we'll also
  // need to make the storage arrays (like quant_data,
  // threshold_reject) bigger. This is what this function does (note
  // that it will never reallocate them smaller since we need to be
  // able to restore the original data). We will leave the work of
  // allocating raw_data to the image processing function.
  void resize_tmp_arrays(int new_width, int new_height); 


  // Many different functions will support threasholding Using this
  // function, and the threshold_reject array, we will unify all of
  // these to keep them consistent. It will also simplify the other
  // code.

  // Type: 0 = whole image, 1 = line, 2 = column, 3 = data value
  void calculate_thresholds(int type, double low, double high, 
			    double bottom_limit=-INFINITY, double top_limit=INFINITY);
  
  // Generic Load from file (determine filetype from extension)
  // Programs using the this class can either use this function
  // directly, but can also use the helper function directly
  // 
  // All load functions will return -1 if the file was not succesfully
  // loaded.
  int load_file(const char *name);

  // Helpers to load data from specific files
  int load_STM(const char *name);
  int load_PGM(const char *name);
  int load_DAT(const char *name);
  int load_XL30S_TIF(const char *name); // hacked to read TIF files from XL30S
  int load_Delft(const char *name);
  // For the following functions, user should set the desired
  // mtx_cut_type, mtx_index, gpload_type and gp_column. If they
  // change during load (for example if they are out of range), then
  // the variables in the class will store the correct value. 
  // Alternatively, you can use the inline functions below
  int load_MTX(const char *name);
  int load_GP(const char *name);
  // A new function that loads data from a "columns" formatted data
  // file with the help of a new metadata file. The metadata file will
  // be called "file.meta.txt". It will use gp_column number to pick
  // the column. It will load 3D data into an MTX. 
  int load_DAT_meta(const char *name);
  void load_mtx_cut();
  void saveMTX(const char *name); // Write the current image data to an MTX file

  // Some handy functions:

  inline int limit(int A) { if (A<0) return 0; else if (A>QUANT_MAX) return QUANT_MAX; else return A; };

  inline int raw_to_quant(double raw) {return limit((int)round(1.0*(raw-qmin)*QUANT_MAX/(qmax-qmin)));}
  inline double quant_to_raw(double quant) {return qmin + 1.0*quant*(qmax-qmin)/QUANT_MAX;};

  // Given a pixel coordinate x, return the united coordinate.
  inline double getX(double i) {return xmin + ((width==1) ? 0 : 1.0*i*(xmax-xmin)/(width-1));};
  inline double getY(double j) {return ymin + ((height==1) ? 0 : 1.0*(height-1-j)*(ymax-ymin)/(height-1));}; // ymax is flipped

  // Given a united coordinate x, return the un-united.
  inline double getX_inv(double x, double zoom=1.0) { return (width==1) ? 0 : (zoom*(x-xmin)/(xmax-xmin)*(width-1)); }
  inline double getY_inv(double y, double zoom=1.0) { return (height==1) ? 0 : zoom*(height-1-(y-ymin)/(ymax-ymin)*(height-1)); }
  inline double getX_inv(double x, int zoom) { return (width==1) ? 0 : ((zoom > 0 ? zoom : -1.0/zoom)*(x-xmin)/(xmax-xmin)*(width-1)); }
  inline double getY_inv(double y, int zoom) { return (height==1) ? 0 : (zoom > 0 ? zoom : -1.0/zoom)*(height-1-(y-ymin)/(ymax-ymin)*(height-1)); }

  // This will rescale the floating point making the old qmin and qmax
  // into the new qmin and qmax. This is the equivalent of changing
  // the zunit settings in the old setup.

  void rescale_data(double new_qmin, double new_qmax); 

  // More convenient functions

  inline void load_mtx_cut(int index) 
  {mtx_index = index; load_mtx_cut();};
  inline void load_mtx_cut(int index, mtxcut_t type) 
  {mtx_index = index; mtx_cut_type = type; load_mtx_cut();};

  inline void load_GP_cols(const char *name) 
  {gpload_type = COLUMNS; load_GP(name);}
  inline void load_GP_index(const char *name, int colnum) 
  {gpload_type = INDEX; gp_column = colnum; load_GP(name);}
  

  // Copy original data back into raw data matrix (saves reloading from file)
  inline void reload_orig() { memcpy(raw_data, orig_data, width*height*sizeof(double)); }

  // Quantizing the data
  void quantize();

  // Image Processing Operations on data:

  // Even simpler (inline) functions

  inline void scale_axes(double xscale, double yscale)
  {
    //info("scale %e %e %e %e %e %e\n", xmin, xmax, ymin, ymax, xscale, yscale);
    xmin *= xscale;
    xmax *= xscale;
    ymin *= yscale;
    ymax *= yscale;
    //info("scale %e %e %e %e\n", xmin, xmax, ymin, ymax);
  }

  inline void offset_axes(double xoff, double yoff)
  {
    info("off %e %e %e %e %e %e\n", xmin, xmax, ymin, ymax, xoff, yoff);
    xmin += xoff;
    xmax += xoff;
    ymin += yoff;
    ymax += yoff;
    info("off %e %e %e %e\n", xmin, xmax, ymin, ymax);
  }

  // Very simple functions (one liners)
  
  void log10(bool do_offset, double new_offset);// data[i] = log10(data[i]) for all i  
  void magnitude();// data[i] = abs(data[i]) for all i, !using z unit!
  void neg();// data[i] = -(data[i]) for all i, !using z unit!
  void offset(double offset, bool do_auto);// add an offset
  void scale(double factor); // scale the data
  void gamma(double gamma, double epsilon); // gamma scaling operation on 16 bit data
  void power2(double x); // y = x^data

  // More complicated functions that preserve the images size

  void lbl(double bp=0.0, double tp=0.0, bool whole_image_threashold=false, bool percentiles=true, 
	   double bottom_limit = -INFINITY, double top_limit = INFINITY); // subtract line average from each line
  void cbc(double bp=0.0, double tp=0.0, bool whole_image_threashold=false, bool percentiles=true,
	   double bottom_limit = -INFINITY, double top_limit = INFINITY); // subtract column average from each column
  void sub_linecut(bool horizontal, int pos); // Subtract a specific line form each line.  
  void outlier_line(bool horizontal, int pos); // Remove an outlier line
  void norm_lbl(); // Normalize each line individually.  
  void norm_cbc(); // Normalize each line individually.
  void fitplane(double bp=0, double tp=0, bool percentiles=true); // subtract a plane
  void plane(double b, double a); // subtract a plane
  void xflip();   
  void yflip();
  void flip_endpoints(bool flipx, bool flipy);
  void rotate_cw();
  void rotate_ccw();
  void grad_mag(double bias); // Replace each pixel with the magnitude of the gradient.  0<bias<1 biases one derv. as more important than the other.
  void dderv(double theta); // Derivative along axis theta *degrees* off the x-axis.
  void shift_data(int after_row, int offset); //designed for gate shifts

  // look for "switches": Run across each row of the data: if the data
  // changes by more than "threshold" in one pixel, then we consider
  // this a switch. We the calculate an average over "avgwin" pixels
  // before the switch, we add the difference to a running offset
  // value that we add to all subsequent datapoints in the row.
  void switch_finder(double threshold, int avgwin, bool vert); 


  typedef enum { LOWPASS_GAUSS = 0, LOWPASS_LORENTZ = 1, LOWPASS_EXP = 2 , LOWPASS_THERMAL} lowpass_kernel_t;  void make_lowpass_kernel(double *data, double sigma, int size, lowpass_kernel_t type=LOWPASS_GAUSS);
  void lowpass(double xsize, double ysize, lowpass_kernel_t type, double mult=7.0); // width is width of mask to use measured in units of xsize, ysize. FWHM is 2.35, so around 6 works well.
  void highpass(double xsize, double ysize, double passthrough=0.0,lowpass_kernel_t type=LOWPASS_GAUSS, double mult=7.0); // mult is width of mask to use measured in units of xsize, ysize
  void notch(double xlow, double xhigh, double ylow, double yhigh, double width=7.0); // width is width of mask to use measured in units of xsize, ysize
  void equalize();  // Perform histogramic equalization

  void despeckle(bool despeckle_x, bool despeckle_y);

  void crop(int left, int right, int lower, int upper);

  // Functions that change the image size. These are going to be tricky
  // to deal with, especially since we also want to keep the original 
  // data around!

  // These functions will not shrink the size of the allocated data
  // storage matrices as we want to be able to use reset() to reload
  // the original data using memcpy. They may change the values of
  // width and height, so that raw(i,j) retrieves the correct values

  void pixel_average(int nx, int ny); // average adjacent pixels to downsample data
  void xderv(); // take a horizontal derivative
  void yderv(); // take a vertical derivative
  void ederv(double pscale, double nscale); // take a vertical derivative, with sign flipped at y=0
  void even_odd(bool even, bool fwd_rev); // extract either even or odd rows
  void remove_lines(int start, int nlines); // remove lines, vertically shifting the data and adjusting the ymax
  void interpolate(int new_width, int new_height, int type=0); // regrid the data, using bilinear interpolation (maybe bicubic in the future)
  inline void scale_image(double xs, double ys) // an obvious next step...
  { interpolate(width*xs, height*ys); };

  void hist2d(double dmin, double dmax, int num_bins); //Transform the y-axis data in to a 2D histogram of datavalues

  void vi_to_iv(double vmin, double vmax, int num_bins); //Transform an VI measurement into an IV measurement

  // Functions for accessing the data with an (i,j) syntax (like
  // Oliver's pixel() function in Image2D) (what does the second one
  // with the const do?)  

  // I've also added array bounds checking, which may slow it down (?)
  // but should help catch loads of bugs.

  inline double &raw(int i, int j) 
  {
 #if CHECK_ARRAY_LIMITS
    if (!data_loaded) 
      {fprintf(stderr, "Data not loaded!\n"); exit(-1);}
    if (i >= width || i < 0)
      {fprintf(stderr, "attempt to acces i = %d, width = %d\n", i, width); return raw_data[0];}
    if (j >= height || j < 0)
      {fprintf(stderr, "attempt to acces j = %d, height = %d\n", i, height); return raw_data[0];}
#endif
    return raw_data[j*width+i];
  }; 

  inline const double &raw(int i, int j) const 
  {
 #if CHECK_ARRAY_LIMITS
    if (!data_loaded) 
      {fprintf(stderr, "Data not loaded!\n"); exit(-1);}
    if (i >= width || i < 0)
      {fprintf(stderr, "attempt to acces i = %d, width = %d\n", i, width); exit(-1);}
    if (j >= height || j < 0)
      {fprintf(stderr, "attempt to acces j = %d, height = %d\n", i, height); exit(-1);}
#endif
    return raw_data[j*width+i];
  }; 

  inline int &quant(int i, int j) 
  {
 #if CHECK_ARRAY_LIMITS
    if (!data_loaded) 
      {fprintf(stderr, "Data not loaded!\n"); exit(-1);}
    if (i >= width || i < 0)
      {fprintf(stderr, "attempt to acces i = %d, width = %d\n", i, width); exit(-1);}
    if (j >= height || j < 0)
      {fprintf(stderr, "attempt to acces j = %d, height = %d\n", i, height); exit(-1);}
#endif
    return quant_data[j*width+i];
  }; 


  // How cool an idea is this?
  //
  // Eventually, use bicubic interpolation for the pixels in the middle:
  //
  // http://www.paulinternet.nl/?page=bicubic
  //
  // but then bilinear interpolation for ones at the edges. 
  //
  // Start by just using bilinear everywhere...the add the bicubic later. (maybe)

  inline double raw_interp(double x, double y)
  {
    int i = (int)floor(x);
    int j = (int)floor(y);

    if (i<0 || i > width-1 || j < 0 || j > height-1)
      {
	info("interp outside dataset: %e (%d) %e (%d)\n", x, width-1, y, height-1);
	return NAN;
      }
    
    double dx = x-(double)i;
    double dy = y-(double)j;

    // The boundary corner
    if (i == width-1 && j == height-1)
      return raw(i,j);
    
    // The right edge
    if (i == width-1)
      return raw(i,j) + (raw(i,j+1)-raw(i,j))*dy;
    
    // The bottom edge
    if (j == height-1)
      return raw(i,j) + (raw(i+1,j)-raw(i,j))*dx;

    
    // The rest (from wikipedia)
    return 
      ((raw(i,j)) + //b1
      (raw(i+1,j)-raw(i,j))*dx + // b2*x
      (raw(i,j+1)-raw(i,j))*dy + // b3*y
      (raw(i,j)-raw(i+1,j)-raw(i,j+1)+raw(i+1,j+1))*dx*dy); // b4*x*y
  };
    
private:

  // Some internal functions
  int parse_pgm_comments(const char *ident, const char *fmt, void *val, char *p, const char *filename);

  inline int check_loaded() {
  if (data_loaded == 0)
    {
      fprintf(stderr, "data accessed before loading! exiting...\n");
      exit(-1);
    }
  }    
  
  void find_raw_limits(); // find the min/max of the raw data: we 

  void find_threasholds(vector <double> data, 
			double bottom_percent, double top_percent,
			double &low, double &high);
  void find_image_threasholds(double bottom_percent, double top_percent,
			      double &low, double &high);
  void store_orig();
};


#endif
