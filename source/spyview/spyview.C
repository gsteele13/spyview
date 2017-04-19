#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include "../config.h"
#include <FLTK_Serialization.H>
#include "spyview.H"
#include "ImageWindow.H"
#include "spyview_ui.h"
#include "ImageWindow_LineDraw.H"
#include "ImageWindow_Fitting.H"
#include "ThresholdDisplay.H"
#include "message.h"
#include <Fl/fl_ask.H>
#include <string.h>
#include <Fl/Fl_File_Chooser.H>
#include <Fl/filename.H>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>
#include "Fiddle.H"
#include "mypam.h"
#include "misc.h"
#include "spypal.h"
#include "spypal_interface.H"
#include "spypal_import.H"
#include <libgen.h>

//How's this for lazy...?
char **arg_values;
int arg_count;
int opt_index;

vector<string> filenames;
vector<string> cmapfiles;
string default_file;
string current_filename;


bool no_files_given;

// Keep track of what directory we started in
string original_dir("");

// On unix, this will be $HOME/.spyview
// On win32, default will be $APPDATA/spyview
// If SPYVIEW_PREF_DIR exist, it will use $SPYVIEW_PREF_DIR/spyview
string userdir("");

// On unix, default to /usr/share
// On windows, if called using file associations, we can find the full
// path to the spyview.exe. If called from the command line, the user
// should set the SPYVIEW_DIR environment variable.
string sharedir("");

int check_loaded()
{
  return filenames.size();
}

void clear_files()
{
  filech->clear();
  filenames.clear();
}

void spyview_exit()
{
  // Will the iw destructor get called automatically on hide?
  // However, it is important not to delete it before hiding all of
  // the windows, as if the zoom window is open, then it will crash on
  // win32 when it tries to call the draw_overlay() of iw after it's
  // been deleted.

  //delete iw; 

  // Just calling exit(0) will rely on the OS to clear all of the
  // memory. While this seems to work fine on UNIX, on win32, this
  // results in intermittent and unpredictable crashes. According to
  // this reference, http://www3.telus.net/public/robark/, it is
  // better to hide all of the windows, which will call Fl::run() to
  // return more safely (?)

  while( Fl::first_window() )
    Fl::first_window()->hide();  
}

void close_window_callback(Fl_Widget*)
{
  //if (fl_ask("Juriaan, do you really want to close spyview?")) // Who is Juriaan?
  spyview_exit();

  // I had never clicked on the "close" button of the image window
  // before, and so I had never noticed the bug that if you close the
  // ImageWindow, the program doesn't close, but then it is impossible
  // to get the window to appear again.

  // This bug was found by one of my earlier Windows-based spyview
  // adopters, Juriaan. My initial answer was "why would you ever
  // click on the close button of the window?", but I eventually caved
  // and added this exit handler, and the warning message...

}

// Find all the colormaps in a path, using "pretty_path" for the browser
// hierarchy.
void find_cmaps(std::string path, std::string pretty_path)
{
  typedef vector<string> subdirs_t;
  subdirs_t subdirs;
  info("Checking \"%s\" for cmaps (%s)\n", path.c_str(),pretty_path.c_str());
  struct dirent **namelist;
  string fn;
  int n = fl_filename_list(path.c_str(), &namelist, fl_casealphasort);
  int count = 0;
  for (int i = 0; i<n ; i++)
    {
      if ((strstr(namelist[i]->d_name, ".ppm") != NULL) || (strstr(namelist[i]->d_name,".spp") != NULL))
	{
	  fn = namelist[i]->d_name;
	  std::string pretty_fn(fn);
	  pretty_fn.erase(pretty_fn.find_last_of('.'));
	  cmapfiles.push_back(path + fn);
	  int ind = cmapch->add((pretty_path + pretty_fn).c_str(), 0, 0, reinterpret_cast<void *>(cmapfiles.size()-1));
      // if(strstr(namelist[i]->d_name,".ppm") != NULL)
      //   cmapch->menu()[ind].labelfont(FL_HELVETICA_ITALIC);
      // else
      //   cmapch->menu()[ind].labelfont(FL_HELVETICA);
	  count++;	  
	}
      else if (fl_filename_isdir((path+namelist[i]->d_name).c_str()))
	subdirs.push_back(namelist[i]->d_name);
      if ((count != 0) && (count%30 == 0))
	pretty_path += "More/";
   }
  for(subdirs_t::iterator i = subdirs.begin(); i != subdirs.end(); i++)
    {
      if(*i == "./" || *i == "../" || *i == "")
	continue;
      find_cmaps(path+*i,pretty_path+*i);
    }
}

int add_file(const char *name)
{
  // We store the actual filenames in a STL vector of strings
  // called "filenames" 
  
  // In "filech", we put a "user friendly" filename in, and we
  // store the index location of the real filename in the
  // userdata.
  
  string menu_text;
  int fd;
  int n;
  if ( (fd = open(name, O_RDONLY)) != -1)
    {
      filenames.push_back(name);
      n=filech->add("foo", 0, filech_cb, reinterpret_cast<void *>(filenames.size()-1));
      filech->replace(n, name);
      close(fd);
      return n;
    }
  else
    {
      warn("Unable to open file \"%s\": %s\n", name, strerror(errno));
      return -1;
    }
}


// breaks apart a string into substrings separated by a character string
// does not use a strtok() style list of separator characters
// returns a vector of std::strings

std::vector<std::string> Explode (const std::string &inString, const std::string &separator)
{ 
  std::vector<std::string> returnVector;
  std::string::size_type start = 0;
  std::string::size_type end = 0;
  
  while ((end = inString.find (separator, start)) != std::string::npos)
    {
      returnVector.push_back (inString.substr (start, end-start));
      start = end + separator.size();
    }
  
  returnVector.push_back(inString.substr(start, inString.size()));
  return returnVector;
}

// A drag-n-drop handler
void load_filech(const char *text)
{
  info("test is:\n_%s_", text);
  vector <string> files = Explode(text, "\n");
  info("found %d files\n", files.size());

  int n;
  for (unsigned i=0; i<files.size(); i++)
    {
      info("adding file '%s'\n", files[i].c_str());
      n=add_file(files[i].c_str());
      if (n == -1)
	return;
    }
  filech->value(n);
  filech->do_callback();
}

int load_orig_files()
{
  filenames.clear();
  filech->clear();
  for (int i=opt_index; i < arg_count; i++)
    add_file(arg_values[i]);
  no_files_given = false;
  if (filenames.size() == 0)
    {
      no_files_given = true;
      add_file(default_file.c_str());
      if (check_loaded() == 0)
	error("Could not find default image!");
    }
  return 0;
}

/* Callbacks for image operations */
// Callbacks for crop
void cb_crop_to_zoom(Fl_Widget *w, void *p)
{
  int x1,y1,x2,y2;
  if(!iw->zoom_window)
    return;
  iw->zoom_window->getSourceArea(x1,y1,x2,y2);
  char buf[1024];
  snprintf(buf,sizeof(buf),"%d",x1);
  proc_parameters[0]->value(buf);
  snprintf(buf,sizeof(buf),"%d",x2);
  proc_parameters[1]->value(buf);
  snprintf(buf,sizeof(buf),"%d",y1);
  proc_parameters[2]->value(buf);
  snprintf(buf,sizeof(buf),"%d",y2);
  proc_parameters[3]->value(buf);
  proc_parameters[3]->do_callback();
}

void cb_reset_zoom(Fl_Widget *w, void *p)
{
  proc_parameters[0]->value("0");
  proc_parameters[1]->value("0");
  proc_parameters[2]->value("0");
  proc_parameters[3]->value("0");
  proc_parameters[3]->do_callback();
}

// Callbacks for line subtraction
void cb_sub_current_line(Fl_Widget *w, void *p)
{
  int line;
  if(proc_bool_parameters[1]->value())
    line = iw->line_cut_yp;
  else
    line = iw->line_cut_xp;
  char buf[1024];
  snprintf(buf,sizeof(buf),"%d",line);
  proc_parameters[0]->value(buf);
  proc_parameters[0]->do_callback();  
}

void cb_hist2d_autorange(Fl_Widget *w, void *p)
{
  double tmp;
  char buf[1024];

  double min = iw->id.quant_to_raw(iw->hmin);
  double max = iw->id.quant_to_raw(iw->hmax);

  if (fabs(min)>fabs(max))
    tmp = fabs(min);
  else 
    tmp = fabs(max);

  snprintf(buf,sizeof(buf),"%e", -tmp);
  proc_parameters[0]->value(buf);

  snprintf(buf,sizeof(buf),"%e", tmp);
  proc_parameters[1]->value(buf);

  //snprintf(buf,sizeof(buf),"%d",iw->id.height*50);
  snprintf(buf,sizeof(buf),"%d",10000);
  proc_parameters[2]->value(buf);

  proc_parameters[0]->do_callback();
}

void usage()
{
  printf("%s\n",Fl::help);
}


void Update_Status_Bar(int n, bool down);
void embed_colormap();
void showUsedFiles(bool leak); // For debugging
int main(int argc, char **argv)
{
  char c;
  Fl::visual(FL_RGB8|FL_DOUBLE);
  Fl::get_system_colors();
  info("Welcome to spyview\n");
  info("Build stamp: %s\n", BUILDSTAMP);

  char buf[1024];
  original_dir = getcwd(buf, sizeof(buf));
  
#ifdef WIN32

  // This is tricky. Look first for an environment variable. If not
  // found, try to guess from program name, which if we've been
  // launched using file associations, will contain the full
  // executable path. This should make things work without having to
  // do and "install"

  if (getenv("SPYVIEW_DIR") != NULL) // otherwise get a windows crash?
    sharedir = getenv("SPYVIEW_DIR");
  else 
    {
      // Under windows, argv[0] (seems) to get the full program path
      // (when launched by explorer.exe, but not when called from the
      // cmd.exe command line) So we try to split out the path to the
      // executable here.
      sharedir = argv[0]; 
      int c2 = sharedir.find_last_of("\\");
      if(c2 >= 0)
	sharedir.replace(c2,string::npos, "");
      info("SPYVIEW_DIR not found: guessing %s from argv[0]\n", sharedir.c_str());
    }
  if (sharedir.size() == 0)
    {
      warn("Could not find good sharedir: reverting to '.'\n");
      sharedir = ".";
    }
  info("sharedir is %s\n", sharedir.c_str());

  // For win32, we have a couple of options to get the user's
  // Application Data directory: APPDATA, USERPROFILE, or HOMEDRIVE +
  // HOMEPATH + "Application Data\".  For local users, these are all
  // the same.  For network users, they can be different: for example,
  //
  // HOMEPATH=\ HOMEDRIVE=H:  

  // USERPROFILE=C:\Documents and Settings\gsteele
  // APPDATA=\\tudelft.net\staff-homes\S\gsteele\Application Data
  //
  // Ideally, we should store it on the network drive H:
  // (\\tudelft.net\staff-homes\S\gsteele\). However, if we just use
  // APPDATA, it will not work since mingw32 does not support DFS UNC
  // paths. (It does support regular UNC paths, such as
  // \\myserver\gsteele, although forward slashes are not interpreted
  // as directory separators, as they are for local dirctories on
  // WinXP).
  //
  // In the end, it seems to be safest to take HOMEDRIVE + HOMEPATH + "Application Data"

  // Update: 3 Dec 09
  //
  // Due to popular demand, I will now change the default behaviour so
  // that spyview uses the settings files that are in the SHAREDIR,
  // unless there is a SPYVIEW_PREF_DIR set. Note that from the
  // command line, you will need to set SPYVIEW_PREF_DIR.
  
  if(getenv("SPYVIEW_PREF_DIR") != NULL)
      userdir = getenv("SPYVIEW_PREF_DIR");
  //else if((getenv("HOMEDRIVE") != NULL) && (getenv("HOMEPATH") != NULL))
  //{
  //  userdir = getenv("HOMEDRIVE");
  //  userdir = userdir + getenv("HOMEPATH") + "\\Application Data";
  //  userdir += "\\spyview"; 
  //}
  else
    userdir=sharedir;
  info("userdir is %s\n", userdir.c_str());

#else
  if(getenv("HOME"))
    userdir = getenv("HOME");
  else
    userdir = ".";
  userdir += "/.spyview";
  sharedir = SPYVIEW_DATADIR;
#endif
  default_file = sharedir + DIRECTORY_SEPARATOR + "default_image.pgm";
  
  info("def file is %s\n", default_file.c_str());

  string settings_file("");

  int firstarg;  	 
  Fl::args(argc,argv,firstarg);

  init_spypal();
  colormap_callback = spypal_cb;
  spypal_sharedir = sharedir;
  while ((c = getopt(argc, argv, "s:")) != -1)
    {
      switch (c)
 	{
 	case 's':
	  info("found settings file %s\n", optarg);
	  settings_file = userdir + DIRECTORY_SEPARATOR + "settings" + DIRECTORY_SEPARATOR + optarg;
	  break;
 	}
    }
  
  //info("settings file %s\n", settings_file.c_str());
  //getchar();

  make_window();

  // Initialize the optional "modules"; these insert themselves into spyview callbacks to grab
  // keystrokes and draw things.

  LineDraw iwld(iw);
  Fitting  iwf(iw);
  ThresholdDisplay iwtd(iw);
  Fiddle iwfiddle(iw);

  // Set the pointers to the controls window and the process_queue in the image window class
  iw->controls_window = control;
  iw->process_queue = pqueue;
  iw->external_update = update_widgets;
  iw->drag_n_drop = load_filech;

  embed_colormap();

  iw->setGamma(1.0,0.0);
  wpbox->value(iw->wpercent);
  bpbox->value(iw->bpercent);

  //add_image_operations(); // why doesn't this work? must be something funny with the macros...

  Define_Image_Operation(new Image_Operation("neg","Negate numerical data"));
  Define_Image_Operation(new Image_Operation("square","Average pixels to make a square image"));
  Define_Image_Operation(new Image_Operation("autoflip","Flip axes so that more neg end is on left/bottom"));

  Image_Operation sub_fitplane("sub fitplane","Subtract a fitted plane with outlier rejection");
  sub_fitplane.addParameter("Low ", 20.0);
  sub_fitplane.addParameter("High ", 20.0);
  sub_fitplane.addParameter("!Percentiles?", 1);
  Define_Image_Operation(&sub_fitplane);

  Image_Operation sub_plane("sub plane","Subtract a plane with a specified slope");
  sub_plane.addParameter("Vert %", 0.0);
  sub_plane.addParameter("Horiz  %", 0.0);
  Define_Image_Operation(&sub_plane);

  Image_Operation scale_axes("scale axes","Scale the ranges of the X & Y axes");
  scale_axes.addParameter("X scale", 1.0);
  scale_axes.addParameter("Y scale", 1.0);
  Define_Image_Operation(&scale_axes);

  Image_Operation offset_axes("offset axes","Offset the ranges of the X & Y axes");
  offset_axes.addParameter("X offset", 0.0);
  offset_axes.addParameter("Y offset", 0.0);
  Define_Image_Operation(&offset_axes);

  Image_Operation interp("interp","Interpolate the data onto a new grid (bilinear)");
  interp.addParameter("New x size", 200);
  interp.addParameter("New y size", 200);
  Define_Image_Operation(&interp);

  Image_Operation scale_img("scale img","Scale the image data using bilinear interpolation");
  scale_img.addParameter("X scaling", 2.5);
  scale_img.addParameter("Y scaling", 2.5);
  Define_Image_Operation(&scale_img);

    Image_Operation sub_lbl("sub lbl", "Subtract the mean of each image line with outlier rejection");
  sub_lbl.addParameter("Low %", 2.0);
  sub_lbl.addParameter("High %", 90.0);
  sub_lbl.addParameter("Low limit", -1e99);
  sub_lbl.addParameter("High limit", 1e99);
  //sub_lbl.addParameter("!Whole image thr?", 0);
  //sub_lbl.addParameter("!Percentiles?", 1);
  Define_Image_Operation(&sub_lbl);

  Image_Operation sub_cbc("sub cbc","Subtract the mean of each image column with outlier rejection");
  sub_cbc.addParameter("Low ", 2.0);
  sub_cbc.addParameter("High ", 90.0);
  sub_cbc.addParameter("Low limit", -1e99);
  sub_cbc.addParameter("High limit", 1e99);
  //sub_cbc.addParameter("!Whole image thr?", 0);
  //sub_cbc.addParameter("!Percentiles?", 1);
  Define_Image_Operation(&sub_cbc);

  Image_Operation sub_line("sub linecut","Subtract a linecut from every line");
  sub_line.addParameter("Line Num",0);
  sub_line.addParameter("!Horizontal",1);
  sub_line.addParameter(".Current Line",0,cb_sub_current_line);
  Define_Image_Operation(&sub_line);

  Image_Operation outlier_line("outlier","Remove an outlier line from the data");
  outlier_line.addParameter("Line Num",0);
  outlier_line.addParameter("!Horizontal",1);
  outlier_line.addParameter(".Current Line",0,cb_sub_current_line);
  Define_Image_Operation(&outlier_line);

  Define_Image_Operation(new Image_Operation("xderiv","Take an x derivative of the image data"));
  Define_Image_Operation(new Image_Operation("yderiv","Take a y derivative of the image data"));
  Image_Operation ederiv("ederiv","Take an energy derivative of the image data");
  ederiv.addParameter("Negative Scale",1.0);
  ederiv.addParameter("Positive Scale",1.0);
  Define_Image_Operation(&ederiv);

  Image_Operation dderiv("dderiv", "Take a derivative along an arbitrary axis");
  dderiv.addParameter("Theta",0.0);
  Define_Image_Operation(&dderiv);
  Image_Operation gradmag("gradmag", "Take the magnitude of the gradient");
  gradmag.addParameter("Axis Bias [0-1.0]",0.5);
  Define_Image_Operation(&gradmag);

  Image_Operation gamma("power","y = data^p. If p<0 & abs(data)<eps => truncate x to avoid 1/0. If data<0 & p not int => set data=0 to avoid complex numbers"); // a better name
  gamma.addParameter("Power", 0.5);
  gamma.addParameter("Epsilon", 1e-20);
  Define_Image_Operation(&gamma);

  Image_Operation power2("power2","y = b^data");
  power2.addParameter("Base (b)", 10);
  Define_Image_Operation(&power2);

  Define_Image_Operation(new Image_Operation("norm lbl","Stretch the contrast of each line to full scale"));
  Define_Image_Operation(new Image_Operation("norm cbc","Stretch the contrast of each column to full scale"));

  Image_Operation lp("lowpass","Low pass filter the image; 0 for no filtering");
  lp.addParameter("X Width",3.0);
  lp.addParameter("Y Width",3.0);
  lp.addParameter("?Type 0,Gaussian 1,Lorentzian 2,Exponential 3,Thermal",0.0);
  Define_Image_Operation(&lp);

  Image_Operation hp("highpass","High pass filter the image; 0 for no filtering");
  hp.addParameter("X Width",3.0);
  hp.addParameter("Y Width",3.0);
  hp.addParameter("Pass. %",0.0);
  hp.addParameter("?Type 0,Gaussian 1,Lorentzian 2,Exponential",0.0);
  Define_Image_Operation(&hp);

  Image_Operation notch("notch","Notch filter the image; 0 for no filtering");
  notch.addParameter("X Low",2.0);
  notch.addParameter("X High",2.0);
  notch.addParameter("Y Low",2.0);
  notch.addParameter("Y High",3.0);
  Define_Image_Operation(&notch);

  Image_Operation despeckle("despeckle","Despeckle Image Using Median Filter");
  despeckle.addParameter("!X Despeckle",1);
  despeckle.addParameter("!Y Despeckle",0);
  Define_Image_Operation(&despeckle);

  Image_Operation flip("flip","Flip the Image");
  flip.addParameter("!Flip X Axis",0);
  flip.addParameter("!Flip Y Axis",0);
  Define_Image_Operation(&flip);

  Image_Operation flip_endpoints("flip endpoints","Flip axis endpoints");
  flip_endpoints.addParameter("!Flip X Axis",0);
  flip_endpoints.addParameter("!Flip Y Axis",0);
  Define_Image_Operation(&flip_endpoints);

  Image_Operation crop("crop","Crop the Image (0 to keep current, negative possible for lower/right)");
  crop.addParameter("left col #", 0);
  crop.addParameter("right col #", 0);
  crop.addParameter("upper row #", 0);
  crop.addParameter("lower row #", 0);
  crop.addParameter(".Copy Zoom",-1,cb_crop_to_zoom);
  crop.addParameter(".Reset Zoom",-1,cb_reset_zoom);
  Define_Image_Operation(&crop);

  Image_Operation pixel_avg("pixel avg","Down sample image by averaging pixels");
  pixel_avg.addParameter("X pixels", 2);
  pixel_avg.addParameter("Y pixels", 2);
  Define_Image_Operation(&pixel_avg);

  Image_Operation scale("scale data","Scale and center the data (change dynamic range)");
  scale.addParameter("Factor",0.5);
  Define_Image_Operation(&scale);

  Image_Operation offset("offset","Add an offset to the data");
  offset.addParameter("Offset by",0);
  offset.addParameter("!Auto (sub min first)", 1);
  Define_Image_Operation(&offset);

  Image_Operation log("log","Take logarithm (base 10) of intensities");
  log.addParameter("!Auto subtract offset", 0);
  log.addParameter("New min",1e-4);
  Define_Image_Operation(&log);

  Image_Operation sw_find("rm switch", "Remove switches (1 pixel jumps) in the data");
  sw_find.addParameter("Threshold", 300);
  sw_find.addParameter("Avg win",10);
  Define_Image_Operation(&sw_find);

  Image_Operation even_odd("even odd","Extract even or odd rows, optionally flipping odd rows");
  even_odd.addParameter("!Even", 1);
  even_odd.addParameter("!Flip odd rows", 1);
  Define_Image_Operation(&even_odd);

  Image_Operation hist2d("hist2d", "Convert y-axis into a histogram of each column");
  hist2d.addParameter("ymin", -100);
  hist2d.addParameter("ymax", +100);
  hist2d.addParameter("Num bins", 100);
  hist2d.addParameter(".Autorange", -1, cb_hist2d_autorange);
  //hist2d.addParameter("!Flip odd rows", 1);
  Define_Image_Operation(&hist2d);

  Image_Operation vi_to_iv("vi_to_iv", "Convert an VI plot into an IV plot");
  vi_to_iv.addParameter("ymin", -100);
  vi_to_iv.addParameter("ymax", +100);
  vi_to_iv.addParameter("Num bins", 100);
  vi_to_iv.addParameter(".Autorange", -1, cb_hist2d_autorange);
  //hist2d.addParameter("!Flip odd rows", 1);
  Define_Image_Operation(&vi_to_iv);


  Image_Operation shift_data("shift data","Horizontally shift data after a given row");
  shift_data.addParameter("After row", 0);
  shift_data.addParameter("Shift (pix)", 0);
  Define_Image_Operation(&shift_data);

  Image_Operation remove_lines("remove lines","Remove lines, shifting data vertically");
  remove_lines.addParameter("Start row", 0);
  remove_lines.addParameter("# lines", 0);
  Define_Image_Operation(&remove_lines);

  Define_Image_Operation(new Image_Operation("rotate cw","Rotate the image by 90 degrees clockwise"));
  Define_Image_Operation(new Image_Operation("rotate ccw","Rotate the image by 90 degrees counter clockwise"));
  Define_Image_Operation(new Image_Operation("abs","Take the absolute value of the intensities"));
  Define_Image_Operation(new Image_Operation("equalize","Perform a histogramic equalization on the image"));

  // Now sort image operations alphabetically


  // Works on unix, but segfaults on win32?

//   Fl_Browser *b = options;
//   for ( int t=1; t<=b->size(); t++ ) {
//     for ( int r=t+1; r<=b->size(); r++ ) {
//       if ( strcmp(b->text(t), b->text(r)) > 0 ) {
// 	b->swap(t,r);
//       }
//     }
//   }

   Fl_Browser *b = options;
   for ( int t=1; t<=b->size(); t++ ) 
     {
     for ( int r=t+1; r<=b->size(); r++ ) 
       {
       if ( strcmp(b->text(t), b->text(r)) > 0 ) 
	 {
	   //b->swap(t,r);
	   string tmp = b->text(t);
	   void *ptr = b->data(t);
	   b->text(t, b->text(r));
	   b->data(t, b->data(r));
	   b->text(r, tmp.c_str());
	   b->data(r, ptr);
	 }
       }
     }

  

  // Check if these swapped properly
//   Image_Operation *op = (Image_Operation *) b->data(1);
//   info("text %s\n", options->text(1));
//   info("name %s\n", op->name.c_str());
  
  // Now it works fine? I don't know why it wasn't working?

  arg_values = argv;
  arg_count = argc;
  opt_index = optind;

  load_orig_files();

  info("Found "_STF" files\n", filenames.size());

  // Construct a list of colormap files
  // First scan them from /usr/share/spyview/cmaps, then ~/cmaps/ under unix
  // Look in current directory, then location of .exe / cmaps under windows.

  string user_path = userdir + DIRECTORY_SEPARATOR + "cmaps" + DIRECTORY_SEPARATOR;
  string share_path = sharedir +  DIRECTORY_SEPARATOR + "cmaps" + DIRECTORY_SEPARATOR;
  find_cmaps(share_path, "");
  info("Loaded "_STF" color maps from %s.\n",cmapfiles.size(),share_path.c_str());

  if(share_path != user_path)
    {
      find_cmaps(user_path, "~/");  
      info("Loaded "_STF" color maps from %s.\n",cmapfiles.size(),user_path.c_str());
    }

  int ind = cmapch->add("Custom",0,0,(void*)-1);
  // cmapch->menu()[ind].labelfont(FL_HELVETICA_BOLD); // Make spypal stand out as it's different.

  // Update some of the widgets with the default values from the ImageWindow class
  gpusing->value(iw->gp_using_string);
  gpwith->value(iw->gp_with_string.c_str());
  location_fmt->value("%.1f");

#ifdef WIN32
  Gnuplot_Interface::gnuplot_cmd = "\"" + sharedir + "\\pgnuplot.exe\"";
#endif

  // Load the first file
  filech->value(0);

  // Find the first valid cmap
  for(int n = 0; true; n++)
    {
      assert(n <= cmapch->size());
      cmapch->value(n);
      if (!cmapch->mvalue()->submenu() && cmapch->text(n) != NULL) // null entry at end of submenu
	break;
    }

  //filech->do_callback(filech, (void *)argv[1]);
  //normb->do_callback();
  Fl::add_handler(keyhandler);
  //control->show();
  iw->statusCallback = Update_Status_Bar;
  // load the colormap
  loadsettings(settings_file.c_str());
  // Both of these are now called by loadsettings so we don't load the file twice at startup
  //cmapch->do_callback();
  //filech->do_callback();  // load the file before we call show
  Fl_Text_Buffer *helpbuf = new Fl_Text_Buffer();
  help_text->buffer(helpbuf);
  string help_file = sharedir + DIRECTORY_SEPARATOR + "help.txt";
  info("loading help from file %s\n", help_file.c_str());
  helpbuf->loadfile(help_file.c_str());
  helpbuf->append("\nBuild stamp:\n\n");
  helpbuf->append(BUILDSTAMP);
  iw->callback(close_window_callback);
  iw->show(argc,argv);

  Fl::run();

  delete iw; // Make sure we clean up the gnuplot nicely.
}

int keyhandler(int event)
{
  int key;
  int n;

  switch (event)
    {
    case FL_SHORTCUT:
      key = Fl::event_key();
      switch(key)
	{
	case 'd':
	  if (Fl::event_state() & FL_SHIFT)
	    loadImageProcessing(NULL);
	  else if (Fl::event_state() & FL_CTRL)
	    loadColors(NULL);
	  else
	    {
	      loadImageProcessing(NULL);
	      loadColors(NULL);
	    }
	  return 1;
	case 'e':
	  filech->do_callback();
	  return 1;
	case 'u':
	  if (unitswin->shown())
	    unitswin->hide();
	  else
	    unitswin->show();
	  return 1;
	case 'c':
	  if (Fl::event_state() & FL_CTRL)
	    {
	      if (location_window->shown())
		location_window->hide();
	      else
		location_window->show();
	      return 1;
	    }
	  // note falling case!!!
	case FL_Escape:
	  //also for 'c' with no shift
	  if(!(iw->line_cut_limit & NOLINE))
	    return 1;
	  iw->line_cut_type = NOLINE;
	  iw->plotLineCut();
	  iw->redraw();
	  return 1;
	case FL_Right:
	case ' ':
	  n = filech->value();
	  while (true)
	    {
	      n++;
	      if (n == filech->size() - 1)
		n=0;
	      if (filech->text(n) != NULL && strcmp(filech->text(n), "More") != 0)
		break;
	    }
	  filech->value(n);
	  filech->do_callback();
	  return 1;
	case FL_Left:
	case FL_BackSpace:
	  n = filech->value();
	  while (true)
	    {
	      n--;
	      if (n<0)
		n = filech->size()-1;
	      if (filech->text(n) != NULL && strcmp(filech->text(n), "More") != 0)
		break;
	    }
	  filech->value(n);
	  filech->do_callback();
	  return 1;
	case FL_Down:
	case 'j':
	  n = cmapch->value();
	  while (true)
	    {
	      n++;
	      if (n == cmapch->size()-1)
		n=0;
	      cmapch->value(n);
	      if (!cmapch->mvalue()->submenu() && cmapch->text(n) != NULL) // null entry at end of submenu
		break;
	    }
	  cmapch->do_callback();
	  return 1;
	case FL_Up:
	case 'k':
	  n = cmapch->value();
	  while (true)
	    {
	      n--;
	      if (n<0)
		n= cmapch->size()-1;
	      cmapch->value(n);
	      if (!cmapch->mvalue()->submenu() && cmapch->text(n) != NULL)
		break;
	    }
	  cmapch->do_callback();
	  return 1;
	case 't':
	  showUsedFiles(Fl::event_state() & FL_SHIFT);
	  break;
	case 'n':
	  if (Fl::event_state() & FL_SHIFT)
	    {
	      Add_Image_Operation(new Image_Operation("neg", "Negate numerical data"));
	      reload_data();
	      pqueue->select(pqueue->size());
	      pqueue->do_callback();
	      return 1;
	    }
	  else 
	    {
	      normb->do_callback();
	      return 1;
	    }
	case 'q':
	  spyview_exit();
	  return 1; // note that this is required if we're exiting by hiding all the windows!!!!
	case 's':
	  iw->exportMTX(true);
	  return 1;
	case 'v':
	  if (control->visible())
	    control->hide();
	  else
	    control->show();
	  return 1;
	case 'h':
	  if (helpwin->visible())
	    helpwin->hide();
	  else
	    helpwin->show();
	  return 1;
	case 'o':
	  if (normwin->visible())
	    normwin->hide();
	  else
	    normwin->show();
	  return 1;
	case 'x':
	  xsecb->do_callback();
	  return 1;
	case 'f':
	  if (!(Fl::event_state() & FL_CTRL) &&
	      !(Fl::event_state() & FL_SHIFT))
	    {
	      if (iw->pfc.win->visible())
		iw->pfc.win->hide();
	      else
		iw->pfc.win->show();
	      return 1;
	    }
	case 'l':
	  if (Fl::event_state() & FL_CTRL)
	    {
	      if (reload_window->shown())
		reload_window->hide();
	      else
		reload_window->show();
	      return 1;
	    }
	case 'r':
	  iw->setXZoom(1);
	  iw->setYZoom(1);
	  return 1;
	case 'p':
	  if (Fl::event_state() & FL_CTRL)
	    {
	      iw->setupPS();
	      return 1;
	    }
	  else if (Fl::event_state() & FL_ALT)
	    {
	      info("exporting postscript\n");
	      iw->exportPS();
	      return 1;
	    }
	  else if (Fl::event_state() & FL_SHIFT)
	    {
	      iw->imageprinter->ipc->preview_button->do_callback();
	      return 1;
	    }
	  else
	    {
	      if (procwin->shown())
		procwin->hide();
	      else
		procwin->show();
	    }
	  return 1;
	}   
    }
  return 0;
}

void save_cmap_cb(Fl_Button *o, void*)
{
  iw->colormap_window->saveFile();
}

// Called by spypal to indicate the colormap has changed.
void spypal_cb()
{
  intptr_t index;
  index = (intptr_t)(cmapch->mvalue()->user_data());
  if(index != -1)
      cmapch->value(cmapch->find_item("Custom"));
  iw->setColormap(spypal_colormap,spypal_colormap_size);
}

static bool cmap_is_ppm; // Is the current cmap a ppm file?
void cmapedit_cb(Fl_Button *, void *)
{
  assert(spypal); assert(spypal->win);
  if(cmap_is_ppm)
    {
      printf("CMap is a .ppm; launching import window\n");
      spypal->copy_cmap(iw->getColormap(), iw->getColormapLength());
      spypal->import_controls->show();
      spypal->import_update();
      cmap_is_ppm = false;
    }
  if(spypal->win->visible())
    spypal->win->hide();
  else
    spypal->win->show();
  colormap_callback();
}

void cmapch_cb(Fl_Widget *o, void*)
{
  FILE *fp;

  intptr_t index = (intptr_t)(cmapch->mvalue()->user_data());

  static char label[1024];
  // This seems to be an old piece of code: where is the image window
  // being labelled? 
  update_title();
  snprintf(label, 1024, "Colormap %s", cmapch->text());
  (iw->colormap_window)->label(label);

  if(index == -1) // spypal mode!  
    { // Don't show the spypal win; that's what the edit button's for.
      //      spypal->win->show(); 
      spypal_cb();
      cmap_is_ppm = false;
      return;
    }

  assert(index >= 0);
  assert(index < cmapfiles.size());

  //info("userdata for %s is %d\n", cmapch->value(), index);
  const char *filename = cmapfiles[index].c_str();
  //info("loading file _%s_ from index %d, text _%s_\n", cmapfiles[index].c_str(), index, cmapch->text(cmapch->value()));
  spypal->import_controls->hide();

  // File is a spypal colormap
  if(strstr(filename,".spp") != NULL)
    {
      // Don't call spypal_cb; it'll reset the chooser. Hack.
      colormap_callback = NULL;
      spypal->load(filename);

      colormap_callback = spypal_cb;

      iw->setColormap(spypal_colormap,spypal_colormap_size);
      cmap_is_ppm = false;
      return;
    }

  cmap_is_ppm = true;
  spypal->win->hide();
  pixel **image;
  pixval maxval;
  int rows, cols;

  fp = fopen(filename, "rb");
  if (fp == NULL)
    {
      perror(filename);
      exit(-1);
    }

#ifndef debug_read //debug a binary read problen in windows. implemented some nice ppm read/write code, will leave it in here.
  image = ppm_readppm(fp, &cols, &rows, &maxval);
  fclose(fp);

  uchar newcmap[3*rows];
  
  if (cols > 1)
    error("Invalid colormap %s: must contain only one column!\n", filename);
  
  if (maxval != 255)
    error("Invalid colormap %s: color depth must be 8 bit (255 maxval)\n", filename);
  
  for (int i=0; i<rows; i++)
    {
      newcmap[i*3] = image[0][i].r;
      newcmap[i*3+1] = image[0][i].g;
      newcmap[i*3+2] = image[0][i].b;
    }

  ppm_freearray(image, rows);
#else
  char buf[256];
  int raw_file = 1;

  fgets(buf, 256, fp);
  info("_%s_\n", buf);
  if (strncmp(buf, "P3",2) == 0)
    raw_file = 0;
  else if (strncmp(buf, "P6",2) != 0)
    error("Unsupported magic number %s: %s", buf, filename);
  fgets(buf, 256, fp);
  while (buf[0] == '#')
    fgets(buf, 256, fp);
  if (sscanf(buf, "%d %d", &cols, &rows) != 2)
    error("Invalid row/cols in PPM file: %s", filename);
  if (cols > 1)
    error("PPM fils should be only 1 pixel wide: %s", filename);
  fgets(buf, 256, fp);
  if (sscanf(buf, "%d", &maxval) != 1)
    error("Invalid maxval in PPM file: %s", filename);
  if (maxval != 255)
    error("Only 8 bit (255 maxval) ppm files supported: %s", filename);

  info("reading file %d rows from file %s, raw file %d\n", rows, filename, raw_file);
  uchar newcmap[3*rows];
  int n;

  if (raw_file)
    {
      if ((n=fread(newcmap, 1, rows*3, fp)) != rows*3)
	{
	  info("feof %d ferror %d\n", feof(fp), ferror(fp));
	  error("Short read on PPM file (error after %d bytes of %d, offset %x)\n"
		"last read values %x %x %x %x: %s", 
		n, rows*3, n, 
		newcmap[n-4], newcmap[n-3], newcmap[n-2], newcmap[n-1], 
		filename);
	}
    }
  else
    {
      for (int i=0; i<rows; i++)
	{
	  int r,g,b;
	  if (fgets(buf, 256, fp) == NULL)
	    error("Short read on PPM file (%d of %d): %s", i, rows, filename);
	  if (sscanf(buf, "%d %d %d", &r,&g,&b) != 3)
	    error("Could not convert row %d (%s): %s", i, buf, filename);
	  newcmap[i*3] = r;
	  newcmap[i*3+1] = g;
	  newcmap[i*3+2] = b;
	}
    }

#endif
  iw->setColormap(newcmap, rows);
}

void filech_cb(Fl_Widget *, void *)
{
  intptr_t index;
  index = (intptr_t)filech->mvalue()->user_data();
  char *filename = strdup(filenames[index].c_str());
  //info("loading file _%s_ from index %d, text _%s_\n", filename, index, filech->text(filech->value()));
  int old_w, old_h;

  // Fixme; this leaks memory.
  char *dir = strdup(filename);
  dir = dirname(dir);
  
  filename = basename(filename);
  current_filename = filename;
  if (strcmp(dir, ".") == 0)
    dir=strdup(original_dir.c_str());
  info("Changing Directory to: %s\n", dir);
  info("Filename: %s\n", filename);
  chdir(dir);

  //info("Qmin/max: %e %e\n", iw->id.qmin, iw->id.qmax);

  // This is a bit awkward due to my naming of the w and h variables
  // representing the data size, which conflict with the fltk w() and
  // h() functions returning the window size...
  old_w = ((Fl_Widget*) iw)->w(); 
  old_h = ((Fl_Widget*) iw)->h();
  
  // All file loading is now handled inside ImageWindow (including mtx). 
  if (iw->loadData(filename) == -1)
    {
      warn("Error with file %s: reverting to default image\n", filename);
      iw->loadData(default_file.c_str());
    }

  update_widgets();
  
  // Normalize the data if we've been asked
  if (norm_on_load->value())
    iw->normalize();

  // Give us a nice label
  //string label = filename;
  //label = label + " - " + cmapch->text();
  //set_units();
  update_title();
}

void embed_colormap()
{
  control->add(iw->colormap_window);

  iw->colormap_window->resize(colormap_placeholder->x() + Fl::box_dx(colormap_placeholder->box()),
			      colormap_placeholder->y() + Fl::box_dy(colormap_placeholder->box()),
			      colormap_placeholder->w() - Fl::box_dw(colormap_placeholder->box()),
			      colormap_placeholder->h() - Fl::box_dh(colormap_placeholder->box()));

  if(control->visible())
    iw->show();
  else
    iw->colormap_window->set_visible();
}

void update_title()
{
  char buf[256]; 
  if (iw->id.data3d)
    snprintf(buf, 256, "%s %.0f (%.3g to %.3g), (%.3g to %.3g)", 
	     current_filename.c_str(), 
	     indexbox->value(),
	     xmin->value(), xmax->value(),
	     ymin->value(), ymax->value());
  else
    snprintf(buf, 256, "%s %s (%.3g to %.3g), (%.3g to %.3g)", 
	     (iw->mouse_order == 0) ? "(123)" :
	     ((iw->mouse_order == 1) ? "(231)" : "(312)"),
	     current_filename.c_str(), 
	     xmin->value(), xmax->value(),
	     ymin->value(), ymax->value());
  iw->copy_label(buf);
}

void set_units()
{
  // First update image data class
  iw->id.xmin = xmin->value();
  iw->id.xmax = xmax->value();
  iw->id.ymin = ymin->value();
  iw->id.ymax = ymax->value();
  iw->id.xname = xunitname->value();
  iw->id.yname = yunitname->value();
  iw->id.zname = zunitname->value();

  // Note: changing these doesn't make any sense!
  // These should now be manipulated through imageprocessing operations
  // that act on the image data matrix
  //iw->id.zmin = zmin->value();
  //iw->id.zmax = zmax->value();

  // Now update the mtx data class if we have data loaded

  int x,y;
  if (iw->id.data3d)
    {
      switch (dim->value())
	{
	case 0:  // YZ
	  x = 1; 
	  y = 2;
	  break;
	case 1: // ZX
	  x = 2;
	  y = 0;
	  break;
	case 2: // XY
	  x = 0;
	  y = 1;
	  break;
	}
      iw->id.mtx.axismin[x] = xmin->value();
      iw->id.mtx.axismax[x] = xmax->value();
      iw->id.mtx.axismin[y] = ymin->value();
      iw->id.mtx.axismax[y] = ymax->value();
      iw->id.mtx.axisname[x] = xunitname->value();
      iw->id.mtx.axisname[y] = yunitname->value();
      iw->id.mtx.dataname = zunitname->value();
    }

  update_widgets();
  iw->plotLineCut();
  update_title();
}

void set_3d_units()
{
  iw->id.mtx.axismin[0] = mtx_xmin->value();
  iw->id.mtx.axismin[1] = mtx_ymin->value();
  iw->id.mtx.axismin[2] = mtx_zmin->value();
  iw->id.mtx.axismax[0] = mtx_xmax->value();
  iw->id.mtx.axismax[1] = mtx_ymax->value();
  iw->id.mtx.axismax[2] = mtx_zmax->value();
  iw->id.mtx.axisname[0] = mtx_xname->value();
  iw->id.mtx.axisname[1] = mtx_yname->value();
  iw->id.mtx.axisname[2] = mtx_zname->value();

  // We also have to update the image window class
  
  int x,y;
  switch (dim->value())
    {
    case 0:  // YZ
      x = 1; 
      y = 2;
	  break;
    case 1: // ZX
      x = 2;
      y = 0;
      break;
    case 2: // XY
      x = 0;
      y = 1;
      break;
    }

  iw->id.xmin = iw->id.mtx.axismin[x];
  iw->id.xmax = iw->id.mtx.axismax[x];
  iw->id.ymin = iw->id.mtx.axismin[y];
  iw->id.ymax = iw->id.mtx.axismax[y];
  iw->id.xname = iw->id.mtx.axisname[x];
  iw->id.yname = iw->id.mtx.axisname[y];

  update_widgets();
  iw->plotLineCut();
}
  

void saveb_cb(Fl_Widget *, void *)
{
  iw->saveFile();
  //fl_message("File was saved to %s", savebox->value());
  fl_beep(FL_BEEP_MESSAGE);
}

void update_widgets()
{
  int min, max, width, center;
  min = iw->hmin;
  max = iw->hmax;
  width = (max - min);
  center = (max + min)/2;

  minv->value(min);
  minslider->value(min);
  minroller->value(min);
  minv_units->value(iw->id.quant_to_raw(min));

  maxv->value(max);
  maxslider->value(max);
  maxroller->value(max);
  maxv_units->value(iw->id.quant_to_raw(max));

  centerv->value(center);
  centerslider->value(center);
  centerroller->value(center);
  centerv_units->value(iw->id.quant_to_raw(center));

  widthv->value(width);
  widthslider->value(width);
  widthroller->value(width);
  widthv_units->value(iw->id.quant_to_raw(width)-iw->id.quant_to_raw(0));

  gammav->value(iw->gam);
  gammaroller->value(log(iw->gam));
  gammaslider->value(log(iw->gam));
  gcenterv->value(iw->gcenter);
  gcenterroller->value(iw->gcenter);
  gcenterslider->value(iw->gcenter);
  iw->adjustHistogram();
  plothistb->do_callback();
  plotcmapb->do_callback();

  plotcmapb->value(iw->plot_cmap);
  plothistb->value(iw->plot_hist);
  invertb->value(iw->invert);
  negateb->value(iw->negate);
  savebox->value(iw->output_basename);
  
  plane_a->value(iw->plane_a*((double)iw->h/65535.0));
  plane_aroller->value(iw->plane_a*((double)iw->h/65535.0));
  plane_aslider->value(iw->plane_a*((double)iw->h/65535.0));
  plane_b->value(iw->plane_b*((double)iw->w/65535.0));
  plane_broller->value(iw->plane_b*(double)iw->w/65535.0);
  plane_bslider->value(iw->plane_b*((double)iw->w/65535.0));

  cmap_min->value(iw->cmap_min);
  cmap_max->value(iw->cmap_max);

  xzoom_value->value(iw->xzoom);
  yzoom_value->value(iw->yzoom);

  xsize->value(iw->w);
  ysize->value(iw->h);

  // Update the units window

  xunitname->value(iw->id.xname.c_str());
  yunitname->value(iw->id.yname.c_str());
  zunitname->value(iw->id.zname.c_str());

  //qinfo("zname update: %s\nq", iw->id.zname.c_str());

  xmin->value(iw->id.xmin);
  ymin->value(iw->id.ymin);
  zmin->value(iw->id.qmin);

  xmax->value(iw->id.xmax);
  ymax->value(iw->id.ymax);
  zmax->value(iw->id.qmax);

  gpusing->value(iw->gp_using_string);
  gpwith->value(iw->gp_with_string.c_str());
  
  axis_type->value(iw->lc_axis);

  // Update the load settings window

  gp_parse_txt->value(iw->id.mtx.parse_txt);
  gp_delft_raw->value(iw->id.mtx.delft_raw_units);
  gp_delft_set->value(iw->id.mtx.delft_settings);
  gp_col->value(iw->id.gp_column+1);
  a_quant_percent->value(iw->id.auto_quant_percent);

  qmin->value(iw->id.qmin);
  qmax->value(iw->id.qmax);

  if (iw->id.data3d)
    {
      controls3d->activate();
      units3d->activate();
    }
  else
    {
      controls3d->deactivate();
      units3d->deactivate();
    }

  if (iw->id.datfile_type == MATRIX)
    dat_type_mat->setonly();
  else if (iw->id.datfile_type == GNUPLOT)
    dat_type_gp->setonly();
  else if (iw->id.datfile_type == DELFT_LEGACY)
    dat_type_delft->setonly();
  else if (iw->id.datfile_type == DAT_META)
    dat_type_meta->setonly();
  
  //info("update: type is %d\n", iw->id.gpload_type);
  if (iw->id.gpload_type == COLUMNS)
    gp_type_col->setonly();
  else if (iw->id.gpload_type == INDEX)
    gp_type_index->setonly();

  if (iw->window_size_action == KEEPZOOM)
    keep_zoom->setonly();
  else if (iw->window_size_action == KEEPSIZE)
    keep_size->setonly();
  else // if (iw->window)_size_action == RESETZOOM)
    reset_zoom->setonly();
  
  if (iw->id.auto_quant)
    a_quant->setonly();
  else 
    man_quant->setonly();

  // Update MTX dialog boxes

  indexbox->value(iw->id.mtx_index);
  indexslider->value(iw->id.mtx_index);
  indexroller->value(iw->id.mtx_index);
  
  indexbox->maximum(iw->id.mtx.size[iw->id.mtx_cut_type]-1);
  indexslider->maximum(iw->id.mtx.size[iw->id.mtx_cut_type]-1);
  indexroller->maximum(iw->id.mtx.size[iw->id.mtx_cut_type]-1);

  index_value->value(iw->id.mtx.get_coordinate(iw->id.mtx_cut_type,
					       iw->id.mtx_index));

  dim->value((int)iw->id.mtx_cut_type);
  mtx_label->value(iw->id.do_mtx_cut_title);

  mtx_x->value(iw->id.mtx.size[0]);
  mtx_y->value(iw->id.mtx.size[1]);
  mtx_z->value(iw->id.mtx.size[2]);

  mtx_xmin->value(iw->id.mtx.axismin[0]);
  mtx_ymin->value(iw->id.mtx.axismin[1]);
  mtx_zmin->value(iw->id.mtx.axismin[2]);

  mtx_xmax->value(iw->id.mtx.axismax[0]);
  mtx_ymax->value(iw->id.mtx.axismax[1]);
  mtx_zmax->value(iw->id.mtx.axismax[2]);

  mtx_xname->value(iw->id.mtx.axisname[0].c_str());
  mtx_yname->value(iw->id.mtx.axisname[1].c_str());
  mtx_zname->value(iw->id.mtx.axisname[2].c_str());

  xrange->value(iw->line_cut_xauto);
  update_title();
}

void Fetch_ProcWindow_Settings(Image_Operation *op)
{
  for(unsigned i = 0; i < op->parameters.size(); i++)
    {
      Image_Operation::Parameter *p = &(op->parameters[i]);
      if(i > proc_bool_parameters.size()) // All buttons must be at end.
	{
	  assert(p->name.c_str()[0] == '.');
	  break;
	}

      Fl_Check_Button *b = proc_bool_parameters[i];
      Fl_Input *in = proc_parameters[i];
      Fl_Choice *ch = proc_choice_parameters[i];
      switch(p->name.c_str()[0])
	{
	case '!':
	  p->value = b->value();
	  break;
	case '?':
	  p->value = (intptr_t)ch->mvalue()->user_data_;
	  break;
	case '.':
	  p->value = 0;
	  break;
	default:
	  p->value = atof(in->value());
	  break;
	}
    }
  if(last_proc_side == pqueue)
    op->enabled = enable_filter->value();
}

void Set_ProcWindow_Settings(Image_Operation *op)
{
  bool redraw_window = false;
  int bcount = 0;  // Number of buttons so far.
  assert(proc_bool_parameters.size() == proc_parameters.size());
  assert(proc_choice_parameters.size() == proc_parameters.size());

  for(unsigned i = 0; i < proc_button_parameters.size(); i++)
    {
      Fl_Button *but = proc_button_parameters[i];
      but->hide();
      but->callback( (Fl_Callback *) NULL,NULL);
    }
  for(unsigned i = 0; i < op->parameters.size(); i++)
    {
      Image_Operation::Parameter *p = &(op->parameters[i]);
      if(p->name.c_str()[0] == '.') // Button
	{
	  assert(bcount < proc_button_parameters.size());
	  Fl_Button *but = proc_button_parameters[bcount++];
	  but->label(p->name.c_str()+1);
	  but->value(static_cast<int>(p->value));
	  if(p->cb)
	    but->callback(p->cb,NULL);
	  but->show();
	  continue;
	}

      assert(op->parameters.size() > i-bcount);
      Fl_Check_Button *b = proc_bool_parameters[i-bcount];
      Fl_Input *in = proc_parameters[i-bcount];
      Fl_Choice *ch = proc_choice_parameters[i-bcount];
      switch(p->name.c_str()[0])
	{
	case '.':
	  assert(0);
	case '!': // Boolean
	  {
	    if(strcmp(p->name.c_str()+1,b->label()) != 0)
	      redraw_window = true;
	    b->label(p->name.c_str()+1);
	    b->value(static_cast<int>(p->value));
	    b->show();
	    in->hide();
	    ch->hide();
	    break;
	  }
	case '?': // Choice
	  {
	    redraw_window = true; // Lazy lazy lazy
	    char *tmp = strdup(p->name.c_str());
	    char *s1 = strchr(tmp,' ');
	    assert(s1 != NULL);
	    *s1++ = 0; // tmp+1 is now the choice name
	    if(strcmp(tmp+1,ch->label()) != 0)
	      redraw_window = true;
	    ch->copy_label(tmp+1);
	    ch->clear();
	    while(isspace(*s1))
	      s1++;
	    while(1)
	      {
		char *s2 = strchr(s1,' ');
		if(s2 != NULL)
		  *s2 = 0;
		char buf[1024];
		int val;
		if(sscanf(s1," %d,%s ",&val,buf) != 2)
		  {
		    fprintf(stderr,"Error parsing format string \"%s\" from %s\n",s1,p->name.c_str());
		    exit(1);
		  }
		int idx = ch->add(buf,(const char *)NULL,NULL,(void *)val);
		if(val == p->value)
		  ch->value(idx);
		if(s2 == NULL)
		  break;
		else 
		  s1 = s2+1;
	      }
	    b->hide();
	    in->hide();
	    ch->show();
	    free(tmp);
	    break;
	  }
	default:
	  {
	    if(p->name != in->label())
	      redraw_window = true;
	    in->label(p->name.c_str());
	    char buf[1024];
	    snprintf(buf,sizeof(buf),"%g",p->value);
	    in->value(buf);
	    in->show();
	    b->hide();
	    ch->hide();
	    break;
	  }
	}
    }
  for(unsigned i = op->parameters.size()-bcount; i < proc_parameters.size(); i++)
    {
      proc_parameters[i]->hide();
      proc_bool_parameters[i]->hide();
      proc_choice_parameters[i]->hide();
    }
  proc_description->value(op->description.c_str());
  if(last_proc_side == pqueue)
    {
      enable_filter->value(op->enabled);
      enable_filter->activate();
    }
  else
    enable_filter->deactivate();  
  if(redraw_window) // Sometimes, if the labels get longer, we get dead characters in the background.
    procwin->redraw();
}

void Define_Image_Operation(Image_Operation *op)
{
  options->add(op->name.c_str(), op);
}

void Add_Image_Operation(Image_Operation *op)
{
  info("Adding operation: %s\n", op->name.c_str());
  Image_Operation *new_op = new Image_Operation(*op); // Copy so we don't share settings.
  pqueue->add(op->name.c_str(), new_op);
}

void Update_Status_Bar(int key, bool down)
{
  static double ox, oy;

  int i = iw->dounzoom(Fl::event_x(),iw->xzoom);
  int j = iw->dounzoom(Fl::event_y(),iw->yzoom);

  double x = iw->id.getX(i);
  double y = iw->id.getY(j);
			 
  if(ox != x || oy != y)
    {
      static char locbuf[1024];
      static char tmpbuf[1024];
      static char fmt_string[1024];
      static char fmt[1024];

      // Format is set in the Cursor Position window
      snprintf(fmt, sizeof(fmt), "%s", location_fmt->value());

      // Update the location bar at the bottom of the controls window
      sprintf(fmt_string, "%s,%s = %s %%s", fmt, fmt, fmt);
      snprintf(locbuf,sizeof(locbuf),fmt_string, 
	       x,y,iw->dataval(i,j), iw->id.zname.c_str());
      location_bar->label(locbuf);

      // Update the "Cursor Postion" window as well
      snprintf(tmpbuf, sizeof(tmpbuf), fmt, x);
      location_x->value(tmpbuf);
      location_x->label(xunitname->value());
      x_col->value(i);

      snprintf(tmpbuf, sizeof(tmpbuf), fmt, y);
      location_y->value(tmpbuf);
      location_y->label(yunitname->value());
      y_row->value(j);

      snprintf(tmpbuf, sizeof(tmpbuf), fmt, iw->dataval(i,j));
      location_data->value(tmpbuf);
      data_name->value(zunitname->value());

      // Update the zoom window information in the Cursor window

      if (iw->zoom_window)
	{
	  zoom_group->activate();
	  int x1,y1,x2,y2;
	  iw->zoom_window->getSourceArea(x1,y1,x2,y2);
	  
	  zx1->value(x1); 
	  snprintf(tmpbuf, sizeof(tmpbuf), fmt, iw->id.getX(x1));
	  zx1v->value(tmpbuf);

	  zx2->value(x2); 
	  snprintf(tmpbuf, sizeof(tmpbuf), fmt, iw->id.getX(x2));
	  zx2v->value(tmpbuf);
	  
	  zy1->value(y1); 
	  snprintf(tmpbuf, sizeof(tmpbuf), fmt, iw->id.getY(y1));
	  zy1v->value(tmpbuf);
	  
	  zy2->value(y2); 
	  snprintf(tmpbuf, sizeof(tmpbuf), fmt, iw->id.getY(y2));
	  zy2v->value(tmpbuf);
	}
      else
	zoom_group->deactivate();
	  
      ox = x;
      oy = y;
      
    }
  
  static const int event_mask=FL_CTRL|FL_SHIFT;
  static std::map<int, string> h;
// This is where the status bar help goes
// Add entries as needed.
  if(h.size() == 0)
    {
      h[0] = "B1: X Linecut; B2: Y Linecut; B3: Control Win";
      h[FL_CTRL] = "B1: Arb Linecut; B2: Move Endpt; B3: Drag Linecut";
      h[FL_SHIFT] = "B1: Drag Zoom; B3: Move Zoom Window";
    }  
  static int last_state = -1;
  
  int state = Fl::event_state() & event_mask;
  // Horrible hack to handle XWindows shift state bug
  if(key == FL_Shift_L || key == FL_Shift_R)
    {
      if(down)
	state = state | FL_SHIFT;
      else
	state = state & ~FL_SHIFT;
    }
  if(key == FL_Control_L || key == FL_Control_R)
    {
      if(down)
	state = state | FL_CTRL;
      else
	state = state & ~FL_CTRL;
    }

  if(last_state != state)
    {
      // This will automagically make an empty entry if event_state 
      // is not in the help database  
      const char *help = h[state].c_str(); 
      help_bar->value(help);
      last_state = state;
    }
}

void reload_cb(Fl_Widget *, void *)
{
  dirent **dirlist;
  int n;
  
  n = fl_filename_list(".", &dirlist);
  if (replaceb->value()) clear_files();

  for (int i = 0; i<n; i++)
    if (fl_filename_match(dirlist[i]->d_name, reload_text->value()))
      add_file(dirlist[i]->d_name);

  if (filenames.size() == 0)
    {
      fl_alert("No files found!! Reverting to the list of files given on command line.");
      load_orig_files();
    }
  
  filech->value(0);
  filech->do_callback();

  for (int i = n; i > 0;) 
    free((void*)(dirlist[--i]));
  free((void*)dirlist);

}


/* 
  =============== Serialization Code ==========================
  boost stores class versions, not archive versions.  In order to maintain backwards compatability
  for the "top level" objects (of which there are probably too many, because the FLTK widgets aren't
  encapsulated in a class), we introduce a helper class here. */
const char * spyviewSettingsDir; // Ugly ugly ugly -- this belongs in the class, but spyview_ui.C wants it as well.
			      //  Maybe the class shouldn't be static...

class Spyview_Serializer_t
{
public:
  typedef boost::archive::text_iarchive input_archive;
  typedef boost::archive::text_oarchive output_archive;

// Directory setup functions.
  std::string settingsDir;

  void maybe_mkdir(const char *name)
  {
#ifdef WIN32
    if(mkdir(name) == 0)
#else
    if(mkdir(name,0777) == 0)
#endif
      return;
    
    switch(errno)
      {
      case EEXIST:
	return;
      default:
	warn("Unable to create directory \"%s\": %s\n",name,strerror(errno));
	return;
      }
  }

  void initSettingsDir() // Use the system dependent userdir
  {
    string buf = userdir;
    maybe_mkdir(buf.c_str());
    buf = buf + DIRECTORY_SEPARATOR + "settings"; // note += won't work because the cast is wrong...
    settingsDir = buf;
    maybe_mkdir(buf.c_str());
    spyviewSettingsDir = settingsDir.c_str();
  }

// Save/load functions
  void save(std::string &name)
  {
    initSettingsDir();
    if(name.empty())
      {
      name = settingsDir + DIRECTORY_SEPARATOR + "default.svs";
      }
    const char *fname = name.c_str();
    try
      {
	std::ofstream ofs(fname, std::ios::binary);
	if(!ofs.good())
	  throw(1);
	output_archive oa(ofs); //doesn't work under win32?
	oa & (*this);
      }
    catch (boost::archive::archive_exception &e)
      {
	warn("Error saving spyview settings: %s\n",e.what());
      }
    catch (std::exception &e)
      {
	warn("Error saving settings file \"%s\": %s (%s)\n",fname,e.what(),strerror(errno));
      }
    catch (...)
      {
	error("Unknown error from settings file\n");
      }
  };

  void load(std::string &name)
  {
    initSettingsDir();
    if(name.empty())
      {
	
	name = settingsDir + DIRECTORY_SEPARATOR + "default.svs";
      }
    
    const char *fname = name.c_str();
    info("loading settings from %s\n", fname);
    try 
      {
	std::ifstream ifs(fname, std::ios::binary);
	if(!ifs.good()) 
	  {
	    warn("IFS is not good!\n");
	    throw(1);
	  }
	input_archive ia(ifs);
	ia & (*this);
      }
    catch (boost::archive::archive_exception &e)
      {
	warn("Error loading settings %s\nTry overwriting the default settings file to get rid of this error.\n",e.what());
      }
    catch (std::exception &e)
      {
	warn("Spyview serialization error; did not load settings: %s (%s)\nTry overwriting the default settings file to get rid of this error.\n",e.what(),typeid(e).name());
      }
    catch (...)
      {
	warn("Unable to load settings file \"%s\": %s\nTry overwriting the default settings file to get rid of this error.\n",fname,strerror(errno));
      }
  };
  
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & (*iw);

    // Newer versions of spyview hide the extensions on the colormaps.
    // This causes the chooser serialization to ignore extensions so old
    // save files still work.
    push_chooser_equality(chooser_equality_noextension);
    ar & flcast(cmapch);
    pop_chooser_equality();

    ar & flcast(norm_on_load);
    ar & flcast(squareb);
    ar & flcast(dim);

    if (version >= 1)
      ar & flcast(gpwith);
    if (version >= 2)
      {
	ar & iw->id.datfile_type;
	ar & iw->id.gpload_type;
	ar & iw->id.gp_column;
	//ar & iw->id.mtx_cut_type; // This one is quite annoying, actually...
      }
    if (version >= 3)
      {
	ar & iw->id.mtx.parse_txt;
	Fl_Input tmp(0,0,0,0,""); // This used to be ipc.preview_cmd
	//ar & flcast(iw->ipc.preview_cmd);
	ar & flcast(&tmp);
      }
    if (version >= 4)
      {
	ar & iw->id.mtx.delft_raw_units;
      }
    if (version >= 5)
      {
	ar & iw->id.mtx.delft_settings;
      }
    if(version == 6)
      {
	warn("Version 6 archives were bad..  Spypal settings ignored\n");
      }
    if(version >= 7)
      {
	ar & (*spypal);
      }
    if(Archive::is_loading::value)
      {
	wpbox->value(iw->wpercent);
	bpbox->value(iw->bpercent);
	cmrot->value(iw->colormap_rotation_angle);
	cmrot_roller->value(iw->colormap_rotation_angle);
	if (version >= 1)
	  iw->gp_with_string = gpwith->value();
	iw->setXZoom(iw->xzoom); // make sure to call this after updating class variables as it calls update_widgets()
	if(cmapch->mvalue()->user_data() == (void*)-1) // Custom
	  {
	    cmap_is_ppm = false;
	    spypal->recalculate(true);
	  }
      }
  };
};
static Spyview_Serializer_t Spyview_Serializer;

BOOST_CLASS_VERSION(Spyview_Serializer_t, 7); // Increment this if the archive format changes.

void savesettings(std::string name)
{
  Spyview_Serializer.save(name);
}

void loadsettings(std::string name)
{
  Spyview_Serializer.load(name);
  cmapch->do_callback();
  filech->do_callback();
}

/* Motivation: open *always* returns the lowest numbered file descriptor.  So by mapping which ones
   it returs, we can figure out which are in use. */
void showUsedFiles(bool leak) // Simulate a file descriptor leak if leak is true
{
  std::vector<int> desc;
  size_t ulen=80;
  int cnt = 0;
  int used[ulen];

  while(1)
    {
      int f = open(filenames[0].c_str(), O_RDONLY); // A file that should be available in both windows and unix.
      if(f < 0)
	{
	  warn("Unable to check file descriptors!  First .pgm file \"%s\": %s\n",
	       filenames[0].c_str(), strerror(errno));
	  goto cleanup;
	}
      if(f > ulen)
	{
	  close(f);
	  break;
	}
      else
	desc.push_back(f);
    }

  for(int i = 0; i < ulen; i++)
    used[i] = 1;
  for(int i = 0; i < desc.size(); i++)
    {
      if(desc[i] < ulen)
	used[desc[i]] = 0;
    }
  printf("Leak map: ");
  for(int i = 0; i < ulen; i++)
    {
      printf("%d", used[i]);
      if(used[i])
	cnt++;
    }
  printf("\n");
  printf("%d Total descriptors in use\n",cnt);
 cleanup:
  while(desc.size() > leak ? 1 : 0)
    {
      close(desc.back());
      desc.pop_back();
    }
}
//#endif

void loadColors(char *fn)
{
  // Automatically pick filename
  string filename;
  if (fn == NULL)
    {
      filename = iw->output_basename;
      filename += ".colors";
    }
  else
    filename = fn;
  info("loading colors to file %s\n", filename.c_str());
  std::ifstream ifs(filename.c_str());
  if (!ifs.good())
    {
      info("Error opening colors file: %s\n", filename.c_str());
      return;
    } 
  boost::archive::text_iarchive ar(ifs);
  
  // Version 1: stuff to archive:
  //
  // hmin
  // hmax
  // gamma
  // gamma_center
  // quant_min
  // quant_max
  // colormap
  //

  int version;
  ar & version;
  ar & iw->hmin;
  ar & iw->hmax;
  ar & iw->gam;
  ar & iw->gcenter;
  ar & iw->id.qmin;
  ar & iw->id.qmax;
  ar & flcast(cmapch);

  // After loading, we will:
  //
  // - call update_widgets()
  // - turn off autonormalize (spyview gui widget) 
  // - turn off autoquantize (in ImageData)
  // - requantize the data
  // - set the min and max

  norm_on_load->value(0);
  iw->id.auto_quant = false;
  iw->id.quantize();
  cmapch->do_callback();
  iw->adjustHistogram(); // this should remap and replot the data
  update_widgets();
}


void saveColors(char *fn)
{
  // Automatically pick filename
  string filename;
  if (fn == NULL)
    {
      filename = iw->output_basename;
      filename += ".colors";
    }
  else
    filename = fn;
  
  info("saving colors to file %s\n", filename.c_str());
  std::ofstream ofs(filename.c_str());
  if (!ofs.good())
    {
      info("Error saving to colors file: %s\n", filename.c_str());
      return;
    } 
   boost::archive::text_oarchive ar(ofs);

  // Note -- we need to set the version for the save direction, 
  // otherwise it'll be random.
  int version = 1;
  ar & version;
  // All this stuff is in version 1
  ar & iw->hmin;
  ar & iw->hmax;
  ar & iw->gam;
  ar & iw->gcenter;
  ar & iw->id.qmin;
  ar & iw->id.qmax;
  ar & flcast(cmapch);
}

void saveImageProcessing(char *fn)
{
  // Automatically pick filename
  string filename;
  if (fn == NULL)
    {
      filename = iw->output_basename;
      filename += ".img_proc";
    }
  else
    filename = fn;
  info("saving image processing to file %s\n", filename.c_str());
  std::ofstream ofs(filename.c_str());
  if (!ofs.good())
    {
      warn("Error saving to image processing file: %s\n", filename.c_str());
      return;
    } 
  boost::archive::text_oarchive ar(ofs);

  int tmp = pqueue->size(); 
  ar & tmp;
  fprintf(stderr,"Saving %d process entries\n",tmp);
  for(int i = 1; i <= pqueue->size(); i++)
    {		  
      std::string tmp = pqueue->text(i);
      ar & tmp;
      Image_Operation * t = reinterpret_cast<Image_Operation *>(pqueue->data(i));
      ar & t;
    }
}

void loadImageProcessing(char *fn)
{
  // Automatically pick filename
  string filename;
  if (fn == NULL)
    {
      filename = iw->output_basename;
      filename += ".img_proc";
    }
  else
    filename = fn;
  info("loading image_processing from file %s\n", filename.c_str());
  std::ifstream ifs(filename.c_str());
  if (!ifs.good())
    {
      info("Error opening image processing file: %s\n", filename.c_str());
      return;
    } 
  boost::archive::text_iarchive ar(ifs);

  for(int i = 1 ; i <= pqueue->size(); i++)
    delete reinterpret_cast<Image_Operation *>(pqueue->data(i));
  int len;
  pqueue->clear();
  ar & len; // This has the size  of the expected queue
  fprintf(stderr,"Loading %d process entries\n",len);
  for(int i = 1; i <= len; i++)
    {
      std::string tmp;
      ar & tmp;
      Image_Operation *op;
      ar & op;
      pqueue->add(tmp.c_str(),op);
    }
  reload_data();
}

