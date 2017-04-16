#ifndef ImageWindow_H
#define ImageWindow_H
#include <unistd.h>
#include <config.h>
#include <FL/Fl_Box.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Overlay_Window.H>
#include <FL/Fl_Browser.H>
#include <FL/fl_draw.H>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <vector>
#include <string>
#include <string.h>
#include <sstream>  //?? might not be used
#include "ImagePrinter.H"
#include "ImagePrinter_Control.h"
#include "PeakFinder.H"
#include "PeakFinder_Control.h"
#include "ImageData.H"
#include "myboost.h"
#include "Gnuplot_Interface.H"

// Define this to disable gnuplot threads so we can use gdb
//#define DEBUG

#define NOLINE 1
#define HORZLINE 2
#define VERTLINE 4
#define OTHERLINE 8

using std::string;

typedef enum { XAXIS, YAXIS, DISTANCE } lc_axis_t;
typedef enum { KEEPZOOM, KEEPSIZE, RESETZOOM } window_size_action_t;

#define LMAX USHRT_MAX

class ZoomWindow;

class ColormapWindow;
class Image_Operation;
class PeakFinder;
class ImageWindow_Module;

class ImageWindow : public Fl_Overlay_Window
{
public:
  friend class ZoomWindow;
  friend class ColormapWindow;
  friend class Image_Printer;
  friend class ImageData;
  friend class PeakFinder;

  ImageWindow(int w, int h, const char *title = 0);
  virtual ~ImageWindow();

  void runQueue(); // Run all the filters in the queue
  void reRunQueue(); // Rerun the image process queue
    
  void draw_overlay();
  void draw_overlay_pixel(int i, int j, Fl_Color color);
  void draw_overlay_pixel(int i, int j, uchar r, uchar g, uchar b);
  void resize(int x, int y, int w, int h);

  // External modules that extend the functionality of spyview.
  typedef std::list<ImageWindow_Module *> modules_t;
  modules_t modules;
  void registerModule(ImageWindow_Module *m) { modules.push_back(m); };
    
  void adjustHistogram();
  void calculateHistogram();
  void normalize();
  void normalizeOnZoom();

  // we can either load directly the data from file using this function
  int loadData(const char *name);
  // or we can just pass the class a matrix of ints
  void loadData(int *newdata, int neww, int newh, const char *name="out", bool reset_units=1);

  void load_mtx_cut(int index, mtxcut_t type);

  inline int dozoom(int x, int A) {return ((A>0) ? A*x : x/(-A));};
  inline int dounzoom(int x, int A) {return ((A>0) ? x/A : x*(-A));};
  inline double dozoom(double x, int A) {return ((A>0) ? A*x : x/(-A));};
  inline double dounzoom(double x, int A) {return ((A>0) ? x/A : x*(-A));};

  // Functions to translate mouse events into x/y positions for the
  // ImageWindow.  These will unzoom the event position and do bounds
  // checking. (Sometimes event position is outside window, which can
  // cause a segfault.)
  inline int get_event_x() 
  {
    int x = dounzoom(Fl::event_x(), xzoom);
    if (x<0) x = 0;
    if (x>w-1) x = w-1;
    return x;
  };
  inline int get_event_y()
  {
    int y = dounzoom(Fl::event_y(), yzoom);
    if (y<0) y = 0;
    if (y>h-1) y = h-1;
    return y;
  };

  // Same as above, but provide sub-pixel precision when zoomed.  Important so lines being dragged
  // don't "snap" around.
  inline double get_event_x_d() 
  {
    double x = dounzoom((double)Fl::event_x(), xzoom);
    if (x<0) x = 0;
    if (x>w-1) x = w-1;
    return x;
  };
  inline double get_event_y_d()
  {
    double y = dounzoom((double)Fl::event_y(), yzoom);
    if (y<0) y = 0;
    if (y>h-1) y = h-1;
    return y;
  };

  void saveFile(); // save displayed image as ppm
  void exportGnuplot(); // export processed data as a gnuplot 3 column file (saved as current name plus .pm3d extension).
  void exportPGM(); // export processed data as a PGM
  void exportMTX(bool save=false, bool zoom=false); // export processed data as a MTX
  void exportMAT();
  void exportMatlab(); // a .m script that includes the data and the
		       // colormap, and generates both an imagesc plot
		       // and a surf plot
  void exportLinecut();
  char *exportPS();  // Export processed data as a ps file; return filename.
  void setupPS();   // Setup to export a PS file.
  void dumpColormap();

  Image_Printer_Control ipc;
  Image_Printer *imageprinter;

  PeakFinder_Control pfc;
  PeakFinder *pf;

  ImageData id;
  double xunit(double x) { return id.getX(x); };
  double yunit(double y) { return id.getY(y); };
  double zunit(double d) { return id.quant_to_raw(d); };
  double getz(double d) { return id.quant_to_raw(d); };

  void setMin(int m);
  void setMax(int m);
  void setCenter(int c);
  void setWidth(int w);

  void setMinLevel(int m);
  void setMaxLevel(int m);
  void setGamma(double g, double gc);
  void setColormap(uchar *cmap, int l);  // cmap should be uchar[3*l] 
  uchar *getColormap() 
    { return colormap; }; 
  unsigned getColormapLength() 
    { return static_cast<unsigned>(colormap_length); };
  void setXZoom(int xz);
  void setYZoom(int yz);
  void allocateImage();  
  int handle(int event);

  inline void getrgb(int row, int col, unsigned char &r, unsigned char &g, unsigned char &b)
  {
    makergb(data[row*w+col],r,g,b,row,col);
  }

  inline void makergb(int data, unsigned char &r, unsigned char &g, unsigned char &b, int row=-1, int col=-1)
  {
    int tmp = data;
    if (row != -1)
      tmp -= planeval(row,col);  
      
    if (tmp < 0) tmp = 0;
    if (tmp > LMAX-1) tmp = LMAX;
    
    tmp = imagehist[tmp]*(colormap_length-1)/LMAX;

    r = colormap[3*tmp];
    g = colormap[3*tmp+1];
    b = colormap[3*tmp+2];
  }

  inline string search_replace(string str, string searchString, string replaceString)
  {
    string::size_type pos = 0;
    while ( (pos = str.find(searchString, pos)) != string::npos ) 
      {
	str.replace( pos, searchString.size(), replaceString );
	pos++;
      }
    return str;
  }

  void plotLineCut(bool nothrottle=false);   // Schedule a line cut update.
  void plotCmap();
  void plotHist();

  int planeval(int x, int y);
  double dataval(int x, int y);
  void fit_plane();

  int *get_data();

  // If this is true, eat initial clicks.
  bool stupid_windows_focus;

  // New feature for permutating mouse button order for poor people
  // (like me) who have only one mouse button on their MacBook
  int mouse_order;  // 0 = (123), 1 = (231), 2 = (312)

  // These variables store the levels of the 16 bit greymap data that
  // get mapped to the edges of the of the colormap
  int hmin;
  int hmax;

  // The limits of the data (used for plotting histogram)
  int datamin;
  int datamax;

  // Min and max levels used in the colormap (double value from 0 to 1);
  double cmap_min;
  double cmap_max;

  double plane_a;
  double plane_b;
  double plane_c;

  int xzoom;
  int yzoom;
  bool swap_zoom_state;

  bool plane;
  bool invert;
  bool negate;
  bool square;

  bool plot_cmap;
  bool plot_hist;

  window_size_action_t window_size_action;
  bool subplane;  
  int line_cut_xp, line_cut_yp; // for horizonal/vertical line cuts
  bool line_cut_xauto;
  int lcx1, lcy1, lcx2, lcy2;
  int line_cut_limit; // Limit the types of allowed line cuts.  
  int line_cut_type; 
  lc_axis_t lc_axis;
  Gnuplot_Interface gplinecut, gphist, gpcmap;

  char gp_using_string[256];
  // Ok, I should really start taking advantage of the STL stuff (I can be pretty stubborn...)
  std::string gp_with_string;

  string filename;
  char output_basename[256];
  string original_dataname;

  char *cc_filename;
  double gam;
  double gcenter;
  int w;
  int h;
  double bpercent;
  double wpercent;

  // We want to be able to open or close the controls window by right
  // clicking in the image window, as in xv. 
  Fl_Window *controls_window;

  // The colormap window
  ColormapWindow *colormap_window;

  // We will also keep track of a zoom window to be popped up with 'z'				
  ZoomWindow *zoom_window;
  void showZoomWindow(bool toggle=false); // if toggle, toggle the state.  Otherwise, show
  void makeZoomWindow(); // Make the zoom window if it doesn't exist ,but don't show it.
private:
  int zwd_x1, zwd_x2, zwd_y1, zwd_y2;     // Used while "dragging" the zoom window to store mouse state
  typedef enum { ZOOM_NO = 0,
			    ZOOM_DRAG=1,
			    ZOOM_RESIZE_NE,
			    ZOOM_RESIZE_NW,
			    ZOOM_RESIZE_SE,
			    ZOOM_RESIZE_SW } zoomWindowDragging_t;
  zoomWindowDragging_t zoomWindowDragging;
  void tryZoomCorner(int x1, int y1, int x2, int y2, zoomWindowDragging_t zw, double &dist);
  void adjust_window_size();
public:
  // An optional external function to call every time the mouse moves
  // in the image window or a key is pressed
  void (*statusCallback)(int key, bool down);

  // We also optionally will reference an external browser widget
  // called process_queue, which is a list of operations to perform.
  Fl_Browser *process_queue;

  // An external function to call when we've changed the value of some
  // of our parameters.
  void (*external_update)();

  // A drag and drop handler
  void (*drag_n_drop)(const char *text);

  int *data; //original data from file

  int datahist[LMAX+1];

  void make_tmpfiles();
  char xsection_fn[256];
  char xsection_tmp_fn[256];
  char hist_fn[256];
  char cmap_fn[256];
  
  double colormap_rotation_angle;
  std::string operations_string;

 protected:
  uchar *colormap;
  int colormap_length;
  int *gammatable;
  int imagehist[LMAX+1];  // this is actually a lookuptable, not really a histogram: badly named variable...
  void draw();
  uchar *image;
  int *databuf; // data after contrast stretch
  int cmap;

public:

  template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
//      ar & *imageprinter;
      ar & hmin;
      ar & hmax;
      ar & bpercent;
      ar & wpercent;
      ar & xzoom;
      ar & yzoom;
      if(version >= 2)
	ar & swap_zoom_state;
      ar & plane;
      ar & negate;
      ar & square;
      ar & gam;
      ar & colormap_rotation_angle;
      ar & plot_hist;
      ar & plot_cmap;
      ar & plane_a;
      ar & plane_b;
      if (version >= 3 && version < 4)
	{
	  // keep_window_size has changed.
	  bool tmp;
	  //ar & keep_window_size;
	  ar & tmp;
	}
      if (version >= 4)
	ar & window_size_action;
      if(version >= 5)
	ar & line_cut_xauto;
      // Here is where we save the image processing operations
      if(process_queue)
	{
	  if(Archive::is_saving::value)
	    {
	      int tmp = process_queue->size(); 
	      ar & tmp;
	      fprintf(stderr,"Saving %d process entries\n",tmp);
	      for(int i = 1; i <= process_queue->size(); i++)
		{		  
		  std::string tmp = process_queue->text(i);
		  ar & tmp;
		  Image_Operation * t = reinterpret_cast<Image_Operation *>(process_queue->data(i));
		  ar & t;
		}
	    }
	  else
	    {
	      for(int i = 1 ; i <= process_queue->size(); i++)
		delete reinterpret_cast<Image_Operation *>(process_queue->data(i));
	      int len;
	      process_queue->clear();
	      ar & len; // This has the size  of the expected queue
	      fprintf(stderr,"Loading %d process entries\n",len);
	      for(int i = 1; i <= len; i++)
		{
		  std::string tmp;
		  ar & tmp;
		  Image_Operation *op;
		  ar & op;
		  process_queue->add(tmp.c_str(),op);
		}
	    }
	}

      if(external_update)
	external_update();
    };     
};

BOOST_CLASS_VERSION(ImageWindow, 5); // Increment this if the archive format changes.

class ColormapWindow : public Fl_Window
{
private:
  int xmult;
  int iw, ih;
  bool vertical;
public:
  static const int wid=256;
  ColormapWindow(int w, int h, const char *title = 0);
  ~ColormapWindow();
  ImageWindow *img;
  uchar *image;
  void update();
  void draw();
  void resize(int x, int y, int w, int h);
  void saveFile(const char *name="colormap.ppm");
};

class ZoomWindow : public Fl_Double_Window
{
private:
  int push_mouse_x;
  int push_mouse_y;
  int push_center_x;
  int push_center_y;

  unsigned char *image;
  size_t image_size;
  void realloc_image();
  char window_label[256];

public:
  static const unsigned hist_len = LMAX+1;
  int histogram[hist_len];

  ZoomWindow(int w, int h, const char *title = 0);  

  ImageWindow *img;
  int xscale, yscale;

  int center_x, center_y;

  void getSourceArea(int &x1, int &y1, int &x2, int &y2); // Work out where we're displaying.
  void draw();  
  void resize(int x, int y, int w, int h);
  int handle(int event);
  void calculateHistogram();
  void zoomMoved(); // Call this when the focus of the zoom window has changed
  bool autonormalize;
};

// This class stores all the information we need about an image operation.
class Image_Operation
{
public:
  std::string name;
  std::string description;
  bool enabled;
  int num_parameters;

  class Parameter
  {
  public:
    std::string name;
    double value;
    Fl_Callback *cb;
    Parameter(const char *n="", double v=0.0, Fl_Callback *cbp=NULL) : name(n), value(v), cb(cbp) { };

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
      ar & name;
      ar & value;
    };
  };
  typedef std::vector<Parameter> parameters_t;
  parameters_t parameters;

  double getParameter(const char *str);
  void addParameter(const char *str, double def=0.0, Fl_Callback *cb=NULL) { parameters.push_back(Parameter(str,def,cb)); num_parameters++;};

  Image_Operation(const char *namep="", const char *descriptionp="") : name(namep), description(descriptionp), enabled(true) { num_parameters = 0; };

  template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
      ar & name;
      ar & description;
      ar & enabled;
      ar & num_parameters;
      ar & parameters;
    };
};


template <class T>
inline void zap(T & x)
{
  if (x != NULL)
    {
      delete [] x;
      x = NULL;
    }
}
;

void inform(char *message,...);
#endif
