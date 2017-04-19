#ifndef ImageWindow3d_H
#define ImageWindow3d_H

#include <config.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <stdlib.h>
#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <stdio.h>
#include <vector>
#include <math.h>
#include <string>
#include <string.h>
#include <limits.h>
#include "gl2ps.h"
#include "Image2D.H"

#define DATATYPE double
#define POINTS 1
#define LINEMESH 2
#define POLYGONS 3

#define POINTSOURCE 1
#define DIRECTIONAL 2



#define LMAX USHRT_MAX

void inform(char *message);

class ImageWindow3d : public Fl_Gl_Window {
protected:
  void draw();

  // Rigid body rotation euler angles
  
  float theta;
  float phi;
  float psi;
  
  // Translation of the surface 
  
  float translatex;
  float translatey;
  float translatez;

  // Scaling of the surface

  float scalex;
  float scaley;
  float scalez;
  
  // Drawing functions
  void draw_surface();
  void draw_surface_points();
  void draw_surface_triangle_strips();
  void draw_surface_line_mesh();
  void pick_color(double val);

  void do_lighting();

  // Colormap stuff
  void setColormap(uchar cmap[3][256]);

  // Data structures
  DATATYPE *data;                  // Array that we keep the normalized data in
  Image2D::Image2D *data_matrix;   // Class that we use to keep the original data around

  float yrange, xrange, yscale, xscale;

  bool square;

  int xp;
  int yp;
  int lastxp;
  int lastyp;
  int button_pushed;

public:
  
  // Colormap stuff

  int colormap[3][356];
  double cmapoffset; 
  double cmapwidth;
  double dmin;
  double dmax;
  bool invert;
  bool negate;

  // Polar coordinates of light 
  
  float light_theta;
  float light_phi;
  float light_r;
  int light_type;

  bool lighting; //do lighting?
  bool grey; //show image as grey? (instead of in colors; best when used with lighting)
  bool plot_light_positions;

  float ambient;  //lighting attributes
  float diffuse;
  float specular;

  // Plane subtraction
  void fitPlane();
  double plane_a;
  double plane_b;
  double plane_c;
  bool plane;

  // Return the data with the plane subtraction.
  double get_data(int i, int j);
  
  // Normalize the data
  // Will copy the original data from data_matrix into our "data" arrayl.
  // The data will be normalized from 0 to 255
  // If plane subtraction is enabled, the data will be scaled so that it is normalized
  // after the plane is subtracted.
  void normalize();
  
  // Transformation clearing functions
  
  void remove_all_trans();
  void remove_trans(string trans);

  // Get and set the transformation variables

  void setTheta(float x);
  float getTheta();
  void setPhi(float x);
  float getPhi();
  void setPsi(float x);
  float getPsi();
  void setTranslateX(float x);
  float getTranslateX();
  void setTranslateY(float x);
  float getTranslateY();
  void setTranslateZ(float x);
  float getTranslateZ();
  void setScaleX(float x);
  float getScaleX();
  void setScaleY(float x);
  float getScaleY();
  void setScaleZ(float x);
  float getScaleZ();

  float currentscalex() {return 1.5*scalex/(float)width;};
  float currentscaley() {return 1.5*scaley/(float)height;};
  float currentscalez() {return scalez;};

  // An external function to call when we've changed the value of some
  // of our parameters.
  void (*external_update)();

  int surfacetype; 

  ImageWindow3d(int x,int y,int w,int h,const char *l=0);
  
  void loadData(const char *filename);
  
  //uses gl2ps (save as ps, eps, or LaTeX)
  void saveFile(const char *name, GLint format);

  void op_ac();
  void op_cc(char *filename);
  void op_fft();
  void op_plane();
  void op_lbl();
  void op_log();

  bool autonormalize;
  
  int width;
  int height;

  int get_width();
  int get_height();

  int handle(int event);
  
  static void cross_product(double vector1[3], double vector2[3], double crossproduct[3]);
  void do_normal(int i, int j); //calculates normal of (i,j) and calls appropriate glNormal method
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
#endif
