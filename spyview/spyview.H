#ifndef testImageWindow_h
#define testImageWindow_h
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Box.H>
#include <FL/Fl.H>
#include <string.h>
#include <string>
#include <vector>
#include "spyview_ui.h"

/*void minl_cb(Fl_Widget *, void *);
void maxl_cb(Fl_Widget *, void *);
void dminl_cb(Fl_Widget *, void *);
void dmaxl_cb(Fl_Widget *, void *);*/

extern string sharedir;

void cmapch_cb(Fl_Widget *, void *);
void filech_cb(Fl_Widget *, void *);
void lblb_cb(Fl_Widget *, void *); 
void saveb_cb(Fl_Widget *, void *);
void cmapedit_cb(Fl_Button *, void *);
void reload_cb(Fl_Widget *, void *);
void load_filech(const char *text);
void spypal_cb();  // Called by spypal when colormap updates.
	  
void plotcmapb_cb(Fl_Widget *, void *);
void plothistb_cb(Fl_Widget *, void *);
void autocorrelate_cb(Fl_Widget *, void *);
void crosscorrelate_cb(Fl_Widget *, void *);
void fft_button_cb(Fl_Widget *, void*);
void update_char_queue();
void set_units();
void set_3d_units();

//vector<string> doable_ops; //fft, ac, cc, plane, lbl
//char doable_ops[][10] = {"fft", "ac", "cc ", "plane", "lbl"};

// Why doesn't this work? I'm confused...
//extern vector<string> filenames;
// We'll do this instead...
int check_loaded();
void clear_files();

int add_file(const char *name);
int load_orig_files();
int keyhandler(int);
void readFile(const char *filename);
void readStmImage(const char *name);
void update_widgets();
void spyview_exit();
void drag_and_drop(const char *name);
void read_mtx(const char *filename);
void read_ascii(const char *filename);

void update_title();

void adjustCenterPeak();
class Image_Operation;
void Fetch_ProcWindow_Settings(Image_Operation *); // Get op's, names, etc, from the proc window
void Set_ProcWindow_Settings(Image_Operation *);   // Send op's, names, etc, to the proc window
void Define_Image_Operation(Image_Operation *);    // Add an image operation to the spyview browser
void Add_Image_Operation(Image_Operation *);       // Copy an image operation into the procedure list.

void savesettings(std::string name);
void loadsettings(std::string name);

void saveColors(char *fn=NULL);
void loadColors(char *fn=NULL);

void saveImageProcessing(char *fn=NULL);
void loadImageProcessing(char *fn=NULL);

extern const char *spyviewSettingsDir;

// A DND handler
class Receiver : public Fl_Box {
public:
  // Receiver Ctor
  Receiver(int x,int y,int w,int h) : Fl_Box(x,y,w,h) {
    box(FL_FLAT_BOX);
  }
  // Receiver event handler
  int handle(int event) {
    int ret = Fl_Box::handle(event);
    switch ( event ) {
    case FL_DND_ENTER:          // return(1) for these events to 'accept' dnd
    case FL_DND_DRAG:
    case FL_DND_RELEASE:
      return(1);
    case FL_PASTE:              // handle actual drop (paste) operation
      load_filech(Fl::event_text()); 
      return(1);
    }
    return(ret);
  }
};

#endif
