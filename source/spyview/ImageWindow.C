#include <assert.h>
#include <string>
#include <FL/fl_ask.H>
#include <sys/time.h>
#include "ImageWindow.H"
#include "ImagePrinter.H"
#include "math.h"
#include "message.h"
#include <FL/Fl_Color_Chooser.H>
#include <stdarg.h>
#include "throttle.H"
#include "ImageWindow_Module.H"
#include <ctype.h>

#include "mypam.h"

#ifdef WIN32

#include <windows.h>

// For win32 gettimeofday():
// Copied from http://www.cpp-programming.net/c-tidbits/gettimeofday-function-for-windows/
#include <time.h>
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

#endif

// Bitwise if and only if
//#define iff(A,B) ((A & B) && !(A & !B)) 
// not as useful as I thought since events also include numlock...

#define ctrl(state)  ( (state & FL_CTRL) && !(state & FL_SHIFT) && !(state & FL_ALT))
#define shift(state) (!(state & FL_CTRL) &&  (state & FL_SHIFT) && !(state & FL_ALT))
#define alt(state)   (!(state & FL_CTRL) && !(state & FL_SHIFT) &&  (state & FL_ALT))
#define none(state)  (!(state & FL_CTRL) && !(state & FL_SHIFT) && !(state & FL_ALT))

using namespace std;

FILE *fopenwarn(const char *name, const char *mode)
{
  FILE *fp = fopen(name, mode);
  
  if (fp == NULL)
    warn("Could not open file %s in mode \"%s\": %s\n", name, mode, strerror(errno));
  return fp;
}

static void nognuplot()
{
  static bool warned = false;
  if(warned)
    return;
#ifdef WIN32
  warn("Gnuplot is not available.  Please install gnuplot for windows and put it on your path.\n");
#else
  warn("Unable to open gnuplot\n");
#endif
}


void ImageWindow::make_tmpfiles()
{

  // If we can, we will use local files xsection.dat, hist.dat and
  // cmap.dat in the current directory, as a convenient way to give
  // the user access to this data
  //
  // If we can't (ie. directory is read only), we will use files in /tmp.

  FILE *fp;
  static int ntries;

  snprintf(xsection_fn, sizeof(xsection_fn), "%sxsection.%d.dat.%d", Gnuplot_Interface::tmpdir(), ntries, getpid());
  if ((fp = fopen(xsection_fn, "w")) == NULL)
    { error("Failed to create tempfile %s: %s\n", xsection_fn, strerror(errno)); }
  else fclose(fp);

  snprintf(xsection_tmp_fn, sizeof(xsection_fn), "%sxsection.%d.dat.tmp_%d", Gnuplot_Interface::tmpdir(), ntries, getpid());
  if ((fp = fopen(xsection_fn, "w")) == NULL)
    { error("Failed to create tempfile %s: %s\n", xsection_tmp_fn, strerror(errno)); }
  else fclose(fp);

  snprintf(cmap_fn, sizeof(cmap_fn), "%scmap.%d.dat.%d", Gnuplot_Interface::tmpdir(), ntries, getpid());
  if ((fp = fopen(cmap_fn, "w")) == NULL)
    { error("Failed to create tempfile %s: %s\n", cmap_fn, strerror(errno)); }
  else fclose(fp);

  snprintf(hist_fn, sizeof(hist_fn), "%shist.%d.dat.%d", Gnuplot_Interface::tmpdir(), ntries, getpid());
  if ((fp = fopen(hist_fn, "w")) == NULL)
    { error("Failed to create tempfile %s: %s\n", hist_fn, strerror(errno)); }
  else fclose(fp);

  info("Locations of temporary files:\n%s\n%s\n%s\n", xsection_fn, cmap_fn, hist_fn);
}

ImageWindow::ImageWindow(int w, int h, const char *title) : 
  Fl_Overlay_Window(w,h) , ipc(this) , pfc(this) 
{
  stupid_windows_focus = getenv("SPYVIEW_CLICKY") == NULL;
    
  gplinecut.bidirectional=true;
  line_cut_limit = HORZLINE | VERTLINE | OTHERLINE | NOLINE;
  //size_range(1,1);
  zoomWindowDragging = ZOOM_NO;
  statusCallback = NULL;
  datamin = 0; datamax = LMAX;
  hmin = 0; hmax = LMAX;
  mouse_order = 0;

  xzoom = yzoom = 1;
  swap_zoom_state=false;
  square = 0;
  line_cut_type = NOLINE;
  line_cut_xauto = 1;
  plot_hist = false;
  plot_cmap = false;
  bpercent = 0.1; wpercent = 0.1;
  //  plane_a = plane_b = plane_c = 0;
  process_queue = NULL;
  controls_window = NULL;
  external_update = NULL;
  drag_n_drop = NULL;
  window_size_action = KEEPZOOM;
  cmap_min = 0.0;
  cmap_max = 1.0;

  sprintf(gp_using_string, "1:2");
  gp_with_string = "lp";

  lc_axis = DISTANCE;

  make_tmpfiles();
  
  // By default, install an 8 bit greyscale colormap.
  colormap_length = 256;
  colormap = new uchar[colormap_length*3];
  gammatable = new int[colormap_length];

  for (int i=0; i<256; i++)
    colormap[3*i] = colormap[3*i+1] = colormap[3*i+2] = i;
  
  gam = 1;
  gcenter = 0.0;
  for (int i=0; i<256; i++)
    gammatable[i] = i;

  // Create the colormap window
  colormap_window = new ColormapWindow(256,25);
  colormap_window->img = this;
  colormap_window->label("Colormap");
  //  Colormap window is now controlled from UI.  Don't show by default.
  //  colormap_window->show();

  imageprinter = new Image_Printer(this, &ipc);

  // Note: pfc is initialized with the constructor of ImageWindow 
  pf = new PeakFinder(this, &pfc);
}

void ImageWindow::resize(int x, int y, int wp, int hp)
{
  int target_xzoom, target_yzoom;

  // This is tricky...

  if (wp == w) target_xzoom = 1; // Zoom should be 1
  else if (wp*(-xzoom) == w) target_xzoom = -w/wp; // Requested size corresponds exactly to a negative zoom
  else if (wp < w && wp*(-xzoom) != w) target_xzoom = -w/(wp+1)-1;  //Negative zoom, need to trucate downwards (-1) and be careful of images with odd number of points (wp+1)
  else target_xzoom = wp/w; // Positive zoom: truncation has desired effect, odd number of points not a problem.

  if (hp == h) target_yzoom = 1;
  else if (hp < h && hp*(-yzoom) != h) target_yzoom = -h/(hp+1)-1;
  else if (hp*(-yzoom) == h) target_yzoom = -h/hp; 
  else target_yzoom = hp/h;

  if (xzoom != target_xzoom || yzoom != target_yzoom)
    {
      xzoom = target_xzoom;
      yzoom = target_yzoom;
      allocateImage();
    }

  external_update();
  size_range(1,1);
  Fl_Overlay_Window::resize(x,y,wp,hp);
}

ImageWindow::~ImageWindow()
{
  if(imageprinter)
    delete imageprinter;
  unlink(xsection_fn);
  unlink(cmap_fn);
  unlink(hist_fn);
  unlink(xsection_tmp_fn);
  unlink("xsection.dat");
  unlink("cmap.dat");
  unlink("hist.dat");
}

void ImageWindow::draw() 
{
  if (!id.data_loaded) return;
  imageprinter->updatePreview();
  
  // data = original data read from file
  //
  // databuf = copy of data[] to store grey data in after we've performed integer
  //           operations, like rescaling the contrast using the imagehist lookup
  //           table, appling the gamma scaling using the gammatable lookup
  //           table, and performing the plane subtraction.
  //
  // Data starts off in data[] in whatever range of values that is
  // occupied in the input image.
  //
  // After plane subtraction, data may exceed LMAX or be less than 0,
  // so we need to trucate these values.
  //
  // The data between hmin and hmax then gets mapped to 0 to LMAX by
  // the imagehist lookup table.
  //
  // The gammatable lookup is then applied, and each pixel in the
  // databuf array will then have a value that ranges from 0 to
  // colormap_length-1.
  //
  // In the final step, the colormap lookup table is applied, which is
  // used to map the data onto a 8x8x8 bit RGB image.
  
  //  warn("Drawing %dx%d image, xzoom=%d, yzoom=%d, total=%dx%d (%d bytes)\n",
  //	  w,h,xzoom,yzoom,w*xzoom,h*yzoom,3*(w*xzoom+h*yzoom));

  // This next chunk of code centers the image.  It should only matter if we're displaying an image bigger than
  // 255x255, in which case by resizing the window manually, the user can make the window not be an intereger
  // zoom of the original image.
  // Zooming by using < and > will still always give an integer zoom.
  int ox, oy; // X and Y offsets.
  ox = Fl_Overlay_Window::w() - w*xzoom;
  oy = Fl_Overlay_Window::h() - h*yzoom;
  if(ox != 0 || oy != 0)
    {
      ox /= 2; // Center the image.  We don't do this outside the if so if the image is only 1 pixel too small,
      oy /= 2; // we still fill in the border in black.
      fl_color(FL_BLACK);
      fl_rectf(0,0,Fl_Overlay_Window::w(), Fl_Overlay_Window::h());
    }
  ox=0; // Argh!  Centering the image kills the line cuts!  Too lazy to fix this right.
  oy=0;

  // Ok, let's finally clean this up a bit. I will now make this
  // compatible with "negative" zoom (ie. shrinking the image). To
  // keep things simple at first, I will do this by just skipping
  // points (a smarter thing to do would be to average).

  int row, col;             // column and row in the data file
  int imgcol, imgrow;       // column and row in the image output file
  int n, m;                 // repeat indices for col and row
  int iw, ih;

  iw = dozoom(w,xzoom);
  ih = dozoom(h,yzoom);
  
  row = 0; n = 0;
  while (row<h) // loop through rows (col = 0; col <h; col++)
    {
      if (yzoom < 0)
	{
	  if (row%(-yzoom) != 0)
	    {
	      row++;
	      continue;
	    }
	  else imgrow = row/(-yzoom);
	}
      else imgrow = yzoom*row + n;
      if (imgrow > ih-1) break;
      
      col = 0; m = 0;
      while (col<w) // now loop through columns
	{
	  if (xzoom < 0)
	    {
	      if (col%(-xzoom) != 0)
		{
		  col++;
		  continue;
		}
	      else imgcol = col/(-xzoom);
	    }
	  else imgcol = xzoom*col + m;
	  if (imgcol > iw-1) break;

	  // Now actually decide on a color for the pixel

	  unsigned char r,g,b;
	  getrgb(row,col,r,g,b);

	  image[3*imgrow*iw+3*imgcol] = r;
	  image[3*imgrow*iw+3*imgcol+1] = g;
	  image[3*imgrow*iw+3*imgcol+2] = b;

	  if (3*imgrow*iw+3*imgcol+2 > dozoom(w,xzoom)*dozoom(h,yzoom)*3)
	    {
	      warn( "row %d col %d n %d m %d ir %d ic %d iw %d ih %d iw*ih*3 %d over bounds %d > %d\n", 
		      row, col, n, m, imgrow, imgcol, iw, ih, iw*ih*3, 3*imgrow*iw+3*imgcol+2, 
		      dozoom(w,xzoom)*dozoom(h,yzoom)*3);
	      exit(-1);
	    }
	  if (xzoom < 0 || m == xzoom-1)
	    {
	      m=0; col++; 
	      if (col == w) break; 
	      else continue;
	    }
	  else
	    {
	      m++; continue; 
	    }
	}
      
      if (yzoom < 0 || n == yzoom-1)
	{
	  n=0; row++; 
	  if (row == h) break;
	  else continue;
	}
      else 
	{
	  n++; continue;
	}
    }

  fl_draw_image(image, ox, oy, iw, ih, 3, 0);
  if(zoom_window)
    zoom_window->redraw();
}

void ImageWindow::draw_overlay() 
{
  if (pfc.hide_data->value())
    fl_rectf(0,0,dozoom(w,xzoom),dozoom(h,yzoom),0,0,0);

  imageprinter->updatePreview();
  if (line_cut_type)
    {
      fl_color(FL_RED);
      if (line_cut_type == HORZLINE)
	fl_line(dozoom(w,xzoom),dozoom(line_cut_yp,yzoom)+dozoom(1,yzoom)/2,
		0,dozoom(line_cut_yp,yzoom)+dozoom(1,yzoom)/2);
      else if (line_cut_type == VERTLINE)
	fl_line(dozoom(line_cut_xp,xzoom)+dozoom(xzoom,1)/2,0,
		dozoom(line_cut_xp,xzoom)+dozoom(xzoom,1)/2,dozoom(h,yzoom));
      else if (line_cut_type == OTHERLINE)
	fl_line(dozoom(lcx1,xzoom)+dozoom(1,xzoom)/2, 
		dozoom(lcy1,yzoom)+dozoom(1,yzoom)/2, 
		dozoom(lcx2,xzoom)+dozoom(1,xzoom)/2, 
		dozoom(lcy2,yzoom)+dozoom(1,yzoom)/2);
    }

  if((zoom_window && zoom_window->visible()) || (zoomWindowDragging))
    {
      fl_color(FL_GREEN);
      if(!zoomWindowDragging)
	zoom_window->getSourceArea(zwd_x1,zwd_y1,zwd_x2,zwd_y2);
      int x2off = (xzoom > 0) ? 1 : 0;
      int y2off = (yzoom > 0) ? 1 : 0;
      fl_line(dozoom(zwd_x1,xzoom), 
	      dozoom(zwd_y1,yzoom), 
	      dozoom(zwd_x2,xzoom)-x2off, 
	      dozoom(zwd_y1,yzoom));
      fl_line(dozoom(zwd_x2,xzoom)-x2off, 
	      dozoom(zwd_y1,yzoom), 
	      dozoom(zwd_x2,xzoom)-x2off, 
	      dozoom(zwd_y2,yzoom)-y2off);
      fl_line(dozoom(zwd_x2,xzoom)-x2off, 
	      dozoom(zwd_y2,yzoom)-y2off, 
	      dozoom(zwd_x1,xzoom), 
	      dozoom(zwd_y2,yzoom)-y2off);
      fl_line(dozoom(zwd_x1,xzoom), 
	      dozoom(zwd_y2,yzoom)-y2off, 
	      dozoom(zwd_x1,xzoom), 
	      dozoom(zwd_y1,yzoom));
    }

  if(pf->peaks != NULL && (pfc.plot_peaks->value() || pfc.plot_valleys->value()))
    {
        for (int j=0; j<h; j++)
	{
	  for (int i=0; i<w; i++)
	    {
	      if (i>=pf->w || j>=pf->h)
		// So that we can plot peaks from previous IP settings
		// on top of current data without a segfault
		continue;
	      if (pf->peaks[j*w+i] == 1 && pfc.plot_peaks->value())
		draw_overlay_pixel(i,j,pfc.peak_color_box->color());
	      else if (pf->peaks[j*w+i] == -1 && pfc.plot_valleys->value())
		draw_overlay_pixel(i,j,pfc.valley_color_box->color());
	    }
	}
    }

  for(modules_t::iterator i = modules.begin(); i != modules.end(); i++)
    (*i)->overlay_callback();
}

void ImageWindow::draw_overlay_pixel(int i, int j, Fl_Color color)
{
  int rw = (xzoom < 0) ? 1 : xzoom;
  int rh = (yzoom < 0) ? 1 : yzoom;
  fl_color(color);
  fl_rectf(dozoom(i,xzoom), dozoom(j,yzoom), rw, rh);
}

void ImageWindow::draw_overlay_pixel(int i, int j, uchar r, uchar g, uchar b)
{
  int rw = (xzoom < 0) ? 1 : xzoom;
  int rh = (yzoom < 0) ? 1 : yzoom;
  fl_color(r,g,b);
  fl_rectf(dozoom(i,xzoom), dozoom(j,yzoom), rw, rh);
}

void ImageWindow::makeZoomWindow()
{
  if(zoom_window == NULL)
    {      
      zoom_window = new ZoomWindow(dozoom(w,xzoom)/5,dozoom(h,yzoom)/5); // *.1 * 2 = /5, 10% of image size (default zoom is 2)
      zoom_window->img = this;
      zoom_window->size_range(1,1);
      if (xzoom > 1) zoom_window->xscale *= xzoom;
      if (yzoom > 1) zoom_window->yscale *= yzoom;
    }
}
void ImageWindow::showZoomWindow(bool toggle)
{
  makeZoomWindow();
  if(zoom_window->visible() && toggle)
    zoom_window->hide();
  else
    zoom_window->show();
}

// Helper function for the middle-shift button below.
inline static double distance_squared(double x1, double y1, double x2, double y2)
{
  x1 -= x2;
  y1 -= y2;
  return x1*x1+y1*y1;
}

// Check to see if (x1,y1) is closer to (x2,y2) than squared-distance distance.  If it is, update
// distance to the distance between them, and set zoomWindowDragging to zw
// Helper function for the middle-shift button below.
void ImageWindow::tryZoomCorner(int x1, int y1, int x2, int y2, zoomWindowDragging_t zw, double &dist)
{
  double nd = distance_squared(x1,y1,x2,y2);
  if(nd < dist)
    {
      dist = nd;
      zoomWindowDragging = zw;
    }
}

// adapted from from http://www.cpp-programming.net/c-tidbits/gettimeofday-function-for-windows/
double current_time()
{
  struct timeval stop;
#ifndef WIN32
  gettimeofday(&stop, NULL);
#else
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  GetSystemTimeAsFileTime(&ft);
  tmpres |= ft.dwHighDateTime;
  tmpres <<= 32;
  tmpres |= ft.dwLowDateTime;
  /*converting file time to unix epoch*/
  tmpres /= 10;  /*convert into microseconds*/
  tmpres -= DELTA_EPOCH_IN_MICROSECS;
  stop.tv_sec = (long)(tmpres / 1000000UL);
  stop.tv_usec = (long)(tmpres % 1000000UL);
#endif
  double time = (((double)(stop.tv_sec)) + ((double)(stop.tv_usec) * 1e-6));
  if (!isnormal(time))
    info("time %e sec %d usec %d\n", time, stop.tv_sec, stop.tv_usec);
  return time;
}

int ImageWindow::handle(int event) 
{
  static double lastFocus = 0; // Time when we got the focus, or NAN if we don't have the focus.
  static double t0Focus = current_time(); // Just a handy offset for debug messages
  static bool hungryFocus = false;  // If this is true, we're eating push/drag events because this was the click that focused the mouse.
  static bool inFocus = false; // This is set to true when we get a focus event
  //if (event == FL_ENTER)
  // info("got FL_ENTER\n");
  //if (event == FL_MOVE)
  //  info("got FL_MOVE\n");
  //Fl_Window::handle() was stealing our FL_ENTER and FL_MOVE events
  //if (Fl_Window::handle(event)) return 1;

  for(modules_t::iterator i = modules.begin(); i != modules.end(); i++)
    {
      int r = (*i)->event_callback(event);
      if(r)
	return r;
    }

  int n;
  int button;

  button = Fl::event_button();
  
  // Mouse order = 0: 1->1 2->2 3->3
  // Mouse order = 1: 2->1 3->2 1->3
  // Mouse order = 2: 3->1 1->2 2->3
  
  //if (event == FL_PUSH || event == FL_DRAG || event == FL_RELEASE) 
  //info("Button %d -> ", button);
  button = button + mouse_order;
  if (button == 4) button = 1;
  if (button == 5) button = 2;
  //if (event == FL_PUSH || event == FL_DRAG || event == FL_RELEASE) 
  //info("%d\n", button);
 
  switch (event)
    {
      // Drag and drop
    case FL_DND_ENTER:          // return(1) for these events to 'accept' dnd
    case FL_DND_DRAG:
    case FL_DND_RELEASE:
      return(1);
    case FL_PASTE:              // handle actual drop (paste) operation
      (*drag_n_drop)(Fl::event_text());
      return(1);
    case FL_SHOW:
      // Use a cross cursor				    
      cursor(FL_CURSOR_CROSS);
      break;
    case FL_KEYUP:
      n = Fl::event_key();
      if(statusCallback)
	statusCallback(n,false);
      break;
    case FL_KEYDOWN:      
      n = Fl::event_key();
      if(statusCallback)
	statusCallback(n,true);
      switch(n)
	{	
// 	case 'd':
// 	  dumpColormap();
// 	  return 1;
	case 's':
	  switch(Fl::event_state() & (FL_SHIFT | FL_CTRL | FL_ALT))
	    {
	    case FL_ALT:
	      // Snap the window size to the actual image size
	      size(dozoom(w,xzoom), dozoom(h,yzoom));
	      return 1;
	    case FL_CTRL:
	      if(zoom_window)
		{
		  zoom_window->autonormalize = !zoom_window->autonormalize;
		  return 1;
		}
              // Falling case					    
	    case FL_SHIFT:
	      normalizeOnZoom();
	      return 1;
	    }
	  break;
	case 'm':
	  mouse_order = (mouse_order+1)%3;
	  external_update();
	  break;
	case 'z':
	  if (Fl::event_state() & FL_CTRL)
	    exportMTX(false, true);
	  else 
	    showZoomWindow(true);
	  return 1;
	  break;
	case '.':
	  if (Fl::event_state() & FL_SHIFT)
	    {
	      setXZoom(xzoom+1);
	      damage(FL_DAMAGE_ALL);	      
	    }
	  else if (Fl::event_state() & FL_CTRL)
	    {
	      setYZoom(yzoom+1);
	      damage(FL_DAMAGE_ALL);
	      
	    }
	  else 
	    {
	      setXZoom(xzoom+1); 
	      setYZoom(yzoom+1);
	      damage(FL_DAMAGE_ALL);
	    }
	  return 1;
	case ',':
	  if (Fl::event_state() & FL_SHIFT)
	    {
	      setXZoom(xzoom-1);
	      damage(FL_DAMAGE_ALL);
	    }
	     
	  else if (Fl::event_state() & FL_CTRL)
	    {
	      setYZoom(yzoom-1);
	      damage(FL_DAMAGE_ALL);
	      
	    }
	  else 
	    {
	      setXZoom(xzoom-1); 
	      setYZoom(yzoom-1);
	      damage(FL_DAMAGE_ALL);
	    }
	  return 1;
	case '1':
	  if (Fl::event_state() & FL_SHIFT)
	    (xzoom > yzoom) ? setXZoom(yzoom) : setYZoom(xzoom);
	  else
	    (xzoom > yzoom) ? setYZoom(xzoom) : setXZoom(yzoom);
	  damage(FL_DAMAGE_ALL);
	  return 1;
// 	case 'i':
// 	  invert = !invert;
// 	  plotCmap();
// 	  redraw();
// 	  colormap_window->update();
// 	  if (external_update != NULL)
// 	    (*external_update)();
//	  return 1;
// 	case 'h':
// 	  plot_hist = !plot_hist;
// 	  plotHist();
// 	  if (external_update != NULL)
// 	    (*external_update)();
// 	  return 1;
	default:
	  if(isalpha(n))
	    {
	      char buf[4096];
	      const char *cmd = getenv("SPYVIEW_EXTERNAL");
	      if(cmd == NULL)
		break;
	      int ix = get_event_x();
	      int iy = get_event_y();
	      snprintf(buf,sizeof(buf),"%s %c %g %g %d %d", cmd, n, id.getX(ix), id.getY(iy),ix, iy);
	      int res = system(buf);
	      if(res == -1)
		warn("Unable to run command: \"%s\": %s\n",buf, strerror(errno));
	      if(res == 0)
		{
		  info("Reloading: \"%s\"\n",filename.c_str());
		  string s = filename;		 // We copy here because loadData nukes filename right away.
		  loadData(s.c_str());
		}
	    }
	  break;

	}    
      break;

    case FL_MOVE:
      if(statusCallback)
	statusCallback(-1,true);
      break;

    case FL_FOCUS:
      lastFocus = current_time();
      inFocus = true;
      //info("focus event time %g\n", lastFocus);
      //fprintf(stderr,"recieved focus %g time %g t0 %g\n",lastFocus-t0Focus, lastFocus, t0Focus);
      return Fl_Overlay_Window::handle(event);

    case FL_UNFOCUS:
      lastFocus = NAN;
      inFocus = false;
      //fprintf(stderr,"Defocus %g at time %g t0 %g\n",current_time()-t0Focus, current_time(), t0Focus);
      return Fl_Overlay_Window::handle(event);

/* Focus logic:
   When we are focused, ignore the *FIRST* mouse click if it occurs within 50 ms of becoming focused.  This way, if the user
     focuses the window with a non-mouse click action (alt-tab), we're in good shape as long as they're not *really* fast.  In
     practice, I usually see the click event comes within a few microseconds (!) of the focus event (under wine)
   If we ignored a click, ignore follow-up drag and release events, unless the user keeps the button down for at least
     one second.  If they do, assume they're frustrated and give in.
   If we get a click when unfocused, assume it's an immediate prelude to being focused and treat it as a click within 50ms of
     getting focused.  This never seems to happen (under my WM in Linux and wine.  Probably smart to leave the logic in)
*/

    case FL_PUSH:
      if(isnan(lastFocus) && stupid_windows_focus)
	{
	  fprintf(stderr,"Ate unfocused click.  This apparently never happens.\n");	  
	  hungryFocus = true;
	  lastFocus = current_time();
	  return Fl_Overlay_Window::handle(event);
	}
      // Warning: you can have an unfocused drag or release under linux; we shouldn't eat these!
      // For this reason, we will only eat things if we are in focus
    case FL_DRAG: // Falling case!
    case FL_RELEASE:

      // Let's only eat a non-modified button 1 click
      if(stupid_windows_focus && (button == 1) && !(Fl::event_state() & FL_SHIFT) && !(Fl::event_state() & FL_CTRL)) 
	{
	  if (hungryFocus)
	    {
	      if (inFocus)
		{
		  if(event == FL_RELEASE || (current_time() - lastFocus >= 0.1))
		    {
		      //fprintf(stderr,"Focus eating is done (100 ms after initial click) %g\n",current_time()-lastFocus);
		      lastFocus = 0;
		      hungryFocus = false;
		      return Fl_Overlay_Window::handle(event);
		    }
		  else
		    {
		      fprintf(stderr,"Ate drag/release event, t<100 ms after focus click %g\n",current_time()-lastFocus);
		      return Fl_Overlay_Window::handle(event);
		    }
		}
	    }
	  else if ((current_time() - lastFocus < 0.05))
	    {
	      //fprintf(stderr,"Ate initial click (%g) last %g t0 %g\n",lastFocus-t0Focus, lastFocus, t0Focus);
	      hungryFocus = true;
	      return Fl_Overlay_Window::handle(event);
	    }
	}

      if(statusCallback)
	statusCallback(-1,true);

      int state = Fl::event_state();
      int tmp;
      double d1,d2;
      if (none(state) || alt(state)) // horz/vert LC
	switch(button)
	  {
	  case 1:
	    tmp = get_event_y();
	    if (tmp > h-1)
	      tmp = h-1;
	    if (line_cut_yp < 0)
	      tmp = 0;
	    if (line_cut_yp == tmp && line_cut_type == HORZLINE)
	      return 1;
	    if(!(line_cut_limit & HORZLINE))
	      return 1;
	    line_cut_yp = tmp;
	    line_cut_type = HORZLINE;
	    plotLineCut();
	    redraw_overlay();
	    return 1;
	  case 3:
	    if(!alt(state))
	      break; // Right-alt is vertical, but not right-unshifted.
	  case 2:	  
	    tmp = get_event_x();
	    if (tmp > w-1)
	      tmp = w-1;
	    if (tmp < 0)
	      tmp = 0;
	    if (line_cut_xp == tmp && line_cut_type == VERTLINE) return 1;
	    if(!(line_cut_limit & VERTLINE))
	      return 1;
	    line_cut_xp = tmp;
	    line_cut_type = VERTLINE;
	    plotLineCut();
	    redraw_overlay();
	    return 1;
	  default: 
	    return Fl_Overlay_Window::handle(event);
	  }
      else if (shift(state)) // zoom windows (this is a long one)
	switch(button)
	{
	case 1:
	  switch(event)
	    {
	    case FL_PUSH:
	      zwd_x1 = get_event_x();
	      zwd_y1 = get_event_y();
	      zwd_x2 = zwd_x1;
	      zwd_y2 = zwd_y1;
	      zoomWindowDragging = ZOOM_DRAG;
	      break;
	    case FL_RELEASE:
	      zwd_x2 = get_event_x();
	      zwd_y2 = get_event_y();
	      makeZoomWindow();
	      zoom_window->center_x = (zwd_x1+zwd_x2)/2;
	      zoom_window->center_y = (zwd_y1+zwd_y2)/2;
	      zoom_window->size((abs(zwd_x2-zwd_x1))*zoom_window->xscale, (abs(zwd_y2-zwd_y1))*zoom_window->yscale);
	      showZoomWindow();
	      zoom_window->zoomMoved();
	      zoomWindowDragging = ZOOM_NO;
	      break;
	    case FL_DRAG:
	      zwd_x2 = get_event_x();
	      zwd_y2 = get_event_y();
	      redraw_overlay();
	      break;
	    }
	  break;
	case 2:
	  if(zoom_window == NULL)
	    return 1;
	  switch(event)
	    {
	    case FL_PUSH:
	      {
		int mx1 = get_event_x();
		int my1 = get_event_y();
		zoom_window->getSourceArea(zwd_x1,zwd_y1,zwd_x2,zwd_y2);
		zoomWindowDragging = ZOOM_RESIZE_NW;
		double distance = distance_squared(mx1,my1, zwd_x1,zwd_y1);
		tryZoomCorner(mx1,my1,zwd_x1,zwd_y2,ZOOM_RESIZE_SW,distance);
		tryZoomCorner(mx1,my1,zwd_x2,zwd_y1,ZOOM_RESIZE_NE,distance);
		tryZoomCorner(mx1,my1,zwd_x2,zwd_y2,ZOOM_RESIZE_SE,distance);
	      } // Warning!  Falling case!              
	    case FL_DRAG:
	      {
		switch(zoomWindowDragging)
		  {
		  case ZOOM_RESIZE_NW:  zwd_x1 = get_event_x(); zwd_y1 = get_event_y(); break;
		  case ZOOM_RESIZE_SW:  zwd_x1 = get_event_x(); zwd_y2 = get_event_y(); break;
		  case ZOOM_RESIZE_NE:  zwd_x2 = get_event_x(); zwd_y1 = get_event_y(); break;
		  case ZOOM_RESIZE_SE:  zwd_x2 = get_event_x(); zwd_y2 = get_event_y(); break;
		  case ZOOM_NO: case ZOOM_DRAG: info("Unusual event; hard to get here.\n"); break;
		  }
		redraw_overlay();
		return 1;
	      }		  
	    case FL_RELEASE:
	      switch(zoomWindowDragging)
		{
		case ZOOM_RESIZE_NW:  zwd_x1 = get_event_x(); zwd_y1 = get_event_y(); break;
		case ZOOM_RESIZE_SW:  zwd_x1 = get_event_x(); zwd_y2 = get_event_y(); break;
		case ZOOM_RESIZE_NE:  zwd_x2 = get_event_x(); zwd_y1 = get_event_y(); break;
		case ZOOM_RESIZE_SE:  zwd_x2 = get_event_x(); zwd_y2 = get_event_y(); break;
		case ZOOM_NO: case ZOOM_DRAG: info("Unusual event; hard to get here.\n"); break;
		}
	      zoom_window->center_x = (zwd_x1+zwd_x2)/2;
	      zoom_window->center_y = (zwd_y1+zwd_y2)/2;
	      zoom_window->size((abs(zwd_x2-zwd_x1))*zoom_window->xscale, (abs(zwd_y2-zwd_y1))*zoom_window->yscale);
	      showZoomWindow();
	      zoomWindowDragging = ZOOM_NO;
	      zoom_window->zoomMoved();
	      return 1;
	    }
	  break;
	case 3:
	  showZoomWindow();
	  zoom_window->center_x = get_event_x();
	  zoom_window->center_y = get_event_y();
	  zoom_window->zoomMoved();
	  return 1;
	default: 
	  return Fl_Overlay_Window::handle(event);
	}
      else if (ctrl(state))  // arb LC
	switch (button)
	  {
	  case 1:
	    if (event == FL_PUSH)
	      {
		lcx1 = lcx2 = get_event_x();
		lcy1 = lcy2 = get_event_y();
		line_cut_type = OTHERLINE;
		redraw_overlay();
		return 1;
	      }
	    else //if (event == FL_DRAG) // Button 1 + control + drag (arb line cut)
	      {
		lcx2 = get_event_x();
		lcy2 = get_event_y();
		if (lcx2 < 0) lcx2 = 0;
		if (lcy2 < 0) lcy2 = 0;
		if (lcx2 > w-1) lcx2 = w-1;
		if (lcy2 > h-1) lcy2 = h-1;
		plotLineCut();
		redraw_overlay();
		return 1;
	      }
	  case 2:
	    static int pointnum = 0;
	    int nx, ny;
	    nx = get_event_x();
	    ny = get_event_y();
	    if (nx < 0) nx = 0;
	    if (ny < 0) ny = 0;
	    if (nx > w-1) nx = w-1;
	    if (ny > h-1) ny = h-1;
	    d1 = sqrt(static_cast<double>((nx-lcx1)*(nx-lcx1)+(ny-lcy1)*(ny-lcy1)));
	    d2 = sqrt(static_cast<double>((nx-lcx2)*(nx-lcx2)+(ny-lcy2)*(ny-lcy2)));
	    if (event == FL_PUSH)
	      {
		if (d1 < d2)
		  { lcx1 = nx; lcy1 = ny; pointnum = 1;}
		else
		  { lcx2 = nx; lcy2 = ny; pointnum = 2;}
	      }
	    else if (event == FL_DRAG)
	      {
		if (pointnum == 1)
		  { lcx1 = nx; lcy1 = ny; }
		else if (pointnum == 2)
		  { lcx2 = nx; lcy2 = ny; }
	      }
	    plotLineCut();
	    redraw_overlay();
	    return 1;
	  case 3:
	    static int x0, y0, x1, y1; // Mouse coord at push and then at drag
	    static int lcx1i, lcy1i, lcx2i, lcy2i; // Initial line endpoint on the push
	    int dx, dy; // displacments
	    if (event == FL_PUSH) // Pushing button 3 + control (arb line cut)
	      {
		if(!(line_cut_limit & OTHERLINE))
		  return 1;
		x0 = get_event_x();
		y0 = get_event_y();
		lcx1i = lcx1;
		lcx2i = lcx2;
		lcy1i = lcy1;
		lcy2i = lcy2;
		line_cut_type = OTHERLINE;
		return 1;
	      }
	    else if (event == FL_DRAG) // Dragging button 3 + control (arb line cut)
	      {
		x1 = get_event_x();
		y1 = get_event_y();
		dx = x1-x0;
		dy = y1-y0;
		if (lcx1i+dx < 0) dx = -lcx1i;
		if (lcx1i+dx > w-1) dx = w-1-lcx1i;
		if (lcx2i+dx < 0) dx = -lcx2i;
		if (lcx2i+dx > w-1) dx = w-1-lcx2i;
		if (lcy1i+dy < 0) dy = -lcy1i;
		if (lcy1i+dy > h-1) dy = h-1-lcy1i;
		if (lcy2i+dy < 0) dy = -lcy2i;
		if (lcy2i+dy > h-1) dy = h-1-lcy2i;
		if (dx == 0 && dy == 0 && line_cut_type == OTHERLINE) return 1;
		lcx1 = lcx1i + dx; lcx2 = lcx2i + dx;
		lcy1 = lcy1i + dy; lcy2 = lcy2i + dy;
		line_cut_type = OTHERLINE;
		plotLineCut();
		redraw_overlay();
		return 1;
	      }
	  default: 
	    return Fl_Overlay_Window::handle(event);
	  }
      // no modifiers, or multiple modifiers...
      if (button == 3 && controls_window != NULL && event == FL_PUSH)
	{
	  if (controls_window->visible())
	    controls_window->hide();
	  else
	    controls_window->show();
	  return 1;
	}
      break;
    }
  return 0;
}

double ImageWindow::dataval(int x, int y)
{
  if (x>=0 && x<w && y>=0 && y<h)
    return id.raw(x,y) - ((plane) ? id.quant_to_raw(planeval(y,x)) : 0); // watchout: planeval is swapped i,j!
  else 
    return 0;
}
  

static void swap_crossection(const char *xsection_fn, const char *xsection_tmp_fn)
{
#ifdef WIN32
  if(unlink(xsection_fn) != 0)
    {
      info("Error deleting temporary file \"%s\": %s\n", xsection_fn,strerror(errno));
      // Sometimes, win32 seems to have trouble delete files: probably due to stupid locking...
      // In this case, we will try again with a new filename
//       int n=0;
//       while (true)
// 	{
// 	  make_tmpfiles();
// 	  info("Trying new temporary filename %s", xsection_fn);
// 	  if (fopen(xsection_fn, "w") != NULL)
// 	    return;
// 	  sleep(5);
// 	  n++;
// 	  if (n==10)
// 	    {
// 	      info("giving up on xsection file!");
// 	      return;
// 	    }
    }
#endif
  if(rename(xsection_tmp_fn, xsection_fn) != 0)
    {
      info("Error renaming temporary \"%s\" to \"%s\": %s\n",
	   xsection_tmp_fn, xsection_fn, strerror(errno));
      return;
    }
}



void ImageWindow::plotLineCut(bool nothrottle)
{
  static OptThrottle<ImageWindow> throttle(this,&ImageWindow::plotLineCut);
  if(!(nothrottle || throttle.throttle()))
    return;

  FILE *fp;
  if (line_cut_type == NOLINE)
    {
      if (gplinecut.isopen())
	{
	  // set term x11 close doesn't work if the terminal type is not wxt.
	  // Newer versions of gnuplot default to wxt.
	  // Safer to kill gnuplot; of course, this "forgets" where the window was.
	  gplinecut.close();
	}
      return;
    }
  
  if (!gplinecut.isopen())
    {
      gplinecut.open();
      if(!gplinecut.open())
	{
	  nognuplot();
	  return;
	}
      gplinecut.cmd("set style data %s\n", gp_with_string.c_str());
    }

  if ((fp = fopen(xsection_fn, "w")) == NULL)
    {
      error("Error opening file \"%s\": %s\n", xsection_tmp_fn, strerror(errno));
      
      return;
    }
  unlink("xsection.dat"); 
#ifdef HAVE_SYMLINK
// just so that we always have a file in the current dir where we can easily access the data
  if(symlink(xsection_fn, "xsection.dat") != 0) 
    fprintf(stderr,"Error creating xsection.dat symlink: %s\n",strerror(errno));
#endif

  // OK, let's give this a real cleanup.
  // We will plot the real, unquantized raw data, with as little permutation as possible.  

  double xstep;
  double ystep;
  string axname;
  int x1,x2,y1,y2;
  double ax1, ax2;

  int npeaks = 0;
  int  nvalleys = 0;
  double peak, valley;

  if (line_cut_type == HORZLINE)
    {
      int j = line_cut_yp;
      for (int i=0; i < w; i++)
	{
	  fprintf(fp, "%e %e ", id.getX(i), dataval(i,j));
	  if (pf->peaks != NULL && pfc.plot_peaks->value() && pf->peaks[j*w+i] == 1)
	    {
	      npeaks++;
	      fprintf(fp, "%e ",  dataval(i,j));
	    }
	  else fprintf(fp, "none ");
	  if (pf->peaks != NULL && pfc.plot_valleys->value() && pf->peaks[j*w+i] == -1)
	    {
	      nvalleys++;
	      fprintf(fp, "%e\n",  dataval(i,j));
	    }
	  else fprintf(fp, "none\n");
	}
      x1 = 0; x2 = w; 
      y1 = y2 = line_cut_yp;
      axname = id.xname;
      ax1 = id.getX(x1);
      ax2 = id.getX(x2);
    }
  else if (line_cut_type == VERTLINE)
    { 
      int i = line_cut_xp;
      for (int j=0; j < h; j++)
	{
	  fprintf(fp, "%e %e ", id.getY(j), dataval(i,j));
	  if (pf->peaks != NULL && pfc.plot_peaks->value() && pf->peaks[j*w+i] == 1)
	    {
	      npeaks++;
	      fprintf(fp, "%e ",  dataval(i,j));
	    }
	  else fprintf(fp, "none ");
	  if (pf->peaks != NULL && pfc.plot_valleys->value() && pf->peaks[j*w+i] == -1)
	    {
	      nvalleys++;
	      fprintf(fp, "%e\n",  dataval(i,j));
	    }
	  else fprintf(fp, "none\n");
	}
      x1 = x2 = line_cut_xp; 
      y1 = h; y2 = 0; 
      axname= id.yname;
      ax1 = id.getY(y1);
      ax2 = id.getY(y2);
    }
  else // otherwise, we'll use bilinear interpolation
    { 
      int num_steps, n;
      x1 = lcx1; x2 = lcx2; 
      y1 = lcy1; y2 = lcy2; 
      
      if (x1 == x2 && y1 == y2) return;

      double i,j;
      double i1,j1;
      double i2,j2;
      double d1,d2,d;
      double x;
      bool step_x;
      
      if (abs(x2-x1) > abs(y2-y1))
	{ 
	  step_x = true;
	  num_steps = abs(x2-x1); 
	  xstep = 1.0*(x2-x1)/num_steps; 
	  ystep = 1.0*(y2-y1)/num_steps;  
	}
      else 
	{ 
	  step_x = false;
	  num_steps = abs(y2-y1); 
	  ystep = 1.0*(y2-y1)/num_steps; 
	  xstep = 1.0*(x2-x1)/num_steps; 
	}

      i = x1; j = y1;
      for (n=0; n<num_steps; n++)
	{
	  i1 = floor(i);
	  i2 = ceil(i);
	  j1 = floor(j);
	  j2 = ceil(j);
	  if (i1 == i2 && j1 == j2)
	    d = dataval(i1,j1);
	  else if (step_x)
	    {
	      d1 = dataval(i1, j1);
	      d2 = dataval(i1, j2);
	      d = d1 + (d2-d1)*(j-j1);
	    }
	  else 
	    {
	      d1 = dataval(i1, j1);
	      d2 = dataval(i2, j1);
	      d = d1 + (d2-d1)*(i-i1);
	    }
	  if (lc_axis == XAXIS) x = id.getX(i);
	  else if (lc_axis == YAXIS) x = id.getY(j);
	  else
	    {
	      d1 = id.getX(i)-id.getX(x1);
	      d2 = id.getY(j)-id.getY(y1);
	      x = sqrt(d1*d1+d2*d2);
	    }
	  if (n == 0) ax1 = x;
	  if (n == num_steps-1) ax2 = x;
	  fprintf(fp, "%e %e\n", (double) x, d);
	  i+=xstep; j+=ystep;
	}
      if (lc_axis == XAXIS) axname = id.xname; 
      else if (lc_axis == YAXIS) axname = id.yname; 
      else axname = "Distance (" + id.xname + ")"; 
    }

  fprintf(fp, 
	  "#X axis: %s\n"
	  "#Line cut: %s %e to %e, %s %e to %e';\n"
	  "#Row %d %d Column %d %d'\n",
	  axname.c_str(), 
	  id.xname.c_str(), id.getX(x1), id.getX(x2), 
	  id.yname.c_str(), id.getY(y1), id.getY(y2),
	  x1, x2, y1, y2);
	  

  fclose(fp);
  //swap_crossection(xsection_fn, xsection_tmp_fn);
  
  //warn( "%e %e\n", ax1, ax2);

  if (line_cut_xauto)
    gplinecut.cmd("set xrange [%e:%e]\n",ax1, ax2);

  for(modules_t::iterator i = modules.begin(); i != modules.end(); i++)
    (*i)->linecut_callback(true);

  char buf[1024];
  gplinecut.cmd("set xlabel '%s';\n"
		"set ylabel '%s';\n"
		"set title \"%s \\n Line cut: %s %g to %g, %s %g to %g\";\n"
		"plot '%s' u %s w %s t 'x %d %d y %d %d'",
		axname.c_str(), 
		id.zname.c_str(),
		Gnuplot_Interface::escape(filename).c_str(), 
		id.xname.c_str(), id.getX(x1), id.getX(x2), 
		id.yname.c_str(), id.getY(y1), id.getY(y2),
		xsection_fn, gp_using_string, gp_with_string.c_str(),
		x1, x2, y1, y2);
  if (npeaks != 0)
    gplinecut.cmd(", '' u 1:3 w p pt 5 t 'Peaks'");
  if (nvalleys != 0)
    gplinecut.cmd(", '' u 1:4 w p pt 5 t 'Valleys'");
  

  for(modules_t::iterator i = modules.begin(); i != modules.end(); i++)
    (*i)->linecut_callback(false);

  gplinecut.cmd("\n");
}

void ImageWindow::plotCmap()
{
  static Throttle<ImageWindow> throttle(this,&ImageWindow::plotCmap);
  if(!throttle.throttle())
    return;
  int i,j;
  if (plot_cmap)
    {
      if (!gpcmap.isopen())
	{
	  if(!gpcmap.open())
	    {
	      nognuplot();
	      return;
	    }
	}
      FILE *fp = fopen(cmap_fn, "w");
      if (fp == NULL)
	{
	  error("Error opening file \"%s\": %s\n", cmap_fn, strerror(errno));
	  return;
	}
      unlink("cmap.dat"); 
#ifdef HAVE_SYMLINK
      symlink(cmap_fn, "cmap.dat");
#endif
      for (i = 0; i < colormap_length; i++)
	{
	  j = gammatable[i];
	  fprintf(fp, "%d %d %d %d\n", i ,colormap[3*j], colormap[3*j+1], colormap[3*j+2]);
	}
      fclose(fp);
      gpcmap.cmd("set xrange [0:%d]; set yrange [0:255];"
		 "set data style linespoints; set nokey;\n", colormap_length);
      gpcmap.cmd("plot '%s' u 1:2, '' u 1:3, '' u 1:4\n",cmap_fn);
    }
  else if (gpcmap.isopen())
    gpcmap.close();
}

void ImageWindow::plotHist()
{
  static Throttle<ImageWindow> throttle(this,&ImageWindow::plotHist);
  if(!throttle.throttle())
    return;
  int i, min, max, inc, bintotal;
  if (plot_hist)
    {
      if(!gphist.isopen())
	{
	  if(!gphist.open())
	    {
	      nognuplot();
	      return;
	    }
	}
      FILE *fp = fopen(hist_fn, "w");
      if (fp == NULL)
	{
	  error("Error opening file \"%s\": %s\n", hist_fn, strerror(errno));
	  return;
	}
      unlink("hist.dat"); 
#ifdef HAVE_SYMLINK
      symlink(hist_fn, "hist.dat");
#endif
      min = (hmin < datamin) ? hmin : datamin; 
      max = (hmax > datamax) ? hmax : datamax;
      inc = (datamax - datamin)/300; 
      if (inc < 1) inc = 1;
      bintotal=0;
      for (i = min ; i < max+inc && i < 65535 ; i++)
	{
	  bintotal += datahist[i];
	  if ( (i%inc) == 0)
	    {
	      fprintf(fp, "%d %e %d %d\n",i, id.quant_to_raw(i), bintotal, 
		      ((hmax-inc>=i && hmax-inc<=(i+inc-1)) ? 1 : 0) + 
		      ((hmin+inc>=i && hmin+inc<=(i+inc-1)) ? 1 : 0));
	      bintotal = 0;
	    }
	}
      fclose(fp);
      gphist.cmd("set x2range [%d:%d]; set xrange [%e:%e]; "
		 "set x2tics; set yrange [1e-1:*];"
		 "set style data boxes; set style fill solid;\n"
		 "set nokey; set title '%s';"
		 "set xlabel '%s'; set x2label 'Gray Value'\n", 
		 min-3*inc, max+3*inc, id.quant_to_raw(min-3*inc), id.quant_to_raw(max+3*inc), hist_fn, id.zname.c_str());
      gphist.cmd("plot '%s' u 1:4 ax x2y2 lt 2, '' u 1:3 ax x2y1 lt 1\n", hist_fn);
    }
  else if (gphist.isopen())
    {
      gphist.close();
    }
}
      
void ImageWindow::setMin(int m) 
{
  if (m > LMAX) m = LMAX;
  if (m < 0) m = 0;
  hmin = m;
  if (hmax < hmin) 
    hmin = hmax;
}

void ImageWindow::setMax(int m) 
{
  if (m > LMAX) m = LMAX;
  if (m < 0) m = 0;
  hmax = m;
  if (hmin > hmax)
    hmax = hmin;
}

void ImageWindow::allocateImage()
{
  int newsize=(dozoom(w,xzoom))*(dozoom(h,yzoom))*3;
  zap(image);
  image = new uchar [newsize];
}

  
void ImageWindow::setXZoom(int xz)
{
  if (xz == 0 || xz == -1)
    {
      if (xzoom > xz) xzoom = -2;
      else xzoom = 1;
    }
  else xzoom = xz;

  allocateImage();
  size(dozoom(w,xzoom), dozoom(h,yzoom)); 
}

void ImageWindow::setYZoom(int yz)
{
  if (yz == 0 || yz == -1)
    {
      if (yzoom > yz) yzoom = -2;
      else yzoom = 1;
    }
  else yzoom = yz;

  allocateImage();
  size(dozoom(w,xzoom), dozoom(h,yzoom));
}

void ImageWindow::setColormap(uchar *cmap, int l)
{
  zap(colormap);
  zap(gammatable);
  colormap = new uchar[3*l];
  gammatable = new int[l];
  colormap_length = l;

  
  int min_index = cmap_min * l;
  int max_index = cmap_max * l;
  //warn( "min %f max %f\n", cmap_min, cmap_max);
  //warn( "min index %d max index %d\n", min_index, max_index);

  int i,j;
  double r,g,b,h,s,v;
  for (j=0; j < l; j++)
    {
      if (j < min_index) i = min_index;
      else if (j > max_index) i = max_index;
      else i = j;

      r = cmap[3*i];
      g = cmap[3*i+1];
      b = cmap[3*i+2];

      // This was kindof fun, but not all that useful...
      if(fabs(colormap_rotation_angle) > 1e-3)
	{
	  Fl_Color_Chooser::rgb2hsv(r,g,b,h,s,v);
	  h = (h+(colormap_rotation_angle+0.001)/360.0*6.0);  // avoid singularity at angles of 60 degrees (problem on win32)
	  h = h - 6.0*floor(h/6.0);
	  Fl_Color_Chooser::hsv2rgb(h,s,v,r,g,b);
	}
      
      // Let's move all of this crap into the colormap loading routine,
      // where it really belongs. This is really, really overdue.
      if (negate) i = (colormap_length-1) - j;
      else i = j;
      
      colormap[3*i] = (int) (invert) ? 255-r : r;
      colormap[3*i+1] = (int) (invert) ? 255-g : g;
      colormap[3*i+2] = (int) (invert) ? 255-b : b;
    }

  setGamma(gam, gcenter);
  adjustHistogram();
  plotCmap();
  colormap_window->update();
}

void ImageWindow::setGamma(double g, double gc) 
{
  // Build a lookup table that maps our data 
  gam = g;
  gcenter = gc;
  
  // problem near 1.0...  in the end, for some reason this didn't
  // always work (was still crashing in win32), so i configured the
  // spyview gui so that you cannot set gc greater that 0.999
  //if (gc > 0.999) gc = 0.999;

  // function:
  //  f3(x,a,b) = b - (x<=b) * b * ((b-x)/b)**(a)  + (x>=b) * (1-b) * ((x-b)/(1-b))**(a)

  double x,val;
  for (int i = 0; i<colormap_length; i++)
    {
      x = 1.0*i/(colormap_length-1)+1e-9;
      val = gc + ((x<=gc) ? (-gc) * pow(((gc-x)/gc), g) : (1-gc) * pow((x-gc)/(1-gc), g));
      if (val > 1.0) val = 1.0;
      if (val < 0.0) val = 0.0;
      int index = round((val * (colormap_length-1)));
      if (index < 0) index = 0;
      if (index >= colormap_length) index = colormap_length-1;
      gammatable[i] = index;
      //if (i==0 || i == colormap_length-1)
      //info( "%03d %.3f %07.3f %03d ", i,val, val*(colormap_length-1),  gammatable[i]);
    }
  //info( "\n");
  
  calculateHistogram();
  adjustHistogram();
  redraw();
  colormap_window->update();
}

// Calculate the data's histogram
void ImageWindow::calculateHistogram()
{
  int d;

  // Zero the histogram table
  for (int i = 0; i <= LMAX; i++)
    datahist[i] = 0;
  
  //warn( "data[2] = %d\n",data[2]);
  for (int i=0; i<w*h; i++)    
    {
      d = data[i] - planeval(i/w,i%w);
      if (d > LMAX)
	d = LMAX;
      if (d < 0) 
	d = 0;
      ++datahist[d];
    }
  for (datamin = 0; datamin <= LMAX; datamin++)
    if (datahist[datamin]>0) break;
  for (datamax = LMAX; datamax >= 0; datamax--)
    if (datahist[datamax]>0) break;
}

// Adjust the lookup table for the image histogram mapping
void ImageWindow::adjustHistogram() 
{
  int i;
  double s; // Scale factor that takes us from hmin-hmax to 0...1
  double w; // Width of output
  for (i = 0; i < hmin; i++)
    imagehist[i] = 0;
  for (i = hmax+1; i <= LMAX; i++)
    imagehist[i] = LMAX;
  if (hmin == hmax)
    {
      if (hmin == 0) hmax++;
      else hmin--;
      external_update();
    }		    
  s = 1.0/(hmax-hmin);
  w = LMAX;;

  // I forgot that the gamma mapping is actually applied here on the 16 bit data!!!

  for (i = hmin; i <= hmax; i++)
    {
      double d = (i-hmin)*s + 1e-9; // Put ourselves in the range 0...1
      //d = pow(d,gam); // Apply our gamma correction
      d = gcenter + ((d<=gcenter) ? 
		     (-gcenter) * pow(((gcenter-d)/gcenter), gam) : 
		     (1-gcenter) * pow((d-gcenter)/(1-gcenter), gam));
      imagehist[i] = static_cast<int>(round(d*w));
    }

  // Drawing the window when it wasn't shown resulted in a flaky window manager placement problem!
  if (shown())
    redraw();
}

void ImageWindow::normalize() 
{
  int new_hmax, new_hmin;
  int nwhite, nblack;
  nblack = nwhite = 0;
  calculateHistogram();
  for ( new_hmin = 0; new_hmin <= LMAX; new_hmin++)
    {
      nblack += datahist[new_hmin];
      if (nblack > bpercent*w*h/100) break;
    }
  for ( new_hmax = LMAX; new_hmax >= 0; new_hmax--)
    {
      nwhite += datahist[new_hmax];
      if (nwhite > wpercent*w*h/100) break;
    }
  //setGamma(1.0);
  setMin(new_hmin);
  setMax(new_hmax);
  adjustHistogram();
}

void ImageWindow::normalizeOnZoom() 
{
  if(!zoom_window)
    return;
  int new_hmax, new_hmin;
  int nwhite, nblack;
  int x1, y1, x2, y2;
  zoom_window->getSourceArea(x1,y1,x2,y2);
  int s = (x2-x1)*(y2-y1);
  nblack = nwhite = 0;
  zoom_window->calculateHistogram();
  for ( new_hmin = 0; new_hmin <= LMAX; new_hmin++)
    {
      nblack += zoom_window->histogram[new_hmin];
      if (nblack > bpercent*s/100) break;
    }
  for ( new_hmax = LMAX; new_hmax >= 0; new_hmax--)
    {
      nwhite += zoom_window->histogram[new_hmax];
      if (nwhite > wpercent*s/100) break;
    }
  //setGamma(1.0);
  setMin(new_hmin);
  setMax(new_hmax);
  adjustHistogram();
}

void ImageWindow::runQueue()
{
  bool swap_zoom = 0;

  operations_string = "(";

  if (process_queue != NULL)
    {
      for (int i=1; i<=process_queue->size(); i++)
        {
	  Image_Operation *op = (Image_Operation *) process_queue->data(i);

	  assert(op);

	  if(!op->enabled)
	    continue;

	  if (i!=1)
	    operations_string += ";" ;
	  operations_string += op->name;
	  
 	  for (int n=0; n<op->num_parameters; n++)
	    {
	      operations_string += "-";
	      ostringstream os;
	      os << op->parameters[n].value;
	      operations_string += os.str();
	    }

          if (op->name == "sub fitplane") 
            id.fitplane(op->parameters[0].value, op->parameters[1].value, op->parameters[3].value);
          else if (op->name == "shift data") 
            id.shift_data(op->parameters[0].value, op->parameters[1].value);
          else if (op->name == "remove lines") 
            id.remove_lines(op->parameters[0].value, op->parameters[1].value);
	  else if (op->name == "sub linecut")
	    id.sub_linecut(op->parameters[1].value, op->parameters[0].value);
	  else if (op->name == "outlier")
	    id.outlier_line(op->parameters[1].value, op->parameters[0].value);
	  else if (op->name == "scale axes")
	    id.scale_axes(op->parameters[0].value, op->parameters[1].value);
	  else if (op->name == "offset axes")
	    id.offset_axes(op->parameters[0].value, op->parameters[1].value);
          else if (op->name == "sub plane") 
            id.plane(op->parameters[0].value, op->parameters[1].value);
          else if (op->name == "sub lbl") 
            id.lbl(op->parameters[0].value, op->parameters[1].value, 0, 1, op->parameters[2].value, op->parameters[3].value);
          else if (op->name == "sub cbc")
            id.cbc(op->parameters[0].value, op->parameters[1].value, 0, 1, op->parameters[2].value, op->parameters[3].value);
	  else if (op->name == "power")
	    id.gamma(op->parameters[0].value,op->parameters[1].value);
	  else if (op->name == "power2")
	    id.power2(op->parameters[0].value);
	  else if (op->name == "scale data")
	    id.scale(op->parameters[0].value);
	  else if (op->name == "even odd")
	    id.even_odd(op->parameters[0].value, op->parameters[1].value);
	  else if (op->name == "rm switch")
	    id.switch_finder(op->parameters[0].value, op->parameters[1].value, false);
	  else if (op->name == "offset")
	    id.offset(op->parameters[0].value, op->parameters[1].value);
          else if (op->name == "norm lbl") 
            id.norm_lbl();
          //else if (op->name == "square") 
	  //id.square();
          else if (op->name == "norm cbc") 
            id.norm_cbc();
          else if (op->name == "log") 
            id.log10(op->parameters[0].value, op->parameters[1].value);
          else if (op->name == "interp") 
            id.interpolate(op->parameters[0].value, op->parameters[1].value);
          else if (op->name == "scale img") 
            id.scale_image(op->parameters[0].value, op->parameters[1].value);
          else if (op->name == "abs") 
            id.magnitude();
          else if (op->name == "neg") 
            id.neg();
	  else if (op->name == "hist2d")
	    id.hist2d(op->parameters[0].value, op->parameters[1].value, op->parameters[2].value);
	  else if (op->name == "vi_to_iv")
	    id.vi_to_iv(op->parameters[0].value, op->parameters[1].value, op->parameters[2].value);
          else if (op->name == "xderiv")
            id.xderv();
          else if (op->name == "yderiv")
            id.yderv();
          else if (op->name == "ederiv")
	    id.ederv(op->parameters[0].value,op->parameters[1].value);
	  else if (op->name == "dderiv")
	    id.dderv(op->parameters[0].value);
	  else if (op->name == "gradmag")
	    id.grad_mag(op->parameters[0].value);
	  else if (op->name == "lowpass")
	    id.lowpass(op->parameters[0].value, op->parameters[1].value,(ImageData::lowpass_kernel_t)op->parameters[2].value);
	  else if (op->name == "highpass")
 	    id.highpass(op->parameters[0].value, op->parameters[1].value, op->parameters[2].value / 100.0, (ImageData::lowpass_kernel_t)op->parameters[3].value);
 	  else if (op->name == "notch")
 	    id.notch(op->parameters[0].value, op->parameters[1].value, op->parameters[2].value, op->parameters[3].value);
 	  else if (op->name == "crop")
 	    id.crop(op->parameters[0].value, op->parameters[1].value, op->parameters[3].value, op->parameters[2].value);
 	  else if(op->name == "despeckle")
 	    id.despeckle(op->parameters[0].value, op->parameters[1].value);
	  else if(op->name == "flip")
	    {
	      if(op->parameters[0].value)
		id.xflip();
	      if(op->parameters[1].value)
		id.yflip();
	    }
	  else if(op->name == "flip endpoints")
	    id.flip_endpoints(op->parameters[0].value, op->parameters[1].value);
	  else if (op->name == "autoflip")
	    {
	      if (id.xmin > id.xmax)
		id.xflip();
	      if (id.ymin > id.ymax)
		id.yflip();
	    }
	  else if (op->name == "pixel avg")
	    id.pixel_average(op->parameters[0].value, op->parameters[1].value);	  
	  else if(op->name == "rotate cw")
	    {
	      id.rotate_cw();
	      swap_zoom = !swap_zoom;
	    }
	  else if(op->name == "rotate ccw")
	    {
	      id.rotate_ccw();
	      swap_zoom = !swap_zoom;
	    }
          else if (op->name == "equalize") 
            id.equalize();
	  else
	    warn("Warning: unknown operation \"%s\"\n",process_queue->text(i));
        }
    }
  
  //info( "op string %s\n", operations_string.c_str());
  operations_string += ")";
  if (process_queue->size() == 0)
    operations_string = "";

  //info( "op string2 %s\n", operations_string.c_str());
  //warn( "swap_zoom %d zoom_is_swapped %d\n", swap_zoom, zoom_is_swapped);

  //info("op string2 %s\n", operations_string.c_str());
  //info("zname %s\n", id.zname.c_str());

  // We used to do id.zname = id.zname + operation_string, but this
  // didn't work for the MTX data derived from meta.txt files? I
  // coudn't figure it out, so we'll just use old-fashioned c
  // functions...
 
  char tmp[4096];
  snprintf(tmp, sizeof(tmp), "%s %s", id.zname.c_str(), operations_string.c_str());
  //  info("sum test: %s\n", tmp);
  id.zname = tmp;
  //info( "zname %s\n", id.zname.c_str());

  if (swap_zoom != swap_zoom_state)
    {
      int tmp = xzoom;
      xzoom = yzoom;
      yzoom = tmp;
    }
  swap_zoom_state = swap_zoom;

  // We also need to recalculate the peaks. If they're not displayed, it's no
  // biggy, since it doesn't take too much time.
  
  // For some reason, this is reproducibly generating a segfault on
  // win32?  with some specific input files (actually, in particular,
  // a really big input file of 600x3600)

  // However, I think the problem is deeper than this, since we were also
  // getting random segfaults in win32 before peakfinder. 

  // Running in gdb, it is segfaulting when calling iw->dataval

  // Actually, I just realized that pf calls iw->dataval, which is no
  // good since if this function is called from loadData, we may not
  // have updated the iw width and height properly yet.

  pf->calculate();
  external_update();
}

// a little procedure
void ImageWindow::adjust_window_size()
{
  if (window_size_action == KEEPSIZE)
    {
      xzoom = 0; yzoom = 0;
      size(((Fl_Widget *) this)->w(), ((Fl_Widget *)this)->h());
    }
  else if (window_size_action == KEEPZOOM)
    setXZoom(xzoom); // call this to allocate the image array and set the window size.
  else // if (window_size_action == RESETZOOM)
    {
      xzoom = 1; yzoom = 1;
      setXZoom(xzoom);
    }
}

int ImageWindow::loadData(const char *name) 
{
  // This is now completely rewritten!
  filename = name;
  if (id.load_file(name) == -1) return -1;
  if (id.width == 0 || id.height == 0) return -1;
  if ((id.width>id.height) && (id.width%id.height == 0) && square) 
    id.pixel_average(id.width/id.height, 1);
  original_dataname = id.zname;
  runQueue();
  id.quantize();
  data = id.quant_data;

  w = id.width;
  h = id.height;

  zap(databuf);
  databuf = new int [w*h];

  adjust_window_size();

  calculateHistogram();
  adjustHistogram();
  plotLineCut();
  plotHist();
  
  // Set a nice basename
  char *p;
  strncpy(output_basename, filename.c_str(), 256);
  if ((p = strstr(output_basename, ".pgm")) == 0)
    if ((p = strstr(output_basename, ".Stm")) == 0)
      if ((p = strstr(output_basename, ".mtx")) == 0)
	if ((p = strstr(output_basename, ".dat")) == 0)
	  p = strchr(output_basename, 0);
  *p = 0;
  
  external_update();
  return 0;
}

void ImageWindow::loadData(int *newdata, int neww, int newh, const char *name, bool reset_units)
{
  filename = "imagedata";
  // Keep these around: if the image doesn't change size, we won't bother reallocating the arrays.
  int oldw = w;
  int oldh = h;

  w = neww;
  h = newh;

  id.load_int(newdata, neww, newh);
  if ((id.width>id.height) && (id.width%id.height == 0) && square) 
    id.pixel_average(w/h, 1);
  runQueue();
  id.quantize();
  data = id.quant_data;

  w = neww = id.width;
  h = newh = id.height;

  if ( oldw*oldh != neww*newh )
    {
      zap(databuf);
      databuf = new int [w*h];
    }

  adjust_window_size();

  calculateHistogram();
  adjustHistogram();
  plotLineCut();
  plotHist();
  sprintf(output_basename, name);
  external_update();
}

void ImageWindow::reRunQueue()
{
  int oldw = w;
  int oldh = h;

  // This will copy the original data back into the raw data matrix
  id.reset();
  if ((id.width>id.height) && (id.width%id.height == 0) && square) 
    id.pixel_average(id.width/id.height, 1);
  runQueue();
  id.quantize();
  data = id.quant_data;
  
  w = id.width;
  h = id.height;

  // since we will likely often be loading a dataset of the same dimesions
  if ( oldw*oldh != w*h )
    {
      zap(databuf);
      databuf = new int [w*h];
    }

  setXZoom(xzoom); // this will allocate the image array

  calculateHistogram();
  adjustHistogram();
  plotLineCut();
  plotHist();
}

void ImageWindow::load_mtx_cut(int index, mtxcut_t type)
{
  int oldw = w;
  int oldh = h;

  type = (mtxcut_t)(type %3);
  
  if (!id.data3d)
    {
      info( "3D data not loaded!\n");
      return;
    }
  
  id.load_mtx_cut(index, type);
  original_dataname = id.zname;
  runQueue();
  id.quantize();
  data = id.quant_data;
  
  w = id.width;
  h = id.height;

  // since we will likely often be loading a dataset of the same dimesions
  if ( oldw*oldh != w*h )
    {
      zap(databuf);
      databuf = new int [w*h];
    }

  setXZoom(xzoom); // this will allocate the image array

  calculateHistogram();
  adjustHistogram();
  plotLineCut();
  plotHist();
  external_update();
}


void ImageWindow::saveFile() 
{
  char buf[256];
  snprintf(buf, 256, "%s.ppm", output_basename);
  FILE *fp = fopen(buf, "wb");
  if(fp == NULL)
    {
      warn("Unable to open file \"%s\": %s\n",
	   buf, strerror(errno));
      return;
    }
  fprintf(fp, 
	  "P6\n%d %d\n"
	  "#hmin %d %e %s\n"
	  "#hmax %d %e %s\n"
	  "#hwidth %d %e %s\n"
	  "#plane_a %e\n"
	  "#plane_b %e\n"
	  "#gamma %e\n"
	  "#Image processing: %s\n"
	  "255\n", w, h, 
	  hmin, id.quant_to_raw(hmin), id.zname.c_str(),
	  hmax, id.quant_to_raw(hmax), id.zname.c_str(),
	  hmax-hmin, id.quant_to_raw(hmax-hmin), id.zname.c_str(),
	  plane_a, plane_b, gam, operations_string.c_str());

  unsigned char r,g,b;
  for (int i = 0; i<w*h; i++)
    {
      makergb(data[i],r,g,b,i/w,i%w);
      fwrite(&r, 1, 1, fp);
      fwrite(&g, 1, 1, fp);
      fwrite(&b, 1, 1, fp);
    }
	
  fclose(fp);
}

void ImageWindow::fit_plane() //calculates plane_a, plane_b, and plane_c
{
  // Formula for the plane: Z = a*X + b*Y + c
  double a,b,c;
  // calculate the moments
  int N = w*h;
  double Zavg = 0;
  double Xavg = 0;
  double Yavg = 0;
  double sXZ = 0;
  double sYZ = 0;
  double sXX = 0;
  double sYY = 0;

  for (int x=0; x < w; x++)
    {
      for (int y=0; y < h; y++)
	{
	  Zavg += (double) data[y*w + x];
	  Xavg += (double) (x-w/2);
	  Yavg += (double) (y-h/2);
	  sXZ += (double) data[y*w + x] * (x-w/2);
	  sYZ += (double) data[y*w + x] * (y-h/2);
	  sXX += (double) (x-w/2)*(x-w/2);
	  sYY += (double) (y-h/2)*(y-h/2);
	}
    }

  Xavg /= N;
  Yavg /= N;
  Zavg /= N;

  a = (sXZ - N*Xavg*Zavg)/(sXX - N*Xavg*Xavg);
  b = (sYZ - N*Yavg*Zavg)/(sYY - N*Yavg*Yavg);
  c = Zavg - a*Xavg - b*Yavg - LMAX/2;
  
  //warn( "c = %f\n",c);
  //warn( " a %10.2f\n b %10.2f\n c %10.2f\n", a, b, c);

  plane_a = b;
  plane_b = a;

  plane_c = c;
};

int ImageWindow::planeval(int x,int y) 
{
  if (plane)
    return (int) (plane_a * (x-w/2) + plane_b*(y-h/2));
  else
    return 0;
}

double Image_Operation::getParameter(const char *str)
{
  for(parameters_t::iterator i = parameters.begin(); i != parameters.end(); i++)
    if(i->name == str)
      return i->value;
  warn("Warning: unknown parameter \"%s\" on image operation \"%s\"\n", str, name.c_str());
  warn("Available parameters:\n");
  for(parameters_t::iterator i = parameters.begin(); i != parameters.end(); i++)
    warn("\t%s\n",i->name.c_str());
  return 0.0;
}

void ImageWindow::dumpColormap()
{
  string fn = output_basename;
  fn += ".colormap.dat";
  info("dumping colormap to %s\n", fn.c_str());
  FILE *fp = fopen(fn.c_str(), "w");
  fprintf(fp, "# hmin %d hmax %d gamma %e\n", hmin, hmax, gam);
  
  unsigned char r,g,b;
  int cmap_index;
  for (int i=hmin; i<hmax; i++)
    {
      cmap_index = imagehist[i]*(colormap_length-1)/LMAX;
      makergb(i, r, g, b);
      fprintf(fp, "%d %d %e %d %d %d %d\n",
	      cmap_index, i, id.quant_to_raw(i), 
	      imagehist[i],  
	      r, g, b);
    }
  fclose(fp);
}

      

void ImageWindow::exportLinecut()
{
  // Ok, this is a real hack, but it's easy...
  char tmp[1024];
  char label[1024];
  char fn[1024];

  //sprintf is just so damn more convenient than c++ strings
  if (line_cut_type == HORZLINE) 
    snprintf(label, 1024, "l.%d", line_cut_yp);
  else if (line_cut_type == VERTLINE) 
    snprintf(label, 1024, "c.%d", line_cut_xp);
  else 
    sprintf(label, "other");
  snprintf(fn, 1024, "%s.%s.linecut.dat", output_basename, label);
	
  info("exporting linecut to file %s\n", fn);

  strncpy(tmp, xsection_fn, 1024);
  strncpy(xsection_fn, fn, 1024);
  plotLineCut();
  strncpy(xsection_fn, tmp, 1024);
}

void ImageWindow::exportGnuplot()
{
  FILE *fp;
  //char buf1[256],buf2[256];
  int i,j;
 
  string base = output_basename;
  // Output the data in pm3d format

  //snprintf(buf1, 256, "%s.pm3d", output_basename);
  if ((fp = fopenwarn((base+".gp").c_str(), "w")) == NULL)
    return;

  for (i=0; i<w; i++)
    {
      for (j=0; j<h; j++)
	fprintf(fp, "%e %e %e\n", id.getX(i), id.getY(j), dataval(i,j));
      fprintf(fp, "#\n\n");
    }
  fclose(fp);

  // Now output a gnuplot script file to plot the data with the right colormap range.

  //snprintf(buf2, 256, "%s.gnu", output_basename);
  if ((fp = fopenwarn((base+".gnu").c_str(), "w")) == NULL)
    return;
  
  fprintf(fp, "set palette defined (");
  for (int i=0; i<colormap_length-1; i+=1)
    fprintf(fp, "%f %f %f %f,", 1.0*i/(colormap_length-1), 1.0*colormap[3*i]/255.0, 1.0*colormap[3*i+1]/255.0, 1.0*colormap[3*i+2]/255.0);
  fprintf(fp, "%f %f %f %f)\n", 1.0, 1.0*colormap[3*colormap_length-3]/255.0, 1.0*colormap[3*colormap_length-2]/255.0, 1.0*colormap[3*colormap_length-1]/255.0);

  fprintf(fp, 
	  "set view map; set pm3d; set st d pm3d;\n"
	  "unset grid; set pm3d corners2color c1;\n"
	  "set cbrange [%e:%e];\n"
	  "set xrange [%e:%e]; set yrange [%e:%e]\n"
	  "set xlabel '%s'; set ylabel '%s'; set cblabel '%s'\n"
	  "splot \"%s\"\n", 
	  id.quant_to_raw(hmin), id.quant_to_raw(hmax),
	  id.getX(0), id.getX(w),
	  id.getY(h), id.getY(0),
	  id.xname.c_str(), id.yname.c_str(), id.zname.c_str(),
	  (base+".pm3d").c_str()); 
  fclose(fp);

}

void ImageWindow::exportMAT()
{
  string name = output_basename;
  name += ".export.dat";
  FILE *fp;
  if ((fp = fopenwarn(name.c_str(), "w")) == NULL)
    return;
  
  for (int i=0; i<w; i++)
    {
      for (int j=0; j<h; j++)
	fprintf(fp, "%e ", dataval(i,j));
      fprintf(fp, "\n");
    }
  fclose(fp);
}

void ImageWindow::setupPS()
{
  imageprinter->updateSettings(&ipc);
  if (ipc.win->shown())
    ipc.win->hide();
  else
    ipc.win->show();
}

char *ImageWindow::exportPS()
{  
  //const char *extension = ipc.format->value() == Image_Printer::FORMAT_PDF ? "pdf" : "ps";
  struct stat s;
  FILE *fp;
  static char buf[1024];  
  static char buf2[1024];

  if (ipc.auto_inc->value())
    {
      int number = 0;
      snprintf(buf, sizeof(buf), "%s.%d", output_basename, number++);
      while(stat(buf,&s) == 0) // Stat will return 0 if the file exists
	{
	  snprintf(buf, sizeof(buf), "%s.%d", output_basename, number++);
	  if(number > 1000)
	    {
	      warn("Unable to come up with a suitable file name\n");
	      return NULL;
	    }
	}
      if(errno != ENOENT)
	{
	  warn("Problem stating output file name: %s\n",strerror(errno));
	  return NULL;
	}
      ipc.incnum->value(number);
    }
  else if (ipc.do_number->value())
    {
      int n = ipc.incnum->value();
      snprintf(buf, sizeof(buf), "%s.%d", output_basename, n);
      if (ipc.increment->value())
	ipc.incnum->value(n+1);
    }
  else
    snprintf(buf, sizeof(buf), "%s", output_basename);

  snprintf(buf2, sizeof(buf2), "%s.ps", buf);
  info("outputting postscript file %s\n", buf2);
  fp = fopen(buf2, "w");
  imageprinter->print(fp);
  fflush(fp);
  fclose(fp);

  // This will call ghostscript to do the extra conversions
  imageprinter->do_extra_conversions(buf);
  return buf2;
}

void ImageWindow::exportMTX(bool save, bool zoom)
{
  FILE *fp;

  string name = output_basename;

  if (save)
    name += ".mtx";
  else if (zoom)
    name += ".zoom.mtx";
  else
    name += ".export.mtx"; 

  info("save %d name %s\n", save, name.c_str());

  if ((fp = fopen(name.c_str(), "r")) != NULL && (save || zoom))
    {
      if (fl_ask("Overwrite file %s", name.c_str()) == 1)
	fclose(fp);
      else
	{
	  fclose(fp);
	  return;
	}
    }

  if ((fp = fopenwarn(name.c_str(), "wb")) == NULL)
    return;
  
  int x1, y1, x2, y2;
  double xmin, ymin, xmax, ymax;

  if (zoom)
    {
      zoom_window->getSourceArea(x1,y1,x2,y2);
      x2--;
      y2--;
    }
  else
    {
      x1 = 0; x2 = id.width-1;
      y1 = 0; y2 = id.height-1;
    }
  
  xmin = id.getX(x1);
  xmax = id.getX(x2);
  ymin = id.getY(y1);
  ymax = id.getY(y2);

  int wid = x2-x1+1;
  int hgt = y2-y1+1;

  string zname = search_replace(id.zname, ",", ";");
  string xname = search_replace(id.xname, ",", ";");
  string yname = search_replace(id.yname, ",", ";");
  
  fprintf(fp, "Units, %s,"
	  "%s, %e, %e,"
	  "%s, %e, %e," 
	  "Nothing, 0, 1\n",
	  zname.c_str(), 
	  xname.c_str(), xmin, xmax,
	  yname.c_str(), ymin, ymax);
  fprintf(fp, "%d %d 1 8\n", wid, hgt);

  //info("x1 %d x2 %d width %d\n", x1, x2, wid);
  //info("y1 %d y2 %d height %d\n", y1, yq2, hgt);

  for (int i=x1; i<=x2; i++)
    for (int j=y1; j<=y2; j++)
      fwrite(&id.raw(i,j), sizeof(double), 1, fp);
  
  fclose(fp);
}

void ImageWindow::exportPGM()
{
  FILE *fp;
  char buf[256];
  int i,j;

  snprintf(buf, 256, "%s.export.pgm", output_basename);

  if ((fp = fopenwarn(buf, "wb")) == NULL)
    return;
  
  fprintf(fp, "P5\n%d %d\n", w, h);
  fprintf(fp, "#zmin %e\n"
	  "#zmax %e\n"
	  "#xmin %e\n"
	  "#xmax %e\n"
	  "#ymin %e\n"
	  "#ymax %e\n"
	  "#xunit %s\n"
	  "#yunit %s\n"
	  "#zunig %s\n"
	  "#Image Processing: %s\n"
	  "65535\n", id.qmin, id.qmax, id.xmin, id.xmax, id.ymin, id.ymax,
	  id.xname.c_str(), id.yname.c_str(), id.zname.c_str(), 
	  operations_string.c_str());
  
  char c;
  int val;
  for (j=0; j<h; j++)
    for (i=0; i<w; i++)
      {
	val = data[j*w+i]-planeval(j,i); 
	c = val/256; 
	fwrite(&c,1,1,fp);
	c = val%256;
	fwrite(&c,1,1,fp);
      }
  fclose(fp);
}

void ImageWindow::exportMatlab()
{
  FILE *fp;
  char buf[256];

  string base = output_basename;

  // Matlab M-file names can contain only alphanumeric characters!
  // http://authors.ck12.org/wiki/index.php/Introduction_to_Programming_with_M-file_Scripts
  // How annoying...

  for (int pos = 0; pos < base.size(); pos++)
    if (!isalnum(base[pos]) && 
	base[pos] != '\\' &&  // we should not replace directory separators...
	base[pos] != ':' &&
	base[pos] != '/')
      base[pos] = '_';
  
  info("Outputting file %s\n", (base+".m").c_str());

  if ((fp = fopenwarn((base+".m").c_str(), "w")) == NULL)
    return;
  
  // Output the current colormap
  fprintf(fp, "cmap = [ ");
  for (int i=0; i<colormap_length; i+=1)
    {
      int tmp = gammatable[i];
      fprintf(fp, "%f %f %f ", 
	      1.0*colormap[3*tmp]/255.0, 
	      1.0*colormap[3*tmp+1]/255.0, 
	      1.0*colormap[3*tmp+2]/255.0);
      if (i != colormap_length-1)
	fprintf(fp, ";\n");
    }
  fprintf(fp, "];\n");

  fprintf(fp, "data = [ ");
  for (int j=0; j<h; j++)
    {
      for (int i=0; i<w; i++)
	fprintf(fp, "%e ", dataval(i,j));
      if (j != h-1)
	fprintf(fp, ";\n");
    }
  fprintf(fp, "];\n");

  double min = id.quant_to_raw(hmin);
  double max = id.quant_to_raw(hmax);
  

  fprintf(fp, 
	  "figure(1)\n"
	  "colormap(cmap)\n"
	  "imagesc(data)\n"
	  "caxis([%e %e])\n"
	  "figure(2)\n"
	  "colormap(cmap)\n"
	  "surf(data)\n"
	  "caxis([%e %e])\n"
	  "shading flat\n"
	  "lighting gouraud\n"
	  "camlight\n", 
	  min, max, min, max);
  fclose(fp);
}

// Zoom window support

void ZoomWindow::realloc_image()
{
  if((image == NULL) || (image_size != static_cast<size_t>(w()*h())))
    {
      if(image != NULL)
	free(image);
      image_size = w()*h();
      image = (unsigned char *)malloc(sizeof(unsigned char) * image_size * 3);      
    }
}
ZoomWindow::ZoomWindow(int w, int h, const char *title) : Fl_Double_Window(w,h,title), image(NULL), img(NULL), xscale(2),yscale(2), center_x(0), center_y(0)
{
  autonormalize = false;
}

void ZoomWindow::calculateHistogram()
{
  int x1,y1,x2,y2;
  getSourceArea(x1,y1,x2,y2);
  for(unsigned i = 0; i < hist_len; i++)
    histogram[i] = 0;
  for(int x = x1; x < x2; x++)
    for(int y = y1; y < y2; y++)
      {
	int d = img->id.quant(x,y);
	if(d >= 0 && d < static_cast<int>(hist_len))
	  histogram[d]++;   
      }
}

void ZoomWindow::draw()
{
  if(!img)
    return;
  realloc_image();
  int src_x1, src_y1, src_x2, src_y2;
  getSourceArea(src_x1, src_y1, src_x2, src_y2);
  int my = h();
  int mx = w();
  unsigned char *p = image;
  for(int y = 0; y < my; y++)
    for(int x = 0; x < mx; x++)
      {
	int sx = src_x1 + x/xscale;
	int sy = src_y1 + y/yscale;
	if(sx >= src_x2 || sy >= src_y2)
	  {
	    *p++=0;
	    *p++=0;
	    *p++=0;
	    continue;
	  }
	else
	  {
	    unsigned char r,g,b;
	    img->getrgb(sy,sx,r,g,b);
	    *p++=r;
	    *p++=g;
	    *p++=b;
	  }	
      }
  fl_draw_image(image,0,0,mx,my,3,0);
  // This is sometimes getting out of sync: put in the redraw...
  snprintf(window_label, 256, "Zoom of %s: (%dx,%dx)", img->filename.c_str(), xscale, yscale);
  label(window_label);
}

int ZoomWindow::handle(int event)
{
  switch (event)
    {    
    case FL_PUSH:
      push_mouse_x = Fl::event_x();
      push_mouse_y = Fl::event_y();
      push_center_x = center_x;
      push_center_y = center_y;
      break;
    case FL_DRAG:
    case FL_RELEASE:
      center_x = push_center_x - (Fl::event_x()-push_mouse_x)/xscale;
      center_y = push_center_y - (Fl::event_y()-push_mouse_y)/yscale;
      zoomMoved();
      break;
    case FL_HIDE:
      if(img)
	img->redraw_overlay();
      break;
    case FL_SHOW:
      if(img)
	img->redraw_overlay();
      break;
    case FL_KEYDOWN:
      char c = Fl::event_key();
      //info("Key %c\n",c);
      switch(c)
	{
	case '=':
	case '+':
	case '.':
	case '>':
	  {
	    int oldw, oldh;
	    oldw = w()/xscale;
	    oldh = h()/yscale;
	    if (Fl::event_state() & FL_SHIFT)
	      xscale++;
	    else if (Fl::event_state() & FL_CTRL)
	      yscale++;
	    else
	      {xscale++; yscale++;}
	    size(oldw*xscale, oldh*yscale);
	    if(img)
	      img->redraw_overlay();
	    redraw();
	    return 1;
	  }
	case '-':
	case ',':
	case '<':
	  {
	    int oldw, oldh;
	    oldw = w()/xscale;
	    oldh = h()/yscale;
	    if (Fl::event_state() & FL_SHIFT)
	      xscale--;
	    else if (Fl::event_state() & FL_CTRL)
	      yscale--;
	    else
	      {xscale--; yscale--;}
	    if(xscale < 1)
	      xscale = 1;
	    if(yscale < 1)
	      yscale = 1;
	    size(oldw*xscale, oldh*yscale);
	    if(img)
	      img->redraw_overlay();
	    redraw();
	    return 1;
	  }
	case 'z':
	  if (!(Fl::event_state() & FL_CTRL))
	    {
	      hide();
	      return 1;
	    }
	}
      break;
    }
  return 0;
}

void ZoomWindow::resize(int x, int y, int w, int h)
{
  snprintf(window_label, 256, "Zoom of %s: (%dx,%dx)", img->filename.c_str(), xscale, yscale);
  label(window_label);
  Fl_Double_Window::resize(x,y,w,h);
  img->external_update();
  zoomMoved();
  size_range(1,1);
} 

void ZoomWindow::getSourceArea(int &x1, int &y1, int &x2, int &y2)
{
  int dx = w()/xscale;
  int dy = h()/yscale;
  
  //warn( "xscale %d yscale %d\n", xscale, yscale);

  x1 = center_x - dx/2; // Find the ideal top-left
  y1 = center_y - dy/2;

  if(x1 < 0) x1 = 0; // Clip to the window
  if(y1 < 0) y1 = 0;

  x2 = x1 + dx; // Find the bottom right, including top-left clipping
  y2 = y1 + dy;

  if(x2 >= img->w) // Clip the bottom right to the window
    {
      x2 = img->w;
      if(x2-dx < 0) // Be careful if the source window isn't as wide as dx!
	x1 = 0;
      else
	x1 = x2-dx;
    }
  if(y2 >= img->h)
    {
      y2 = img->h;
      if(y2-dy < 0)
	y1 = 0;
      else
	y1 = y2-dy;
    }
}

void ZoomWindow::zoomMoved()
{
  if(autonormalize && img)
    img->normalizeOnZoom();
  redraw();
  if(img)
    img->redraw_overlay();
}

ColormapWindow::ColormapWindow(int wp, int hp, const char *title) : Fl_Window(wp,hp,title), image(NULL)
{  
  // You shouldn't call functions from a constructor!
  //resize(x(),y(),w(),h());
  //size_range(1,1,0,0);
}

void ColormapWindow::resize(int x, int y, int w, int h)
{
  Fl_Window::resize(x,y,w,h);

  if(image)
    delete[] image;
  image = NULL; 

  if(h < w)
    {
      vertical = false;
      xmult = w/wid;
      //warn( "xmult %d\n", xmult);
      if(xmult == 0)
	return;
      ih = h;
      iw = w - (w % wid);
    }
  else
    {
      vertical = true;
      xmult = h/wid;
      //warn( "xmult %d\n", xmult);
      if(xmult == 0)
	return;
      ih = w;
      iw = h - (h % wid);
    }
  if(iw <= 0 || ih <= 0)
    return;
  image = new uchar[3*iw*ih];
  assert(image);  
  update();
  redraw();
}

ColormapWindow::~ColormapWindow()
{
  delete[] image;
}

void ColormapWindow::update()
{
  if(!img || !image)
    return;

  double foo = 1.0 * (img->colormap_length-1)/(iw-1);
  int tmp, tmp2;
  int i;
  for (int y = 0; y < ih; y++)
    for (int x = 0; x < iw; x++)
      {
	tmp2 = (int) round(x*foo);
	tmp = img->gammatable[tmp2];
	if (vertical)
	  i = iw-1-x;
	else
	  i = x;
	
// 	if (x == iw/2 && y == 0)
// 	  {
// 	    warn( "iw %d cmapl %d foo %g x %d tmp2 %d tmp %d\n", 
// 		    iw, img->colormap_length, foo, x, tmp2, tmp);
// 	  }
	if(tmp > img->colormap_length-1)
	  tmp = img->colormap_length-1;
	if(tmp < 0)
	  tmp = 0;
	image[3*(i+y*iw)] = img->colormap[3*tmp];
	image[3*(i+y*iw)+1] = img->colormap[3*tmp+1];
	image[3*(i+y*iw)+2] = img->colormap[3*tmp+2];	
      }
  redraw();
}
void ColormapWindow::draw()
{
  if(image)
    if(vertical)
      fl_draw_image(image,0,(h()-iw)/2,ih,iw,iw*3,3);      
    else
      fl_draw_image(image,(w()-iw)/2,0,iw,ih,3,0);      
}

void ColormapWindow::saveFile(const char *name)
{
  static const int height=32;
  warn( "saving colormap to %s", name);
  FILE *fp = fopen(name, "wb");
  if(fp == NULL)
    {
      warn("Unable to open file \"%s\": %s\n",
	   name, strerror(errno));
      return;
    }
  fprintf(fp, 
	  "P6\n%d %d\n"
	  "#zmin %f\n"
	  "#zmax %f\n"
	  "255\n", wid, height, 
	  img->zunit(img->hmin),
	  img->zunit(img->hmax));

  for(int j = 0; j < height; j++)
    for (int i = 0; i< wid; i++)
      {
	int tmp = img->gammatable[i];
	fwrite(&(img->colormap[3*tmp]),sizeof(char),3,fp);
      }
	
  fclose(fp);
}

