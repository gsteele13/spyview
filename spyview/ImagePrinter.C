#include <sys/types.h>
#include "ImagePrinter.H"
#include "ImageWindow.H"
#include "ImageWindow_LineDraw.H"
#include "message.h"
#include "throttle.H"
#include <algorithm>
#include <sys/time.h>
#include "eng.h"
#include <limits.h>
#include <libgen.h>
#include <signal.h>


// Violating all rules of abstraction, but I can't figure our any other way to get access to argv[0]
#include "spyview.H"

static bool Clip(double &x1,double &y1,double &x2,double &y2, double wx1, double wy1, double wx2, double wy2);
static void setColor(FILE *out, Fl_Color_Chooser *c);

#ifndef WIN32
#include <sys/wait.h>
using namespace boost;
using namespace serialization;
using namespace std;
#endif 

static const double INCH=72.0; // Inch in points.
using namespace std;
std::string Image_Printer::settingsDir;

// Implementation of cohen-sutherland algorithm.
static const int NONE=0;
static const int LEFT=1;
static const int RIGHT=2;
static const int TOP=4;
static const int BOTTOM=8;

int wx1,wy1,wx2,wy2;

Image_Printer::Image_Printer(ImageWindow *p_iw, Image_Printer_Control *p_ipc)
{
  iw = p_iw;
  ipc = p_ipc;
  ipc->ip = this;
  ipc->face->value("Palatino");
  ipc->fontsize->value("24");
  width=7;
  ipc->height->value("7.0");
  xoff=1;
  yoff=1;
  boxwidth=4;
  tickwidth=4;
  ticklength=.25;    
  zero=1e-15;
  xspacing=0;
  yspacing=0;
  ipc->xscale->value("1.0");
  ipc->yscale->value("1.0");
  ipc->zscale->value("1.0");

  ipc->xticfmt->value("%.2g");
  ipc->yticfmt->value("%.2g");
  ipc->cticfmt->value("%.2g");
  page_width = 11;
  page_height = 8.5;
  ipc->landscape->value(1);
  ipc->colorbar->value(0);
  loadPrintSettings(""); // Load the default settings...
  previewFileName = tmpnam(NULL);  
  previewProc = -1;

  // Some default settings for the preview & formats, in case the
  // settings file is not found
  ipc->png_dpi->value("300");
#ifdef WIN32
  ipc->gs_cmd->value("\"c:/Program Files/gs/gs8.64/gswin32c.exe\"");
  ipc->preview_cmd->value("\"c:/Programs Files/Ghostgum/gsview/gsview32.exe\"");
#else
  ipc->gs_cmd->value("gs");
  ipc->preview_cmd->value("gv --watch");
#endif
  loadPreviewSettings();
}

Image_Printer::~Image_Printer()
{
  info("Cleaning up\n");

#ifndef WIN32
  if(previewProc != -1) // We're about to unlink the preview ps file.  Better kill gs first.
    kill(previewProc,SIGTERM);
#endif

  unlink(previewFileName.c_str());
}

void Image_Printer::do_extra_conversions(string basename)
{
  string command = "";
  if (ipc->extra_pdf->value())
    {
      command = "";
      command = ipc->gs_cmd->value();
      if (ipc->cmyk->value())
	command += " -dProcessColorModel=/DeviceCMYK ";
      if (ipc->paper_letter->value())
	command += " -sPAPERSIZE=letter ";
      else if (ipc->paper_a4->value())
	command += " -sPAPERSIZE=a4 ";
      command += " -q -sDEVICE=pdfwrite -dBATCH -dNOPAUSE -dEPSCrop ";
      command += " -dAutoFilterColorImages=false -dAutoFilterGrayImages=false ";
      command += " -dColorImageFilter=/FlateEncode -dGrayImageFilter=/FlateEncode ";
      command += " -sOutputFile=\"" + basename + ".pdf\" \"";
      command += basename + ".ps\"";
      info("pdf command:\n%s\n",command.c_str());
      // Don't you just love win32? This is pretty obvious, right?
      // http://jason.diamond.name/weblog/2005/04/14/dont-quote-me-on-this
#ifdef WIN32
      system(("\""+command+"\"").c_str());
#else
      system(command.c_str());
#endif
    }
  if (ipc->extra_png->value())
    {
      command = "";
      command = ipc->gs_cmd->value();
      if (ipc->paper_letter->value())
	command += " -sPAPERSIZE=letter ";
      else if (ipc->paper_a4->value())
	command += " -sPAPERSIZE=a4 ";
      command += " -q -sDEVICE=pngalpha -dBATCH -dNOPAUSE -dEPSCrop ";
      command += " -r";
      command += ipc->png_dpi->value();
      command += " -sOutputFile=\"" + basename + ".png\" \"";
      command += basename + ".ps\"";
      info("png command:\n%s\n",command.c_str());
#ifdef WIN32
      system(("\""+command+"\"").c_str());
#else
      system(command.c_str());
#endif
    }
  if(ipc->extra_svs->value())
    {
      savesettings(basename + ".svs");
    }

  if(ipc->extra_set->value())
    {
      savePrintSettings(basename + ".set");
    }
  info("done\n");
}

void Image_Printer::startPreview()
{
#ifndef WIN32
  if(previewProc != -1)
    kill(previewProc,SIGTERM);
#endif
  updatePreview(true);
#ifndef WIN32
  previewProc = fork();
#endif
  if(previewProc == 0) // We're the baby
    execlp("gv","gv",previewFileName.c_str(),"--watch", NULL);  
}

void Image_Printer::updatePreview(bool first)
{
  static Throttle<Image_Printer> throttle(this,&Image_Printer::updatePreview, 1.0);
  if(!throttle.throttle())
    return;
  //reset_time();
  //validatePreviewCmd();
  if (previewFileName.size() == 0) return;
  if(!first)
    {
      if(previewProc == -1)
	return;
      // If the viewer has quit, exit.	 	
      int stat;	 	
      int ret = 0;
#ifndef WIN32
      ret = waitpid(previewProc,&stat,WNOHANG);	 	
#endif
      if(ret != 0)	 	
	{	 	
	  warn("PS viewer has quit...\n");	 	
	  previewProc = -1;	 	
	  unlink(previewFileName.c_str());
	  return;	 	
	}
    }
  char *n = tmpnam(NULL);

  FILE *out = fopen(n,"w");
  print(out,true);
  fclose(out);

  //  printf("Took %g seconds to generate preview\n",current_time());
  // No need to unlink the old file; rename is guaranteed to do that atomically.
  //  if (unlink(previewFileName.c_str()) != 0)
  //    info("error deleteing file \"%s\": %s\n", previewFileName.c_str(), strerror(errno));
  if (rename(n,previewFileName.c_str()) != 0)
    info("error renaming file \"%s\" to \"%s\": %s\n", n, previewFileName.c_str(), strerror(errno));
#ifndef WIN32
  if(!first)
    kill(previewProc,SIGHUP);
#endif
}

double Image_Printer::autospace(double min, double max)
{
  double range = fabs(min-max);
  if(range*range == 0)
    {
      return 1;
    }	
  
  double digitsRange = floor(log10(range));
  double rangeMult = (double)pow(10.0,digitsRange);
  
  double collapsedRange = range/rangeMult;
  if(collapsedRange < 2.5)
    return .25 * rangeMult;
  else if(collapsedRange < 5)
    return .5 * rangeMult;
  else if(collapsedRange <= 10)
    return 1.0 * rangeMult;
  warn("Tick Pick Problem... min %e max %e\n", min, max);
  return 1.0 * rangeMult;
}

void Image_Printer::pixelToDevice(double &x, double &y)
{
  double xscale = atof(ipc->xscale->value());
  double yscale = atof(ipc->yscale->value());
  
  y = iw->id.getY(y)*yscale; // Why the hell is this right?
  // Why does spyview have so many coordinate systems, some of which have 0,0 as top-left, some as bottom-left?
  x = iw->id.getX(x)*xscale;
  unitToDevice(x,y);  
}
void Image_Printer::unitToDevice(double &x, double &y)
{
  double height = atof((ipc->height->value()));
  double xscale = atof(ipc->xscale->value());
  double yscale = atof(ipc->yscale->value());
  double xmin = iw->id.getX(img_x1)*xscale;
  double xmax = iw->id.getX(img_x2)*xscale;
  double ymin = iw->id.getY(img_y1)*yscale;
  double ymax = iw->id.getY(img_y2)*yscale;
  
  double xrange, xoffset, yrange, yoffset;
  if (ipc->precise_ticks->value())
    {
      xrange=width*(img_xsize-1)/(img_xsize);  // xmin is actually 1/2 pixel from the left edge, xmax is 1/2 pixel from the right.
      xoffset = width*(0.5/(img_xsize));
      yrange = height*(img_ysize-1)/(img_ysize);
      yoffset = height*(0.5/(img_ysize)); 
    }
  else 
    {
      yrange = height;
      yoffset = 0;
      xrange=width;
      xoffset=0;
    }
  y = yoff + yrange*(y-ymax)/(ymin-ymax) + yoffset;
  x = xoff + xrange*(x-xmin)/(xmax-xmin) + xoffset;
}
// void Image_Printer::validatePreviewCmd()
// {
//   if(strstr(ipc->preview_cmd->value(),".ps"))
//     {
//       warn("Overriding apparently inappropriate preview command \"%s\" with default of \"%s\"\n",
// 	   ipc->preview_cmd->value(),defaultPreviewCmd);
//       ipc->preview_cmd->value(defaultPreviewCmd);
//     }
// }

void Image_Printer::box(FILE *out, double x, double y, double w, double h)
{
  fprintf(out,"     %g inch %g inch moveto\n",x,y);
  fprintf(out,"     %g inch 0  rlineto\n",w);
  fprintf(out,"     0 %g inch rlineto\n",h);
  fprintf(out,"     %g inch 0 rlineto closepath\n",-w);
}
void Image_Printer::pushClip(FILE *out, double xoff, double yoff,double width,double height)
{  
  fprintf(out,"     clipsave\n");
  box(out, xoff,yoff,width,height);
  fprintf(out,"     clip\n");
}

/* This function is getting *way* too long and messy.  Something needs to be done.. */
void Image_Printer::print(FILE *out_p, bool preview_p)
{
  preview = preview_p;
  out = out_p;

  // initialize some variables
  computePlotRange();
  max_ytic_width = 0; // for choosing the offset of the y axix label
  max_ctic_width = 0; // for choosing the offset of the colorbar axix label
  height = atof((ipc->height->value()));
  fontsize = atof(ipc->fontsize->value());
  xscale = atof(ipc->xscale->value());
  yscale = atof(ipc->yscale->value());
  zscale = atof(ipc->zscale->value());
  
  write_header();

  if (ipc->landscape->value() && !ipc->paper_eps->value())
    fprintf(out, "%d 0 translate 90 rotate\n", bb_ur_x );

  if(ipc->watermark->value())
    write_watermark();

  draw_image();
  draw_lines();

  if (ipc->colorbar->value())
    draw_colorbar();

  draw_axis_tics();

  // Draw the bounding box
  if(boxwidth > 0)
    {
      fprintf(out,"%% Draw our box around the plot\n");
      setColor(out,ipc->border_color);
      fprintf(out,"  %g setlinewidth newpath\n",boxwidth);
      box(out,xoff,yoff,width,height);
      fprintf(out,"  stroke\n");
    }
  else
    fprintf(out,"%% User requested no box\n");  

  draw_axis_labels();

  // By defering the bounding box, we now have access to max_ytic_width and max_ctic_width				
  if (ipc->paper_eps->value())
    {
      int label_x = static_cast<int>(- (max_ytic_width+2.5)*fontsize/2.0);
      int label_c = static_cast<int>((max_ctic_width+2.5)*fontsize/2.0);
      // Tricky part: estimate a nice bounding box for an EPS file
      bb_ll_x = (int) (xoff*INCH - (ipc->yaxis_label->value() ? 1 : -1) * fontsize + label_x); 
      // the y axis tickmark labels are much wider than the x axis are tall
      bb_ll_y = (int) (yoff*INCH - (ipc->xaxis_label->value() ? 3 : 2) * fontsize);
      if(ipc->colorbar->value())
	bb_ur_x = (int) ((cbar_xoff+cbar_width)*INCH + label_c + (ipc->caxis_label->value() ? fontsize : 0));
      else
	bb_ur_x = (int) ((xoff+width)*INCH + fontsize);
      bb_ur_y = (int) ((yoff+height)*INCH + (ipc->do_title->value() ? 2 : 1) * fontsize);
    }

  if (ipc->dir_stamp->value())
    add_dirstamp();

  if (preview || !ipc->paper_eps->value()) // If we're using eps, spec says no showpage (adobe maybe disagrees)
    fprintf(out,"showpage\n");
  fprintf(out,"grestore\n");
  fprintf(out,"%%%%Trailer\n");
  fprintf(out, "%%%%Actual BoundingBox was: %d %d %d %d\n", bb_ll_x, bb_ll_y, bb_ur_x, bb_ur_y); 
  fprintf(out,"%%%%EOF\n");

  info("Label width actual values:\n");
  info("ytic %d ctic %d\n", max_ytic_width, max_ctic_width);
  info("%%%%BoundingBox: %d %d %d %d\n", bb_ll_x, bb_ll_y, bb_ur_x, bb_ur_y); 
}

void Image_Printer::updatePlotRange(Image_Printer_Control *ipc)
{
  computePlotRange();
  double xscale = atof(ipc->xscale->value());
  double yscale = atof(ipc->yscale->value());

  char buf[1024];
  double minx, miny, maxx, maxy;
  //int img_x1, img_y1, img_x2, img_y2; // overriding class variables!!!?
  switch(ipc->plotRange->value())
    {
    case PLOT_ZOOM:
      iw->zoom_window->getSourceArea(img_x1, img_y1, img_x2, img_y2);
      //falling case!
    case PLOT_MANUAL:
      minx = iw->xunit(img_x1);
      maxx = iw->xunit(img_x2);
      miny = iw->yunit(img_y1-1); //strange: why did i need to -1?
      maxy = iw->yunit(img_y2-1);      
      break;
    case PLOT_ALL:
    default:
      minx = iw->xunit(0);
      maxx = iw->xunit(iw->w-1);
      miny = iw->yunit(0);
      maxy = iw->yunit(iw->h-1);
      break;
    }
  minx *= xscale;
  miny *= yscale;
  maxx *= xscale;
  maxy *= yscale;
  snprintf(buf,sizeof(buf),"%g,%g",minx,maxx);
  ipc->xrange->value(buf);
  snprintf(buf,sizeof(buf),"%g,%g",maxy,miny);
  ipc->yrange->value(buf);
}

void Image_Printer::computePlotRange()
{
  double xscale = atof(ipc->xscale->value());
  double yscale = atof(ipc->yscale->value());

  switch(ipc->plotRange->value())
    {
    case PLOT_ALL:
    default:
      img_x1 = 0; 
      img_y1 = 0;
      img_x2 = iw->w-1; 
      img_y2 = iw->h-1;
      break;
    case PLOT_ZOOM:
      if(iw->zoom_window)
	iw->zoom_window->getSourceArea(img_x1, img_y1, img_x2, img_y2);
      else
	{
	  ipc->plotRange->value(PLOT_ALL);
	  computePlotRange();
	  return;
	}
      break;
    case PLOT_MANUAL:
      {
	double xmin,xmax,ymin,ymax;
	if(sscanf(ipc->xrange->value(),"%lg,%lg",&xmin,&xmax) == 2)
	  {
	    img_x1 = round(iw->id.getX_inv(xmin / xscale));
	    img_x2 = round(iw->id.getX_inv(xmax / xscale));
	    if(img_x1 > img_x2)
	      swap(img_x1,img_x2);
	  }
	else
	  {
	    warn("Warning: X Range \"%s\" isn't of the form 1.0,2.0\n",ipc->xrange->value());
	    img_x1 = 0; 
	    img_x2 = iw->w-1; 
	  }
	if(sscanf(ipc->yrange->value(),"%lg,%lg",&ymax,&ymin) == 2)
	  {
	    img_y1 = round(iw->id.getY_inv(ymin / yscale));
	    img_y2 = round(iw->id.getY_inv(ymax / yscale));
	    if(img_y1 > img_y2)
	      swap(img_y1,img_y2);
	  }
	else
	  {
	    warn("Warning: Y Range \"%s\" isn't of the form 1.0,2.0\n",ipc->yrange->value());
	    img_y1 = 0; 
	    img_y2 = iw->h-1; 
	  }
      }
    }
  img_xsize = abs(img_x2-img_x1+1);
  img_ysize = abs(img_y2-img_y1+1);
}

void Image_Printer::updateSettings(Image_Printer_Control *ipc)
{
  char buf[64];

  snprintf(buf,sizeof(buf),"%g",width);
  ipc->width->value(buf);
  snprintf(buf,sizeof(buf),"%g",xoff);
  ipc->xoff->value(buf);
  snprintf(buf,sizeof(buf),"%g",yoff);
  ipc->yoff->value(buf);

  snprintf(buf,sizeof(buf),"%g",ticklength);
  ipc->ticklength->value(buf);
  snprintf(buf,sizeof(buf),"%g",xspacing);
  ipc->xspacing->value(buf);
  snprintf(buf,sizeof(buf),"%g",yspacing);
  ipc->yspacing->value(buf);

  snprintf(buf,sizeof(buf),"%g",boxwidth);
  ipc->boxwidth->value(buf);
  snprintf(buf,sizeof(buf),"%g",tickwidth);
  ipc->tickwidth->value(buf);  
  updatePlotRange(ipc);
  update_page_size();
}

void Image_Printer::fetchSettings(Image_Printer_Control *ipc)
{
  width=atof(ipc->width->value());
  xoff=atof(ipc->xoff->value());
  yoff=atof(ipc->yoff->value());

  ticklength=atof(ipc->ticklength->value());
  xspacing=atof(ipc->xspacing->value());
  yspacing=atof(ipc->yspacing->value());

  boxwidth=atof(ipc->boxwidth->value());
  tickwidth=atof(ipc->tickwidth->value());

  updatePreview();
}

void Image_Printer::update_page_size(int type)
{
  // This is a pain in the ass because in FLTK, the status of the
  // other radio buttons is not updated until after the callback is
  // executed: during the callback, two radio buttons could both be
  // "1" at the same time...

  if (type == -1 && ipc->paper_letter->value())
    type = 1;
  else if (type == -1 && ipc->paper_a4->value())
    type = 2;

  if (type == 1)
    {
      page_width = 8.5;
      page_height = 11.0;
    }

  else if (type == 2)
    {
      page_width = 8.27;
      page_height = 11.69;
    }

  if (ipc->landscape->value())
    {
      double tmp = page_width;
      page_width = page_height;
      page_height = tmp;
    }
}

void Image_Printer::setAspectRatio(int change_axis)
{
  double target_width, target_height;
  
  double cur_height = atof(ipc->height->value());
  double cur_width = width;
  
  // define ar as h/w, giving h = ar*w, w = h/ar
  double spyview_ar = 1.0* iw->dozoom(iw->h, iw->yzoom) / iw->dozoom(iw->w, iw->xzoom);
  
  // change axis:
  // 1 = change width, keep left edge
  // 2 = change width, center
  // 3 = change width, keep right edge
  // 4 = change height, keep top edge
  // 5 = change height, center
  // 6 = change height, keep bottom edge


  if (change_axis < 4) // change width
    {
      target_width = cur_height / spyview_ar;
      target_height = cur_height;
      if (change_axis == 2)
	xoff += (cur_width - target_width)/2;
      else if (change_axis == 3)
	xoff += (cur_width - target_width);
    }
  else
    {
      target_width = cur_width;
      target_height = cur_width * spyview_ar;
      if (change_axis == 4)
	yoff += (cur_height - target_height);
      else if (change_axis == 5)
	yoff += (cur_height - target_height)/2;
    }

  width = target_width;

  //historically, height is not a local class variable...
  char buf[1024];
  snprintf(buf,sizeof(buf),"%g",target_height);
  ipc->height->value(buf);
  updateSettings(ipc);
  //ipc->center_x->do_callback();
  //ipc->center_y->do_callback();
}

static void maybe_mkdir(const char *name)
{
#ifdef WIN32
  if(mkdir(name) == 0)
#else
    if(mkdir(name,0777) == 0)
#endif
  
      switch(errno)
	{
	case EEXIST:
	  return;
	default:
	  warn("Unable to create directory \"%s\": %s\n",name,strerror(errno));
	  return;
	}
}

void Image_Printer::initSettingsDir()
{
  string buf;
#ifdef WIN32
  if(getenv("SPYVIEW_PREF_DIR") != NULL)
    buf = getenv("SPYVIEW_PREF_DIR");
  else
    buf = sharedir;
#else
  buf = getenv("HOME");
  buf = buf + DIRECTORY_SEPARATOR + ".spyview";
#endif
  maybe_mkdir(buf.c_str());
  buf = buf + DIRECTORY_SEPARATOR + "print_settings";
  settingsDir = buf;
  maybe_mkdir(buf.c_str());
}

void Image_Printer::savePreviewSettings()
{
  string name;
  initSettingsDir();
  if(name.empty())
    name = settingsDir + DIRECTORY_SEPARATOR + "settings.prv";

  std::ofstream ofs(name.c_str());
  boost::archive::text_oarchive ar(ofs);

  int version;
  ar & version;
  // All this stuff is in version 1
  ar & flcast(ipc->preview_cmd);
  ar & flcast(ipc->gs_cmd);
  ar & flcast(ipc->extra_pdf);
  ar & flcast(ipc->extra_png);
  ar & flcast(ipc->png_dpi);
  ar & flcast(ipc->cmyk);
}

void Image_Printer::loadPreviewSettings()
{
  string name;
  initSettingsDir();
  if(name.empty())
    name = settingsDir + DIRECTORY_SEPARATOR + "settings.prv";

  std::ifstream ifs(name.c_str());
  if (!ifs.good())
    {
      info("preview settings file _%s_ not found\n", name.c_str());
      return;
    }
  boost::archive::text_iarchive ar(ifs);

  int version;
  ar & version;
  ar & flcast(ipc->preview_cmd);
  ar & flcast(ipc->gs_cmd);
  ar & flcast(ipc->extra_pdf);
  ar & flcast(ipc->extra_png);
  ar & flcast(ipc->png_dpi);
  ar & flcast(ipc->cmyk);
}


void Image_Printer::savePrintSettings(std::string name)
{
  fetchSettings(ipc); // Should be redundant, but paranoia is good.
  initSettingsDir();
  if(name.empty())
    {
      name = settingsDir + DIRECTORY_SEPARATOR + "default.set";
    }

  const char *fname = name.c_str();
  std::ofstream ofs(fname);
  try
    {
      if(!ofs.good())
 	throw(1);
    }
  catch (...)
    {
      warn("Unable to create or overwrite settings file \"%s\"\n",fname);
    }
 
  try
    {
      boost::archive::text_oarchive oa(ofs);

      // For some reason, on WIN32 even have this code compiled into
      // the program, even if it is not executed (!?), causes a
      // crash.
       
      // Upon further inspection, this causes a crash on my machine
      // at work, but not on my machine at home. Also, curiously, on
      // my machine at work, the fltk widgets have now started to be
      // displayed at unix "dark grey" instead of the usual windows
      // "light biege". At home, where the exact same executable is
      // not crashing, the fltk widgets are beige (except for the
      // first warning dialog box!!!)

      oa & (*this);
    }
  catch (boost::archive::archive_exception e)
    {
      error("ImagePrinter error loading settings: %s\nTry overwriting the printing settings file to get rid of this error.\n",e.what());
    }
  catch (...)
    {
      error("ImagePrinter error loading settings.\nTry overwriting the printing settings file to get rid of this error.\n");
    }

}

void Image_Printer::loadPrintSettings(std::string name)
{
  initSettingsDir();
  if(name.empty())
    {
      name = settingsDir + DIRECTORY_SEPARATOR + "default.set";
    }
  
  const char *fname = name.c_str();
  try 
    {
      std::ifstream ifs(fname);
      if(!ifs.good())
 	throw(1);
      boost::archive::text_iarchive ia(ifs);
      ia & (*this);
      updateSettings(ipc);
    }
  catch (boost::archive::archive_exception e)
    {
      error("ImagePrinter error loading settings: %s\nTry overwriting the printing settings file to get rid of this error.\n",e.what());
     }
  catch (...)
    {
      warn("Unable to load settings file \"%s\": %s\nTry overwriting the printing settings file to get rid of this error.\n",fname,strerror(errno));
    }
}


static inline int chGetCode(double x, double y, double wx1, double wy1, double wx2, double wy2)
{
  double eps = max(fabs(wx1-wx2),fabs(wy1-wy2))*1e-6;
  int code = NONE;
  if(x<wx1-eps) 
    code |= LEFT;
  else if(x>wx2+eps) 
    code |= RIGHT; 
  if(y<wy1-eps)
    code |= TOP;
  else if(y>wy2+eps)
    code |= BOTTOM;
  return code;
}

static inline bool SubClip(double &x1,double &y1,double &x2,double &y2, double wx1, double wy1, double wx2, double wy2)
{
  if(wy2 < wy1)
    swap(wy2,wy1);
  if(wx2 < wx1)
    swap(wx2,wx1);
  if(fabs(x2-x1) < fabs(y2 - y1))
    return SubClip(y1,x1,y2,x2,wy1,wx1,wy2,wx2);
  double m, tx, ty;
  int code1, code2;
  m=(y2-y1)/(float)(x2-x1);
  code1=chGetCode(x1,y1,wx1,wy1,wx2,wy2);
  code2=chGetCode(x2,y2,wx1,wy1,wx2,wy2);
  for(int i = 0; i < 20; i++)
    {
      if(code1 == NONE)
	break;
      if(code1 & LEFT) 
	{
	  tx=wx1-x1;
	  ty=y1+(tx*m);
	  x1=wx1; y1=ty;
	  code1=chGetCode(x1,y1,wx1,wy1,wx2,wy2);
	}
      if(code1 & RIGHT) 
	{
	  tx=wx2-x1;
	  ty=y1+(tx*m);
	  x1=wx2; y1=ty;
	  code1=chGetCode(x1,y1,wx1,wy1,wx2,wy2);
	}
      if(code1 & TOP) 
	{
	  ty=wy1-y1;
	  tx=x1+(ty/m);
	  x1=tx; y1=wy1;
	  code1=chGetCode(x1,y1,wx1,wy1,wx2,wy2);
	}
      if(code1 & BOTTOM)
	{
	  ty=wy2-y1;
	  tx=x1+(ty/m);
	  x1=tx; y1=wy2;
	  code1=chGetCode(x1,y1,wx1,wy1,wx2,wy2);
	}
      if((code2 & code1) != 0)
	return false;
    }
  if(code1 != NONE)
    {
      fprintf(stderr,"Bad clip: %g,%g %g,%g [%g,%g]-[%g,%g]\n",x1,y1,x2,y2,wx1,wy1,wx2,wy2);
      return false;
    }
  return true;
}

static bool Clip(double &x1,double &y1,double &x2,double &y2, double wx1, double wy1, double wx2, double wy2)
{
  int code1=chGetCode(x1,y1,wx1,wy1,wx2,wy2);
  int code2=chGetCode(x2,y2,wx1,wy1,wx2,wy2);
  if((code1 & code2) != 0)
    return false;
  if((code1 == 0) && (code2 == 0))
    return true;
  //  printf("Clipping %g,%g - %g,%g to %g,%g - %g,%g\n",x1,y1,x2,y2,wx1,wy1,wx2,wy2);
  bool res = SubClip(x1,y1,x2,y2,wx1,wy1,wx2,wy2);
  //  printf("    Clip %g,%g - %g,%g to %g,%g - %g,%g %s\n",x1,y1,x2,y2,wx1,wy1,wx2,wy2, res ? "true" : "false");	 
  return res && SubClip(x2,y2,x1,y1,wx1,wy1,wx2,wy2);


}

void setColor(FILE *out, Fl_Color_Chooser *c)
{
  fprintf(out,"%g %g %g setrgbcolor %% Color: %s\n",c->r(),c->g(),c->b(),c->label());
}

void Image_Printer::write_watermark()
{
  // The "Watermark" prints a text copy of many of the settings into a small
  // Rectangle clipped to the image area.  You can use illustrator to delete
  // the image and see it if you need to duplicate an image.
  static const double ls=5.0/INCH;
  fprintf(out,"%% The following puts a watermark with some info on how the file was made below the image\n");
  setColor(out,ipc->text_color);
  pushClip(out,xoff,yoff,width,height);
  fprintf(out,"/Courier findfont 4 scalefont setfont\n");
  double y = yoff+height-ls;
  char buf[4096];

  snprintf(buf,sizeof(buf),"filename %s colormap %s",
	   iw->filename.c_str(), iw->colormap_window->label());
  fprintf(out," newpath %g inch %g inch moveto (%s) show\n",
	  xoff, y, buf);
  y -= ls;

  snprintf(buf,sizeof(buf),"hmin %d %e %s hmax %d %e",
	   iw->hmin, iw->id.quant_to_raw(iw->hmin), iw->id.zname.c_str(),
	   iw->hmax, iw->id.quant_to_raw(iw->hmax));
  fprintf(out," newpath %g inch %g inch moveto (%s) show\n",
	  xoff, y, buf);
  y -= ls;

  snprintf(buf,sizeof(buf),"plane_a %g plane_b %g gamma %g gammacenter %g", iw->plane_a, iw->plane_b, iw->gam, iw->gcenter);
  fprintf(out," newpath %g inch %g inch moveto (%s) show\n",
	  xoff, y, buf);
  y -= ls;

  snprintf(buf,sizeof(buf),"imop \"%s\"", iw->operations_string.c_str());
  fprintf(out," newpath %g inch %g inch moveto (%s) show\n",
	  xoff, y, buf);
  y -= ls;

  snprintf(buf,sizeof(buf),"box_int (%d,%d)-(%d,%d)", img_x1, img_y1, img_x2, img_y2);
  fprintf(out," newpath %g inch %g inch moveto (%s) show\n",
	  xoff, y, buf);
  y -= ls;

  snprintf(buf,sizeof(buf),"box (%g,%g)-(%g,%g)", iw->xunit(img_x1)*xscale, iw->yunit(img_y1)*yscale, iw->xunit(img_x2)*xscale, iw->yunit(img_y2)*yscale);
  fprintf(out," newpath %g inch %g inch moveto (%s) show\n",
	  xoff, y, buf);
  y -= ls;

  snprintf(buf,sizeof(buf),"xscale %g yscale %g", xscale, yscale);
  fprintf(out," newpath %g inch %g inch moveto (%s) show\n",
	  xoff, y, buf);
  y -= ls;


  fprintf(out," cliprestore\n");
}

void Image_Printer::calculateBoundingBox()
{
  // Before, we put the bounding box at the end of the file after we
  // calculated all the label widths.

  // However, there is an outstanding ghostscript bug that ignores
  // (atend) bounding boxes gv and gsview both have hacked together
  // work arounds however, if we use ghostscript to convert to png or
  // pdf, the bounding box is completely ignored!
  
  // So, now, we have to hack it in here.
  
  // The y values are easy, since the height of the labels is fixed
  bb_ur_y = (int) ((yoff+height)*INCH + (ipc->do_title->value() ? 2 : 1) * fontsize);
  bb_ll_y = (int) (yoff*INCH - (ipc->xaxis_label->value() ? 3 : 2) * fontsize);

  // The x values are much harder, since we need to estimate the maximum width of
  // the y axis labels and the colorbar labels. 
  //
  // Let's try estimating it from the min an max values of the labels
  
//   double tmp;
//   char buf[128];
//   tmp = iw->yunit((img_y2+img_y1)*0.667)*yscale; // a random uneven number?
//   max_ytic_width = snprintf(buf,sizeof(buf),ipc->yticfmt->value(),tmp); 
//   info("%s\n", buf);

//   // Same for the colorbar

//   tmp = iw->getz((iw->hmin+iw->hmax)*0.667)*zscale;
//   max_ctic_width = snprintf(buf,sizeof(buf),ipc->yticfmt->value(),tmp);
//   info("%s\n", buf);

  // the above doesn't work so well either...
  // let's just overestimate the bounding box a bit...
  // it would be better to calculate the tics ahead of time...

  max_ytic_width = 10;
  max_ctic_width = 10;

  int label_x = static_cast<int>(- (max_ytic_width+2.5)*fontsize/2.0);
  bb_ll_x = (int) (xoff*INCH - (ipc->yaxis_label->value() ? 1 : -1) * fontsize + label_x); 

  int label_c = static_cast<int>((max_ctic_width+2.5)*fontsize/2.0);
  cbar_width = ipc->cbar_width->value();
  cbar_xoff = xoff + width + 0.3;

  if(ipc->colorbar->value())
    bb_ur_x = (int) ((cbar_xoff+cbar_width)*INCH + label_c + (ipc->caxis_label->value() ? fontsize : 0));
  else
    bb_ur_x = (int) ((xoff+width)*INCH + fontsize);

  info("Label width estimates:\n");
  info("ytic %d ctic %d\n", max_ytic_width, max_ctic_width);
  info("%%%%BoundingBox: %d %d %d %d\n", bb_ll_x, bb_ll_y, bb_ur_x, bb_ur_y); 

  max_ytic_width = 0;
  max_ctic_width = 0;
}
  
void Image_Printer::write_header()
{
  
  // First, reset the bouding box
  bb_ll_x = bb_ll_y = 0;

  // Now that we to include support for printing the area selected
  // using the zoom window (or a manually set area), we will create
  // some variables here that store the area of the image

  if (ipc->paper_eps->value() && (!preview))
    fprintf(out,"%%!PS-Adobe-3.0 EPSF-3.0\n");
  else 
    fprintf(out,"%%!PS-Adobe-3.0\n");

  // Add a paper description and orientation for nice cooperation with
  // gs: however, for some reason, ps2pdf seems to ignore the paper
  // settings we put in the postscript file and just uses Letter...
  if (ipc->paper_letter->value())
    {
      bb_ur_x = 612;
      bb_ur_y = 792;
    }
  else if (ipc->paper_a4->value())
    {
      bb_ur_x = 595;
      bb_ur_y = 842;
    }
  
  if (ipc->paper_eps->value())
    {
      calculateBoundingBox();
      fprintf(out, "%%%%BoundingBox: %d %d %d %d\n", bb_ll_x, bb_ll_y, bb_ur_x, bb_ur_y); 
      if(preview)
	fprintf(out, "%%%%Pages: 1\n");
      else
	fprintf(out, "%%%%Pages: 0\n");
      fprintf(out,"%%%%Orientation: Portrait\n");
    }
  else
    {
      // The PNG and PDF files will be displayed with rotated pages if we do this.
      //if (ipc->landscape->value()) 
      //{
      //	  int tmp = bb_ur_x;
      //	  bb_ur_x = bb_ur_y;
      //  bb_ur_y = tmp;
      //}
      // the above doesn't really work...
      if (ipc->paper_letter->value())
	fprintf(out, "%%%%DocumentMedia: Letter %d %d 0 () ()\n", bb_ur_x,bb_ur_y);
      else if (ipc->paper_a4->value())
	fprintf(out, "%%%%DocumentMedia: A4 %d %d 0 () ()\n",bb_ur_x,bb_ur_y);

      fprintf(out, "%%%%Orientation: %s\n", ipc->landscape->value() ? "Landscape" : "Portrait");
      fprintf(out, "%%%%Pages: 1\n");
    }

  // DSC comments that don't change with ps/eps...
  fprintf(out,"%%%%DocumentData: Clean7Bit\n");
  fprintf(out,"%%%%Creator: Spyview PS Output\n");
  fprintf(out,
	  "%%%% Spyview parameters:\n"
	  "%%%% colorplotmin = %e ; colorplotmax = %e\n"
	  "%%%% gamma = %e\n"
	  "%%%% rawdata_min = %e, rawdata_max = %e\n",
	  iw->getz(iw->hmin)*zscale, iw->getz(iw->hmax)*zscale, 
	  iw->gam, iw->id.rawmin, iw->id.rawmax);
  fprintf(out,"%%%%EndComments\n\n");


  // Distiller magic
  fprintf(out,
	  "%%%%BeginSetup\n"
	  "%% The following makes distiller not jpeg compress the spyview image.\n"
	  "systemdict /setdistillerparams known {\n"
	  "<< /AutoFilterColorImages false /ColorImageFilter /FlateEncode >>\n"
	  "setdistillerparams\n"
	  "} if\n"
	  "/inch {72 mul} bind def\n"
	  "/rightshow %% stk: string; show a string right aligned at the current location\n"
	  "{ dup stringwidth pop 0 exch sub 0 rmoveto show } def\n"
	  "/centershow %% stk: string show a string centered horizontally at the current location\n"
	  "{ dup stringwidth pop 0 exch sub 0.5 mul 0 rmoveto show } def\n"
	  "%%%%EndSetup\n");

  if(ipc->paper_eps->value())
    {
      // Tricky part: estimate a nice bounding box for an EPS file; the pagesize will crop the output pdf nicely if we get it close.
      int label_c = static_cast<int>((6)*fontsize/2.0);
      if(ipc->colorbar->value())
	bb_ur_x = (int) ((cbar_xoff+cbar_width)*INCH + label_c + (ipc->caxis_label->value() ? fontsize : 0));
      else
	bb_ur_x = (int) ((xoff+width)*INCH + fontsize);
      bb_ur_y = (int) ((yoff+height)*INCH + (ipc->do_title->value() ? 2 : 1) * fontsize);

      // This is causing problems with ghostscript 8.54: strictly
      // speaking, EPS files should not have a page size. In newer
      // versions of ghostscript, if a page size is found, it
      // overrides the bounding box for conversion to, for example,
      // PDF and PNG files.
      //fprintf(out,"<< /PageSize [%d %d] >> setpagedevice\n",bb_ur_x,bb_ur_y);
    }

  if (!ipc->paper_eps->value() || preview)
    fprintf(out, "%%%%Page: 1 1\n");
  fprintf(out,"gsave\n");
}

void Image_Printer::draw_image()
{

  // Draw the image
  fprintf(out,"%% Draw the image\n");
  //fprintf(out,"/imgstr %d string def\n\n", iw->w);
  fprintf(out,"/imgstr %d string def\n\n", img_xsize);
  fprintf(out,"gsave\n  %g inch %g inch translate\n", xoff,yoff);
  fprintf(out,"  %g inch %g inch scale\n",width,height);
  //fprintf(out,"  %d %d %% Columns and Rows\n", iw->w, iw->h);
  fprintf(out,"  %d %d %% Columns and Rows\n", img_xsize, img_ysize);
  fprintf(out,"  8 %% bits per color channel\n");
  //fprintf(out,"  [%d 0 0 %d 0 0]\n",iw->w, iw->h); // Map the unit square to our image
  fprintf(out,"  [%d 0 0 %d 0 0]\n",img_xsize, img_ysize); // Map the unit square to our image
  fprintf(out,"    { currentfile imgstr readhexstring pop } false 3 colorimage\n");
  fprintf(out,"      ");

  // number of columns before a newline in the postscript image
  static const int maxcol=72;

  for(int i = img_y2; i >= img_y1; i--)
    {
      int c = 0;
      for(int j = img_x1; j <= img_x2; j++)
	{
	  c += 6;
	  if(c > maxcol)
	    {
	      fprintf(out,"\n      ");
	      c = 6;
	    }
	  unsigned char r,g,b;
	  if(i >= 0 && i < iw->h &&  j >= 0 && j < iw->w) // This is a horrible hack, but I'm too lazy to work out the right bounds.
	    iw->getrgb(i,j,r,g,b);
	  else
	    {
	      r = g = b = 255;
	    }
	  fprintf(out,"%02x%02x%02x",r,g,b);
       
	}
      fprintf(out,"\n      ");
    }
  fprintf(out,"  grestore\n\n");
}

void Image_Printer::draw_lines()
{
  // We want to draw the linecut before the bounding box and the tic
  // marks, so it comes out beneath them.
  bool clipped=false;

  if(ipc->plotLineCut->value() && (iw->line_cut_type != NOLINE)) // Plot the line for the linecut.  We use the ticmark settings at the moment, which is stupid but easy.
    {
      pushClip(out,xoff,yoff,width,height);
      clipped=true;      
      fprintf(out,"%% Plot the linecut location\n");
      setColor(out, ipc->linecut_color);      
      fprintf(out," %g setlinewidth newpath\n",tickwidth);
      double x1,y1,x2,y2,t;
      switch(iw->line_cut_type)
	{
	case HORZLINE:
	  x1 = xoff; x2 = xoff + width;
	  y1=iw->line_cut_yp;
	  pixelToDevice(t,y1);
	  y2=y1;
	  break;
	case VERTLINE:
	  x1=iw->line_cut_xp;
	  pixelToDevice(x1,t);
	  x2=x1;
	  y1 = yoff; y2 = yoff + height;
	  break;
	case OTHERLINE:
	  x1=iw->lcx1;
	  x2=iw->lcx2;
	  y1=iw->lcy1;
	  y2=iw->lcy2;
	  pixelToDevice(x1,y1);
	  pixelToDevice(x2,y2);
	  break;
	default:
	  warn("Unknown line cut type!\n");
	  exit(1);
	}
      fprintf(out,"     %g inch %g inch moveto\n",x1,y1);
      fprintf(out,"     %g inch %g inch lineto\n",x2,y2);
      fprintf(out,"stroke\n");
    }

  if(ipc->plotZoomBox->value() && iw->zoom_window && iw->zoom_window->visible())
    {
      int x1,y1,x2,y2;
      iw->zoom_window->getSourceArea(x1,y1,x2,y2);
      double x1d, y1d, x2d, y2d;
      x1d = x1; y1d = y1; x2d = x2; y2d = y2;
      pixelToDevice(x1d,y1d);
      pixelToDevice(x2d,y2d);
      setColor(out,ipc->zoombox_color);
      fprintf(out," %g setlinewidth newpath\n",tickwidth);
      fprintf(out," %g inch %g inch moveto\n",x1d,y1d);
      fprintf(out," %g inch %g inch lineto\n",x1d,y2d);
      fprintf(out," %g inch %g inch lineto\n",x2d,y2d);
      fprintf(out," %g inch %g inch lineto\n",x2d,y1d);
      fprintf(out," %g inch %g inch lineto\n",x1d,y1d);
      fprintf(out," stroke\n");
    }
  if (ipc->plotLines->value() && !LineDrawer->lines.empty())
    {
      if(!clipped)
	pushClip(out,xoff,yoff,width,height);
      clipped=true;
      fprintf(out,"%% Plot the lines\n");
      setColor(out,ipc->overlay_color);
      fprintf(out,"%g setlinewidth newpath\n",tickwidth);
      double lx = NAN;
      double ly = NAN;

      for(LineDraw::lines_t::iterator i = LineDrawer->lines.begin(); i != LineDrawer->lines.end(); i++)
	{
	  double x1,y1,x2,y2;
	  x1=i->x1*xscale;
	  x2=i->x2*xscale;
	  y1=i->y1*yscale;
	  y2=i->y2*yscale;
	  unitToDevice(x1,y1);
	  unitToDevice(x2,y2);
	  if(Clip(x1,y1,x2,y2,xoff,yoff,xoff+width,yoff+height))
	    {
	      if(!(isnan(x1) || isnan(y1) || isnan(x2) || isnan(y2)))
		{
		  if((x1 != lx) || (y1 != ly)) // Skip unnecessary moves.
		    fprintf(out,"     %g inch %g inch moveto\n",x1,y1);
		  fprintf(out,"     %g inch %g inch lineto\n",x2,y2);
		}
	      lx = x2;
	      ly = y2;
	    }
	  else
	    {
	      lx = NAN;
	      ly = NAN;
	    }
	}
      fprintf(out,"stroke\n");
    }
  if(clipped)
    {
      fprintf(out," cliprestore\n");
      clipped=false;
    }
}

void Image_Printer::draw_colorbar()
{
  int cbimgx, cbimgy;
  if (ipc->rotate_cbar->value())
    {
      cbar_height = ipc->cbar_width->value();
      cbar_width = height*ipc->cbar_height_per->value()/100.0;
      cbar_xoff = xoff + (width-cbar_width)/2;
      cbar_yoff = yoff + height+0.3;
      cbimgx = iw->colormap_length;
      cbimgy = 1;
    }
  else
    {
      cbar_width = ipc->cbar_width->value();
      cbar_height = height*ipc->cbar_height_per->value()/100.0;
      cbar_xoff = xoff + width + 0.3;
      cbar_yoff = yoff + (height - cbar_height)/2;
      cbimgx = 1;
      cbimgy = iw->colormap_length;
    }



  if (!ipc->linear_cmap->value())
    {
      // Draw a colorbar with a non-linear color mapping but a linear tic spacing
  
      fprintf(out,"%% Draw the colorbar\n");
      fprintf(out,"/imgstr %d string def\n\n", 1);
      fprintf(out,"gsave\n  %g inch %g inch translate\n", cbar_xoff, cbar_yoff);
      fprintf(out,"  %g inch %g inch scale\n",cbar_width, cbar_height);
      fprintf(out,"  %d %d %% Columns and Rows\n", cbimgx, cbimgy);
      fprintf(out,"  8 %% bits per color channel\n");
      fprintf(out,"  [%d 0 0 %d 0 0]\n", cbimgx, cbimgy); // Map the unit square to our image
      fprintf(out,"    { currentfile imgstr readhexstring pop } false 3 colorimage\n");

      int tmp;
      for(int i = 0; i < iw->colormap_length; i++)
	{
	  unsigned char r,g,b;
	  tmp = iw->gammatable[i];
	  r = iw->colormap[3*tmp];
	  g = iw->colormap[3*tmp+1];
	  b = iw->colormap[3*tmp+2];
	  fprintf(out,"%02x%02x%02x",r,g,b);
	  fprintf(out,"\n      ");
	}
      fprintf(out,"  grestore\n\n");

      // Draw the colorbar box
  
      if (boxwidth > 0)
	{
	  fprintf(out, "%% Draw the box around the colorbar\n");
	  setColor(out,ipc->border_color);
	  fprintf(out,"  %g setlinewidth newpath\n",boxwidth);
	  box(out,cbar_xoff,cbar_yoff,cbar_width,cbar_height);
	  fprintf(out,"  stroke\n");
	}

      // Draw the tic marks on the colorbox
      fprintf(out,"/%s findfont %g scalefont setfont\n",ipc->face->value(),fontsize);
      fprintf(out,"  %g setlinewidth\n", tickwidth);
      if(ipc->cticfmt->size() != 0)
	{
	  double cs;
	  double cmin = iw->getz(iw->hmin)*zscale;
	  double cmax = iw->getz(iw->hmax)*zscale;
	  int tic_width;
      
	  if(ipc->cspacing->value() != 0)
	    cs = fabs(ipc->cspacing->value());
	  else
	    cs=autospace(cmin,cmax);

	  if(cmin > cmax)
	    cs=-cs;
      
	  // Tic marks should be on integers (1.0, 1.5,... not 1.37,1.87,...) and should not be outside the frame
	  double min = cs*ceil(cmin/cs);
	  double tic_offset, wrange;
	  if (ipc->precise_ticks->value())
	    {
	      wrange=cbar_height*(iw->colormap_length-1)/(iw->colormap_length);  // cmin is actually 1/2 picel from the left edge, xmax is 1/2 pixel from the right.
	      tic_offset = cbar_height*(0.5/(iw->colormap_length));
	    }
	  else 
	    {
	      wrange=cbar_height;
	      tic_offset=0;
	    }
	  //	  warn("min %g cmin %g cmax %g cs %g zero %g\n",min,cmin,cmax,cs,zero);
	  for(double c = min; c*cs <= cmax*cs + zero*cs*fabs(cs); c += cs)
	    {
	      //	      warn("%g %g %g %g\n",c,c*cs,cmax*cs,zero*cs*abs(cs));
	      //if(x < xmin)
	      //continue;
	      char buf[128];
	      buf[0] = 0; // a good default
	      if (ipc->cticfmt->value()[0] == 'e')
		{
		  const char *p = ipc->cticfmt->value()+1;
		  long int dig = strtol(p, (char **)NULL, 10); // strtol returns zero if no digits are found
		  if (dig == 0) dig = 2; // a reasonable default?
		  strncpy(buf, eng(c, dig, 1), 128);
		}
	      else if (ipc->cticfmt->value()[0] == 's')
		{
		  const char *p = ipc->cticfmt->value()+1;
		  long int dig = strtol(p, (char **)NULL, 10); // strtol returns zero if no digits are found
		  if (dig == 0) dig = 2; // a reasonable default?
		  strncpy(buf, eng(c, dig, 0), 128);
		}
	      else
		{
		  if(fabs(c/cs) < zero)
		    snprintf(buf,sizeof(buf),ipc->cticfmt->value(),0.0);
		  else
		    snprintf(buf,sizeof(buf),ipc->cticfmt->value(),c);
		}
	      info("cvalue %e label '%s'\n", c, buf);
	      tic_width = strlen(buf);

	      if (tic_width > max_ctic_width) max_ctic_width = tic_width;
	  
	      double psx = cbar_xoff+cbar_width+0.25*fontsize/INCH; //
	      double psy = cbar_yoff + wrange * ((c-cmin)/(cmax-cmin)) + tic_offset;
	      setColor(out,ipc->text_color);
	      fprintf(out,"  newpath %g inch %g inch moveto (%s) show\n",psx,psy-0.25*fontsize/INCH,buf);

	      if(ipc->fancy_ticks->value())
		{
		  setColor(out,ipc->large_tick_color);
		  fprintf(out,"%g setlinewidth newpath\n",tickwidth*2);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff,psy,ticklength+tickwidth/72.0);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff+cbar_width,psy,-ticklength-tickwidth/72.0);     
		  setColor(out,ipc->small_tick_color);
		  fprintf(out,"%g setlinewidth newpath\n",tickwidth);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff,psy,ticklength);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff+cbar_width,psy,-ticklength);     
		}
	      else
		{
		  setColor(out,ipc->small_tick_color);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff,psy,ticklength);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff+cbar_width,psy,-ticklength);     
		}
	    }
	}
    }
  else // Draw a colorbar with a linear color scale but with a non-linear tic mark scale
    {

      // First draw the image for inside the colorbar

      int length = iw->colormap_length;
      int start = ipc->cmin->value()*length;
      int end = ipc->cmax->value()*length;

      fprintf(out,"%% Draw the colorbar\n");
      fprintf(out,"/imgstr %d string def\n\n", 1);
      fprintf(out,"gsave\n  %g inch %g inch translate\n", cbar_xoff, cbar_yoff);
      fprintf(out,"  %g inch %g inch scale\n",cbar_width, cbar_height);
      fprintf(out,"  %d %d %% Columns and Rows\n", 1, end-start);
      fprintf(out,"  8 %% bits per color channel\n");
      fprintf(out,"  [%d 0 0 %d 0 0]\n", 1, end-start); // Map the unit square to our image
      fprintf(out,"    { currentfile imgstr readhexstring pop } false 3 colorimage\n");

      for(int i = start; i < end; i++)
	{
	  unsigned char r,g,b;
	  r = iw->colormap[3*i];
	  g = iw->colormap[3*i+1];
	  b = iw->colormap[3*i+2];
	  fprintf(out,"%02x%02x%02x",r,g,b);
	  fprintf(out,"\n      ");
	}
      fprintf(out,"  grestore\n\n");

      // Draw the colorbar box
  
      if (boxwidth > 0)
	{
	  setColor(out,ipc->border_color);
	  fprintf(out, "%% Draw the box around the colorbar\n");
	  fprintf(out,"  %g setlinewidth newpath\n",boxwidth);
	  box(out,cbar_xoff,cbar_yoff,cbar_width,cbar_height);
	  fprintf(out,"  stroke\n");
	}

      // Now, we want to do something a bit tricky: map the colors on
      // the "linear" color scale onto actual data values for tick
      // marks. The first thing we need to do is make what I will call
      // a "reverse map". Using this table, we can get a datapoint
      // value based on it's color value

      // We have to be a bit careful with rounding, particularly for
      // small gammas
      int l = iw->colormap_length;
      int reverse_map[l];
      for (int i=0; i<l; i++)
	reverse_map[i] = -1;
      for (int i=iw->hmin; i<iw->hmax; i++)
	{
	  int index = 1.0*iw->imagehist[i]*(l-1)/LMAX;
	  if (reverse_map[index] == -1) reverse_map[index] = i;
	}
      reverse_map[0] = iw->hmin;
      for (int i=0; i<l; i++)
	if (reverse_map[i] == -1) reverse_map[i] = reverse_map[i-1];
	
      // Now try to work out what the beginning color should be such
      // that we try to end up with a label at the value that the user
      // has asked for

      // TODO: FIX this at some point...
      
      double definitive_label = ipc->cbegin->value(); // will leave this name unchanged to avoid having to rewrite settings code...
      double c1=-1;
      // Get the quantized value from the raw value
      int val_quant = iw->id.raw_to_quant(definitive_label);
      // Get the position in the un-gammed colormap (float between 0 and 1)
      double cmap_position = iw->imagehist[val_quant]/LMAX; 
      info("estimated cmap position is %f\n", cmap_position);
      if (c1 < ipc->cmin->value()) c1 = ipc->cmin->value();

      // Draw the tic marks on the colorbox
      fprintf(out,"/%s findfont %g scalefont setfont\n",ipc->face->value(),fontsize);
      fprintf(out,"  %g setlinewidth\n", tickwidth);
      char buf[256];
      if(ipc->cticfmt->size() != 0)
	{
	  for (double c=c1; c < ipc->cmax->value(); c += ipc->clabelspacing->value())
	    {
	      int i = (int) (c*(l-1));
	      double val = iw->id.quant_to_raw(reverse_map[i]);

	      buf[0] = 0; // a good default
	      if (ipc->cticfmt->value()[0] == 'e')
		{
		  const char *p = ipc->cticfmt->value()+1;
		  long int dig = strtol(p, (char **)NULL, 10); // strtol returns zero if no digits are found
		  if (dig == 0) dig = 2; // a reasonable default?
		  strncpy(buf, eng(c, dig, 1), 128);
		}
	      else if (ipc->cticfmt->value()[0] == 's')
		{
		  const char *p = ipc->cticfmt->value()+1;
		  long int dig = strtol(p, (char **)NULL, 10); // strtol returns zero if no digits are found
		  if (dig == 0) dig = 2; // a reasonable default?
		  strncpy(buf, eng(c, dig, 0), 128);
		}
	      else
		snprintf(buf,sizeof(buf),ipc->cticfmt->value(),c);
	      info("cvalue %e label '%s'\n", c, buf);
	      int tic_width = strlen(buf);

	      if (tic_width > max_ctic_width) max_ctic_width = tic_width;

	      double psx = cbar_xoff + cbar_width + 0.25*fontsize/INCH; 
	      double psy = cbar_yoff + cbar_height * ((c-ipc->cmin->value())/(ipc->cmax->value()-ipc->cmin->value()));
		  
	      // Draw the label
	      setColor(out,ipc->text_color);
	      fprintf(out,"  newpath %g inch %g inch moveto (%s) show\n",psx,psy-0.25*fontsize/INCH,buf);

	      // Draw the tic lines
	      if(ipc->fancy_ticks->value())
		{
		  setColor(out,ipc->large_tick_color);
		  fprintf(out,"%g setlinewidth newpath\n",tickwidth*2);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff,psy,ticklength+tickwidth/72.0);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff+cbar_width,psy,-ticklength-tickwidth/72.0);     
		  setColor(out,ipc->small_tick_color);
		  fprintf(out,"%g setlinewidth newpath\n",tickwidth);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff,psy,ticklength);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff+cbar_width,psy,-ticklength);     
		}
	      else
		{
		  setColor(out,ipc->small_tick_color);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff,psy,ticklength);
		  fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",cbar_xoff+cbar_width,psy,-ticklength);     
		}
	    }
	}
    }
}

void Image_Printer::draw_axis_tics()
{

  // *****************
  // Draw the xtics
  // *****************

  // Draw the tick marks  
  fprintf(out,"/%s findfont %g scalefont setfont\n",ipc->face->value(),fontsize);
  fprintf(out,"  %g setlinewidth\n", tickwidth);
  if(ipc->xticfmt->value()[0] != '\0')
    {
      double xs;
      double xmin = iw->xunit(img_x1)*xscale;
      double xmax = iw->xunit(img_x2)*xscale;
      
      if(xspacing != 0)
	xs = fabs(xspacing);
      else
	xs=autospace(xmin,xmax);

      if(xmin > xmax)
	xs=-xs;
      
      // Tic marks should be on integers (1.0, 1.5,... not 1.37,1.87,...) and should not be outside the frame
      double min = xs*ceil(xmin/xs);
      double tic_offset, wrange;
      if (ipc->precise_ticks->value())
	{
	  wrange=width*(img_xsize-1)/(img_xsize);  // xmin is actually 1/2 pixel from the left edge, xmax is 1/2 pixel from the right.
	  tic_offset = width*(0.5/(img_xsize));
	}
      else 
	{
	  wrange=width;
	  tic_offset=0;
	}
      for(double x = min; x*xs <= xmax*xs + zero*xs*fabs(xs); x += xs)
	{
	  //if(x < xmin)
	  //continue;
	  char buf[128];
	  buf[0] = 0; // a good default
	  if (ipc->xticfmt->value()[0] == 'e')
	    {
	      const char *p = ipc->xticfmt->value()+1;
	      long int dig = strtol(p, (char **)NULL, 10); // strtol returns zero if no digits are found
	      if (dig == 0) dig = 2; // a reasonable default?
	      strncpy(buf, eng(x, dig, 1), 128);
	    }
	  else if (ipc->xticfmt->value()[0] == 's')
	    {
	      const char *p = ipc->xticfmt->value()+1;
	      long int dig = strtol(p, (char **)NULL, 10); // strtol returns zero if no digits are found
	      if (dig == 0) dig = 2; // a reasonable default?
	      strncpy(buf, eng(x, dig, 0), 128);
	    }
	  else
	    {
	      if(fabs(x/xs) < zero)
		snprintf(buf,sizeof(buf),ipc->xticfmt->value(),0.0);
	      else
		snprintf(buf,sizeof(buf),ipc->xticfmt->value(),x);
	    }
	  //info("xvalue %e label '%s'\n", x, buf);

	  double psy = yoff-1.0*fontsize/INCH; //
	  double psx = xoff + wrange * ((x-xmin)/(xmax-xmin)) + tic_offset;
	  setColor(out,ipc->text_color);
	  fprintf(out,"  newpath %g inch %g inch moveto (%s) centershow\n",psx,psy,buf);
	  if(ipc->fancy_ticks->value())
	    {
	      setColor(out,ipc->large_tick_color);
	      fprintf(out,"%g setlinewidth newpath\n",tickwidth*2);
	      fprintf(out,"  newpath %g inch %g inch moveto 0 %g inch rlineto stroke\n",psx,yoff,ticklength+tickwidth/72.0);
	      fprintf(out,"  newpath %g inch %g inch moveto 0 %g inch rlineto stroke\n",psx,yoff+height,-ticklength-tickwidth/72.0);
	      setColor(out,ipc->small_tick_color);
	      fprintf(out,"%g setlinewidth newpath\n",tickwidth);
	      fprintf(out,"  newpath %g inch %g inch moveto 0 %g inch rlineto stroke\n",psx,yoff,ticklength);
	      fprintf(out,"  newpath %g inch %g inch moveto 0 %g inch rlineto stroke\n",psx,yoff+height,-ticklength);
	    }
	  else
	    {
	      setColor(out,ipc->small_tick_color);
	      fprintf(out,"  newpath %g inch %g inch moveto 0 inch %g inch rlineto stroke\n",psx,yoff,ticklength);
	      fprintf(out,"  newpath %g inch %g inch moveto 0 inch %g inch rlineto stroke\n",psx,yoff+height,-ticklength);
	    }
	}
      

    }

  // *****************
  // Draw the ytics
  // *****************

  if(ipc->yticfmt->value()[0] != '\0')
    {
      double ys;
      // we have some issues due to the fact that "y=0" for the fltk
      // drawing routines is at the top of the screen...we must
      // remember that ymax = y1 and ymin = y2...
      double ymin = iw->yunit(img_y2)*yscale; 
      double ymax = iw->yunit(img_y1)*yscale;

      if(yspacing != 0)
	ys = fabs(yspacing);
      else
	ys=autospace(ymin,ymax);

      if(ymin > ymax)
	ys=-ys;

      int tic_width;
      
      // This is horrible; everything changes when ys and ymin change sign.
      double min = ys*ceil(ymin/ys);
      double hrange, tic_offset;
      if (ipc->precise_ticks->value())
	{
	  hrange = height*(img_ysize-1)/(img_ysize);
	  tic_offset = height*(0.5/(img_ysize)); 
	}
      else
	{
	  hrange = height;
	  tic_offset = 0;
	}
      for(double y = min; y*ys <= ymax*ys + zero*ys*fabs(ys); y += ys)
	{
	  //	  warn("%g\n",y);
	  char buf[128];
	  buf[0] = 0; // a good default
	  if (ipc->yticfmt->value()[0] == 'e')
	    {
	      const char *p = ipc->yticfmt->value()+1;
	      long int dig = strtol(p, (char **)NULL, 10); // strtol returns zero if no digits are found
	      if (dig == 0) dig = 2; // a reasonable default?
	      strncpy(buf, eng(y, dig, 1), 128);
	    }
	  else if (ipc->yticfmt->value()[0] == 's')
	    {
	      const char *p = ipc->yticfmt->value()+1;
	      long int dig = strtol(p, (char **)NULL, 10); // strtol returns zero if no digits are found
	      if (dig == 0) dig = 2; // a reasonable default?
	      strncpy(buf, eng(y, dig, 0), 128);
	    }
	  else
	    {
	      if(fabs(y/ys) < zero)
		snprintf(buf,sizeof(buf),ipc->yticfmt->value(),0.0);
	      else
		snprintf(buf,sizeof(buf),ipc->yticfmt->value(),y);
	    }
	  //info("yvalue %e label '%s'\n", y, buf);
	  tic_width = strlen(buf);

	  if (tic_width > max_ytic_width) max_ytic_width = tic_width;
	  double psx = xoff-0.25*fontsize/INCH; //
	  double psy = yoff + hrange * ((y-ymin)/(ymax-ymin)) + tic_offset;
	  setColor(out,ipc->text_color);
	  fprintf(out,"  newpath %g inch %g inch moveto (%s) rightshow\n",psx,psy-0.25*fontsize/INCH,buf);

	  if(ipc->fancy_ticks->value())
	    {
	      setColor(out,ipc->large_tick_color);
	      fprintf(out,"%g setlinewidth newpath\n",tickwidth*2);
	      fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",xoff,psy,ticklength+tickwidth/72.0);
	      fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",xoff+width,psy,-ticklength-tickwidth/72.0);
	      setColor(out,ipc->small_tick_color);
	      fprintf(out,"%g setlinewidth newpath\n",tickwidth);
	      fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",xoff,psy,ticklength);
	      fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",xoff+width,psy,-ticklength);
	    }
	  else
	    {
	      setColor(out,ipc->small_tick_color);
	      fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",xoff,psy,ticklength);
	      fprintf(out,"  newpath %g inch %g inch moveto %g inch 0 rlineto stroke\n",xoff+width,psy,-ticklength);
	    }
	}
    }
}

void Image_Printer::draw_axis_labels()
{

  // Draw the labels
  
  double label_x, label_y;
  
  setColor(out,ipc->text_color);
  if (ipc->yaxis_label->value())
    {
      // Notes: as a nasty hack, assume an average font aspect ration
      // of 2:1. Also, offset us by an extra 3.5 font widths (note
      // max_ytic_width includes terminating null)

      // Would probably be better to keep a copy of the longest ytic
      // string and then have the postscript interpret the
      // width for us...but this seems to work reasonably well.

      label_x = xoff - (max_ytic_width+2.5)*fontsize/2.0/INCH;
      label_y = yoff + height/2;
      fprintf(out, " newpath %g inch %g inch moveto (%s) 90 rotate centershow -90 rotate\n", label_x, label_y, iw->id.yname.c_str());
    }

  if (ipc->caxis_label->value() && ipc->colorbar->value())
    {
      label_x = cbar_xoff + cbar_width + (max_ctic_width+3.5)*fontsize/2.0/INCH;
      label_y = cbar_yoff + cbar_height/2;
      fprintf(out, " newpath %g inch %g inch moveto (%s) 90 rotate centershow -90 rotate\n", label_x, label_y, iw->id.zname.c_str());
    }
  
  if (ipc->xaxis_label->value())
    {
      label_x = xoff + width/2;
      label_y = yoff - 2.5*fontsize/INCH;
      fprintf(out, " newpath %g inch %g inch moveto (%s) centershow\n", label_x, label_y, iw->id.xname.c_str());
    }

  if (ipc->do_title->value())
    {
      label_x = xoff + width/2;
      label_y = yoff + height + 1.0*fontsize/INCH;
      char buf[1024]; 
      char bn[1024];
      strncpy(bn, iw->output_basename, sizeof(bn));
      char *p = basename(bn);
      info("%s -> %s\n", iw->output_basename, p);
      snprintf(buf, 1024, ipc->title->value(), p, iw->id.zname.c_str());
      // Postscript needs to have double \\ to display properly
      string name = buf;
      string::size_type pos=0;
      while ((pos = name.find("\\", pos+2)) != string::npos)
	name.replace(pos, 1, "\\\\");
      fprintf(out, " newpath %g inch %g inch moveto (%s) centershow\n", label_x, label_y, name.c_str());
    }
}

void Image_Printer::add_dirstamp()
{
  double label_x, label_y;
  if  (ipc->paper_eps->value())
    {
      label_x = bb_ur_x/INCH - 0.25;
      label_y = bb_ll_y/INCH - 0.25;
      bb_ll_y -= 0.25*INCH + 5;
    }
  else
    {
      label_x = page_width - 0.25;
      label_y = 0.25;
    }
  char buf[PATH_MAX];
  getcwd(buf, PATH_MAX);
  string name = buf;
  string::size_type pos=0;
  while ((pos = name.find("\\", pos+2)) != string::npos)
    name.replace(pos, 1, "\\\\");
  fprintf(out,"/%s findfont %g scalefont setfont\n",ipc->face->value(),10.0);
  fprintf(out, " newpath %g inch %g inch moveto (%s) rightshow\n", label_x, label_y, name.c_str());
}
