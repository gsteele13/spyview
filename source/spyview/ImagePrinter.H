#ifndef __image2dprinter_h__
#define __image2dprinter_h__
#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include "message.h"

#include "myboost.h"
#include "FLTK_Serialization.H"

#include "ImagePrinter_Control.h"

using namespace std;

class ImageWindow;
class Image_Printer_Control;
class Image_Printer
{
public:
  static const int PLOT_ALL = 0;
  static const int PLOT_ZOOM = 1;
  static const int PLOT_MANUAL = 2;

// Info on what region of the image we're plotting
  int img_x1, img_y1, img_x2, img_y2; 
  int img_xsize, img_ysize;

  static std::string settingsDir;       // Directory to store print settings in.
  std::string previewFileName;          // File name for the "live preview"
  pid_t previewProc;                    // Process handling the postscript preview

  Image_Printer_Control *ipc;
  ImageWindow *iw;
  FILE *out;

  double zero; // Numbers smaller than this fraction of spacing are treated as exactly zero.

  double width; // inches
  double xoff; // inches
  double yoff; // inches

  double boxwidth; // points, set to -1 for no box
  double tickwidth; // points, set to -1 for no tick
  double ticklength; // inches

  double xspacing; // X tick spacing, 0 for auto
  double yspacing; // Y tick spacking, 0 for auto

  double page_width;
  double page_height;

  // Lets move a bunch of the local variables in the print function
  // into the class so that we can modularize things better

  bool preview;
  int bb_ll_x, bb_ll_y, bb_ur_x, bb_ur_y; // Bounding box estimates

  int max_ytic_width; // for choosing the offset of the y axix label
  int max_ctic_width; // for choosing the offset of the y axix label

  double height;
  double fontsize;

  double xscale;
  double yscale;
  double zscale;

  double cbar_width;
  double cbar_height;
  double cbar_xoff;
  double cbar_yoff;

  //Why do we need to pass "toPrint"? Shouldn't this be in the class?
  //I will try taking it out, as it will make it easier to modularize
  //the messy "print()" code.
  void print(FILE *out, bool previewo=false);
  void do_extra_conversions(string basename);
  void validatePreviewCmd();
  void startPreview();
  void updatePreview() { updatePreview(false); };
  void updatePreview(bool first);
  void updateSettings(Image_Printer_Control *ipc);
  void updatePlotRange(Image_Printer_Control *ipc);
  void fetchSettings(Image_Printer_Control *ipc);
  void update_page_size(int type=-1);
  void setAspectRatio(int change_axis);

  void initSettingsDir(); // Set up the print settings directory tree
  void savePrintSettings(std::string fname);
  void loadPrintSettings(std::string fname);

  // Preview settings
  void savePreviewSettings();
  void loadPreviewSettings();

  double autospace(double min, double max); // Automatically pick tic spacing
  void computePlotRange(); // Must be called before unitToDevice and pixelToDevice work!
  void unitToDevice(double &x, double &y); // Convert X & Y from united to device coordinates.
  void pixelToDevice(double &x, double &y); // Convert X & Y from spyview row,col to device coordinateso
  void pushClip(FILE *out, double xoff, double yoff,double width,double height);
  void box(FILE *out, double x, double y, double w, double h); // Draw a box; arguments in inches


  // Start breaking these up into separate functions to make the code
  // easier to read, and to make it easier to modularize things
  void write_header(); // write some stuff in header, including estimating bouding box
  void write_watermark();
  void draw_image(); 
  void draw_lines(); // draw overlay lines and image zoom box
  void draw_colorbar();
  void draw_axis_tics();
  void draw_axis_labels();
  void add_dirstamp();
  void calculateBoundingBox();

  Image_Printer(ImageWindow *p_iw, Image_Printer_Control *p_ipc);
  ~Image_Printer();

  template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
      if(version < 3)
	{
	  warn("No support for settings files older than version 3.  Sorry\n");
	  return;
	}
      ar & flcast(ipc->xaxis_label);
      ar & flcast(ipc->yaxis_label);
      ar & flcast(ipc->dir_stamp);
      ar & flcast(ipc->title);
      if (version == 8)	 
       {
	 Fl_Button tmp(0,0,0,0,""); // Throw the result of the serialization away
	 ar & flcast(&tmp);         //  since we don't use it anymore.
       }
      ar & flcast(ipc->do_title);
      ar & flcast(ipc->caxis_label);
      ar & flcast(ipc->paper_eps);
      ar & flcast(ipc->paper_letter);
      ar & flcast(ipc->paper_a4);
      ar & flcast(ipc->landscape);
      ar & flcast(ipc->width);
      ar & flcast(ipc->height);
      ar & flcast(ipc->xoff);
      ar & flcast(ipc->yoff);
      ar & flcast(ipc->ticklength);
      ar & flcast(ipc->xspacing);
      ar & flcast(ipc->yspacing);
      ar & flcast(ipc->xscale);
      ar & flcast(ipc->yscale);
      if(version >= 5)
	ar & flcast(ipc->zscale);
      ar & flcast(ipc->precise_ticks);
      if(version >= 10)
	ar & flcast(ipc->fancy_ticks);
      ar & flcast(ipc->face);
      ar & flcast(ipc->fontsize);
      ar & flcast(ipc->boxwidth);
      ar & flcast(ipc->tickwidth);
      ar & flcast(ipc->xticfmt);
      ar & flcast(ipc->yticfmt);
      ar & flcast(ipc->cticfmt);
      ar & flcast(ipc->colorbar);
      ar & flcast(ipc->cbar_height_per);
      ar & flcast(ipc->cbar_width);
      if(version >= 6)
	{
	  ar & flcast(ipc->plotRange);
	  ar & flcast(ipc->xrange);
	  ar & flcast(ipc->yrange);
	}
      if(version >= 7)
	{
	  ar & flcast(ipc->plotLineCut);
	}
      if(version >= 11)
	ar & flcast(ipc->plotLines);
      if(version >= 12)
	{
	  ar & flcast(ipc->increment);
	  // here, we have to recontruct the old FL choice
	  // otherwise, we get a segmentation fault at a completely 
	  // non obvious point in the program!!! 
	  //Fl_Choice tmp(0,0,0,0,"");
	  //tmp.add(".PDF");
	  //tmp.add(".PS");
	  //ar & flcast(&tmp);

	  // No matter what I do, this always segfaults. It segfaults
	  // only when the "controls" window is opened, which is also
	  // completely non-obvious. We are clearly clobbering some
	  // kind of memory space that we're not supposed to. It's
	  // simplest just to leave the widget in the damn FLTK
	  // window, and just not show it.

	  ar & flcast(ipc->format);
	}
      if (version >= 13)
	{
	  ar & flcast(ipc->cspacing);
	  ar & flcast(ipc->linear_cmap);
	  ar & flcast(ipc->cmin);
	  ar & flcast(ipc->cmax);
	  ar & flcast(ipc->cbegin);
	  ar & flcast(ipc->clabelspacing);
	}
      if(version >= 14)
	{
	  ar & flcast(ipc->plotZoomBox);
	}
      if(version >= 15)
	{
	  ar & flcast(ipc->text_color);
	  ar & flcast(ipc->border_color);
	  ar & flcast(ipc->large_tick_color);
	  ar & flcast(ipc->small_tick_color);
	  ar & flcast(ipc->linecut_color);
	  ar & flcast(ipc->overlay_color);
	  ar & flcast(ipc->zoombox_color);
	}
      if(version >= 16)
	ar & flcast(ipc->watermark);
      if(version >= 17)
	{
	  ar & flcast(ipc->extra_svs);
	  ar & flcast(ipc->extra_set);
	}
      fetchSettings(ipc);      
    };     

};

BOOST_CLASS_VERSION(Image_Printer, 17); // Increment this if the archive format changes.
// Note; you also have to change serialize to provide backward compatability if you do
// so!  See the boost documentation.

#endif
