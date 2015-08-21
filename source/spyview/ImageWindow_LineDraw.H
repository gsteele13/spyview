#ifndef __IMAGEWINDOW_LINEDRAW_H__
#define __IMAGEWINDOW_LINEDRAW_H__
class ImageWindow;

#include <list>
#include "ImageWindow_Module.H"

class LineDraw : public ImageWindow_Module
{
public:
  LineDraw(ImageWindow *iwp);
  virtual ~LineDraw();
  char xsection_fn[1024]; // Name of crossection etmp file.
  class line
  {
  public:
    double x1, y1, x2, y2; // These are stored as united positions, so use pixel positions, so use getX_inv(x1) to get pixel locations
    line(double x1p=0, double y1p=0, double x2p=0, double y2p=0) : x1(x1p), y1(y1p), x2(x2p), y2(y2p) {}; 
    inline void offset(double dx, double dy) { x1 += dx; x2 += dx; y1 += dy; y2 += dy; };
    inline double theta() { return atan2(y2-y1,x2-x1); };
    inline double length() { return sqrt((y2-y1)*(y2-y1)+(x2-x1)*(x2-x1)); };
  };

  typedef std::list <line> lines_t;
  lines_t lines;
  line edit_original;
  line *editing;
  double *edit_x;
  double *edit_y;
  double edit_x_orig;
  double edit_y_orig;
  bool first_on;

  line *nearest_line;

  bool on(); // Return true if linedraw extension is on.
  bool toggle(); // Enable/disable the linedraw extension

  double push_x, push_y;
  line ltmp;
  typedef enum { LINE_OFF = 0, LINE_DRAWING, LINE_DRAGGING, LINE_STRETCHING } mode_t;
  mode_t mode;

  struct stat last_gp_update;  // Last stat info on the gnuplot table file.
  bool valid;                  // True if the gnuplot table existed when last checked
  bool tracking;               // True if we currently believe the table changed
  void loadGnuplotLines(const char *fname); // Load lines from a gnuplot table format file
  double autoLoadGnuplotLines(); // Load lines from gnuplot table file, only if it's new.  Return appropriate amount of time to wait for next check.
  void watchGnuplot(); // Begin watching gnuplot file for changes.
  void loadOldLines();
  void update_file();
  void update_info_win();
  line *find_closest_line(double x1, double y1);
  void saveLines(const char* filename);
  void loadLines(const char* filename);

  int event_callback(int event);
  void overlay_callback();
  void linecut_callback(bool init);
};

extern LineDraw *LineDrawer;
#endif
