#include "ImageWindow3d.H"
#include "spyview3d_ui.h"
#include <math.h>
#define POINTS 1
#define LINEMESH 2
#define POLYGONS 3

ImageWindow3d::ImageWindow3d(int x,int y,int w,int h,const char *l) : Fl_Gl_Window(x,y,w,h,l) 
{
    data_matrix = NULL;

    cmapoffset = 0;
    cmapwidth = 1;
    invert = false;

    plane = 0;
    plane_a = plane_b = plane_c = 0;
    
    // this doesn't seem right, i thought it would  be theta = 90 degrees for the rotation I wanted (top view)
    //although it depends on the initial orientation of the GL context
    // the initial gl context is with Z axis pointing into screen, so theta is actually initial -180.
    theta = -180; 
    phi = 0; 
    psi = 0.0;

    scalex = 1.5;
    scaley = 1.5;
    scalez = 1.0;

    translatex = 0.0;
    translatey = 0.0;
    translatez = 0.0;

    light_theta = 90;
    light_phi = 0;
    light_r = 1;
    plot_light_positions = true;
    light_type = DIRECTIONAL;
    ambient = 0.2;
    diffuse = 1;
    specular = 0.0;
    external_update = NULL;

    surfacetype = POLYGONS;
    mode(FL_RGB|FL_DEPTH|FL_DOUBLE);
}

// Surface transformations:
//
// Rigid body rotation is defined by three Euler angles:
//
// Phi:             (x0,y0,z0) -> (x1, y1, z1=z0)
//   Rotation about initial z0 axis (will rotate final tilted z3 around initial z0)
// Theta:           (x1,y1,z1) -> (x2=x1, y2, z2)
//   Rotation about axis x1 (rotate z2 away from initial z axis)
// Psi:             (x2,y2,z2) -> (x3, y3, z3=z2)
//   Rotation about axis z2, rotate body about it's final z axis
//  
// Goldstien page 146.
//
// By setting phi = 90, we will always be tilting the surface away or towards us if
// we adjust theta. This is the desired behaviour for the mouse interaction (this is the
// same as gnuplot and matlab use).

void ImageWindow3d::setTheta(float x) { theta = fmod(x,360.0); redraw(); }
float ImageWindow3d::getTheta() { return theta; }

void ImageWindow3d::setPhi(float x) {  phi = fmod(x,360.0); redraw(); }
float ImageWindow3d::getPhi() {  return phi; }

void ImageWindow3d::setPsi(float x) {  psi = fmod(x,360.0); redraw(); }
float ImageWindow3d::getPsi() {  return psi; }

void ImageWindow3d::setTranslateX(float x) {  translatex = x; redraw(); }
float ImageWindow3d::getTranslateX() {  return translatex; }

void ImageWindow3d::setTranslateY(float x) {  translatey = x; redraw(); }
float ImageWindow3d::getTranslateY() {  return translatey; }

void ImageWindow3d::setTranslateZ(float x) {  translatez = x; redraw(); }
float ImageWindow3d::getTranslateZ() {  return translatez; }

void ImageWindow3d::setScaleX(float x) {  scalex = x; redraw(); }
float ImageWindow3d::getScaleX() {  return scalex; }

void ImageWindow3d::setScaleY(float x) {  scaley = x; redraw(); }
float ImageWindow3d::getScaleY() {  return scaley; }

void ImageWindow3d::setScaleZ(float x) {  scalez = x; redraw(); }
float ImageWindow3d::getScaleZ() {  return scalez; }

void ImageWindow3d::setColormap(uchar cmap[3][256])
{
  for (int i=0; i < 256; i++)
    {
      colormap[0][i] = cmap[0][i];
      colormap[1][i] = cmap[1][i];
      colormap[2][i] = cmap[2][i];
    }
  redraw();
}

void ImageWindow3d::remove_trans(string trans) 
{
  if (trans == "angle") 
    {
      theta = -180;
      phi = 0.0;
      psi = 0.0;
    }
  if (trans == "translate") 
    {
      translatex = 0.0;
      translatey = 0.0;
      translatez = 0.0;
    }
  if (trans == "scale") 
    {
      scalex = 1.5;
      scaley = 1.5;
      scalez = 1.0;
    }
  if (external_update != NULL)
    (*external_update)();
}

void ImageWindow3d::remove_all_trans() 
{
  theta = -180;
  phi = 0.0;
  psi = 0.0; 
  translatex = 0.0;
  translatey = 0.0;
  translatez = 0.0;
  scalex = 1.5;
  scaley = 1.5;
  scalez = 1.0;
}

void ImageWindow3d::draw() 
{
  if (!valid()) 
    {
      glLoadIdentity();
      glViewport(0,0,w(),h());
      glEnable(GL_DEPTH_TEST);
    }
  glDrawBuffer(GL_BACK);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glPushMatrix();
  glDisable(GL_LIGHTING);
  glDisable(GL_LIGHT0);

  glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS, 64);

  //fprintf(stderr, "%f %f %f\n", scalex, scaley, scalez);
  //fprintf(stderr, "%f %f %f\n", phi, theta, psi);
  glLoadIdentity();
  glTranslatef(translatex, translatey, translatez);
  glRotatef(phi,   0,0,1);
  glRotatef(theta, 1,0,0);     
  glRotatef(psi,   0,0,1);
  if (lighting) 
      do_lighting();
  glScalef(scalex, scaley, scalez);
  draw_surface();
  glPopMatrix();
}

void ImageWindow3d::draw_surface() 
{
  if (surfacetype == POINTS) 
    draw_surface_points();
  else if (surfacetype == POLYGONS)
    draw_surface_triangle_strips();
  else if (surfacetype == LINEMESH)
    draw_surface_line_mesh();
}

void ImageWindow3d::draw_surface_points() 
{
  glBegin(GL_POINTS);
  for (int i = 0; i < height; ++i) 
    {
      for (int j = 0; j < width; ++j) 
	{
	  DATATYPE x = ((DATATYPE)j-0.5*(DATATYPE)width)/width;
	  DATATYPE y = ((DATATYPE)i-0.5*(DATATYPE)height)/height;
	  DATATYPE z = get_data(i,j)/255-0.5; 
	  pick_color(get_data(i,j));
	  glVertex3d(x,y,z);
	}
    }
  glEnd();
}

double ImageWindow3d::get_data(int i,int j)
{
  if (plane)
    {
      //if (i == 0 & j == 0) fprintf(stderr, "a b c %f %f %f\n", plane_a , plane_b, plane_c);
      return data[i*width+j] - (plane_a * (i-width/2) + plane_b*(j-height/2) + plane_c);
    }
  else
    return data[i*width+j];
}
    

void ImageWindow3d::pick_color(double val)
{
  val = (val-128-cmapoffset*255)/cmapwidth + 128;
  if (val<0) val = 0;
  if (val>255) val = 255;
  if (negate)
    val = 255 - val;
  if (grey)
    glColor3ub(100,100,100);
  else
    {
      if (!invert)
	glColor3ub(colormap[0][(int)val],colormap[1][(int)val],colormap[2][(int)val]);
      else
	glColor3ub(255-colormap[0][(int)val],255-colormap[1][(int)val],255-colormap[2][(int)val]);
    }
}

void ImageWindow3d::draw_surface_triangle_strips() 
{
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  for (int j = 0; j < width - 1; ++j) 
    {
      glBegin(GL_TRIANGLE_STRIP);
      for (int i = 0; i < height - 1; ++i) 
	{
	  DATATYPE x1 = ((DATATYPE)j-0.5*(DATATYPE)width)/width;
	  DATATYPE y1 = ((DATATYPE)i-0.5*(DATATYPE)height)/height;
	  DATATYPE z1 = get_data(i,+j)/255 - 0.5;

	  DATATYPE x2 = ((DATATYPE)(j+1)-0.5*(DATATYPE)width)/width;
	  DATATYPE y2 = ((DATATYPE)i-0.5*(DATATYPE)height)/height;
	  DATATYPE z2 = get_data(i,+j+1)/255 - 0.5;
      
	  do_normal(i,j);
	  pick_color(get_data(i,+j));
	  glVertex3d(x1,y1,z1);
	  do_normal(i+1,j+1);
	  pick_color(get_data(i,+j+1));
	  glVertex3d(x2,y2,z2);
	}
      glEnd();
    }
}

void ImageWindow3d::draw_surface_line_mesh() 
{
  glBegin(GL_LINES);
  for (int i = 0; i < height - 1; ++i) 
    {
      for (int j = 0; j < width - 1; ++j) 
	{
	  DATATYPE x1 = ((DATATYPE)j-0.5*(DATATYPE)width)/width;
	  DATATYPE y1 = ((DATATYPE)i-0.5*(DATATYPE)height)/height;
	  DATATYPE z1 = get_data(i,+j)/255 - 0.5;

	  DATATYPE x2 = ((DATATYPE)(j+1)-0.5*(DATATYPE)width)/width;
	  DATATYPE y2 = ((DATATYPE)i-0.5*(DATATYPE)height)/height;
	  DATATYPE z2 = get_data(i,+j+1)/255 - 0.5;

	  DATATYPE x3 = ((DATATYPE)j-0.5*(DATATYPE)width)/width;
	  DATATYPE y3 = ((DATATYPE)(i+1)-0.5*(DATATYPE)height)/height;
	  DATATYPE z3 = get_data((i+1),+j+1)/255 - 0.5;
      
	  pick_color(get_data(i,+j+1));
	  glVertex3d(x1,y1,z1);
	  glVertex3d(x2,y2,z2);

      	  pick_color(get_data((i+1),+j));
	  glVertex3d(x1,y1,z1);
	  glVertex3d(x3,y3,z3);
	}
    }
  glEnd();
}

void ImageWindow3d::do_lighting()   //does the necessary GL magic for lighting conditions.
{
  double x,y,z;
  x = light_r * cos(light_phi/180*M_PI) * sin(light_theta/180*M_PI);
  y = light_r * sin(light_phi/180*M_PI) * sin(light_theta/180*M_PI);
  z = light_r * cos(light_theta/180*M_PI);
  
  float lightposition0[4] = {x,y,z,0};
  if (light_type == POINTSOURCE)
    lightposition0[3]= 1;
  else if (light_type == DIRECTIONAL)
    lightposition0[3] = 0;
  float lightambient0[4] = {ambient,ambient,ambient,1.0};
  float lightdiffuse0[4] = {diffuse,diffuse,diffuse,1.0};
  float lightspecular0[4] = {specular,specular,specular,1.0};

  glLightfv(GL_LIGHT0, GL_AMBIENT, lightambient0);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lightdiffuse0);
  glLightfv(GL_LIGHT0, GL_SPECULAR, lightspecular0);
  glLightfv(GL_LIGHT0, GL_POSITION, lightposition0);

  if (plot_light_positions) 
    {
      int materialemission_lights[4] = {1,1,1,1};
      glMaterialiv(GL_FRONT_AND_BACK, GL_EMISSION, materialemission_lights);
      glPointSize(10.0);
      glEnable(GL_POINT_SMOOTH);
      glColor3f(255.0,255.0,255.0);
      glBegin(GL_POINTS);
    
      glNormal3f(0.0,0.0,-1.0);
      glVertex3fv(lightposition0);

      glEnd();
      glPointSize(1.0);
      glDisable(GL_POINT_SMOOTH);
      int materialemission[4] = {0,0,0,1};
      glMaterialiv(GL_FRONT_AND_BACK, GL_EMISSION, materialemission);
    }

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  // why does this not enable specular lighting? It seems to always use ambient and diffuse.
  glEnable(GL_COLOR_MATERIAL);
}

void ImageWindow3d::cross_product(double vector1[3], double vector2[3], double crossproduct[3]) 
{
  crossproduct[0] = (vector1[1]*vector2[2]-vector1[2]*vector2[1]);
  crossproduct[1] = (vector1[2]*vector2[0]-vector1[0]*vector2[2]);
  crossproduct[2] = (vector1[0]*vector2[1]-vector1[1]*vector2[0]);
}

void ImageWindow3d::do_normal(int i, int j) 
{
  if ( (i < height - 1) && (j < width - 1) ) 
    {
      DATATYPE x1 = ((DATATYPE)j-0.5*(DATATYPE)width)/width;
      DATATYPE y1 = ((DATATYPE)i-0.5*(DATATYPE)height)/height;
      DATATYPE z1 = get_data(i,+j)/255 - 0.5;

      DATATYPE x2 = ((DATATYPE)(j+1)-0.5*(DATATYPE)width)/width;
      DATATYPE y2 = ((DATATYPE)i-0.5*(DATATYPE)height)/height;
      DATATYPE z2 = get_data(i,+j+1)/255 - 0.5;
      
      DATATYPE x3 = ((DATATYPE)j-0.5*(DATATYPE)width)/width;
      DATATYPE y3 = ((DATATYPE)(i+1)-0.5*(DATATYPE)height)/height;
      DATATYPE z3 = get_data((i+1),+j)/255 - 0.5;
      
    
      DATATYPE vector1[3] = {x2-x1,y2-y1,z2-z1};
      DATATYPE vector2[3] = {x3-x1,y3-y1,z3-z1};
    
      double normalvector[3];
      cross_product((double *)vector1, (double *)vector2, normalvector);
  
      //calculate length of results from cross_product and then
      //normalize the vector
      double length = sqrt(normalvector[0]*normalvector[0]+normalvector[1]*normalvector[1]+normalvector[2]*normalvector[2]);
      if (length) 
	{
	  normalvector[0] /= length;
	  normalvector[1] /= length;
	  normalvector[2] /= length;
	}
      glNormal3dv(normalvector);
    }
}


int ImageWindow3d::get_width() 
{
  return width;
}

int ImageWindow3d::get_height() 
{
  return height;
}


// This is kindof silly: We have a class that has function for loading and doing calculation, which we instantiate,
// then use, then copy the data out of, then delete later. 

void ImageWindow3d::loadData(const char *name) 
{
  zap(data);
  if (data_matrix != NULL) 
    {
      delete data_matrix;
      data_matrix = NULL;
    }
  data_matrix = new Image2D::Image2D(name);

  for (int i=1; i<=pqueue->size(); i++)
    {
      if (strcmp(pqueue->text(i),"fft") == 0) 
	op_fft();
      if (strcmp(pqueue->text(i),"ac") == 0) 
	op_ac();
      if (strcmp(pqueue->text(i),"sub lbl") == 0) 
	op_lbl();
      if (strcmp(pqueue->text(i), "log") == 0) 
	op_log();
    }

  width = data_matrix->width;
  height = data_matrix->height;

  zap(data);
  normalize();
}

void ImageWindow3d::normalize()
{
  // Copy "data_matrix" into "data" 
  // Also, always make it square if the width is and interger multiple of the height
  
  int navg = 1;
  if ((width>height) && (width%height == 0)) 
    {
      navg = width/height;  
      width = height;
    }

  data = new DATATYPE [width*height];

  for (int i = 0; i < width*height; ++i) 
    {
      data[i] = 0;
      for (int m=0; m<navg; m++)
	data[i] += data_matrix->data[i*navg+m];
      data[i] /= navg;
    }

  // Let's find the min and the max, then nomalize the data vertically from 0 to 255
  dmax = -1e100;
  dmin = 1e100;
  double d;

  for (int i = 0; i < width*height; ++i) 
    {
      d = data[i];
      if (d < dmin) dmin = d;
      if (d > dmax) dmax = d;
    }
   for (int i = 0; i < width*height; ++i) 
     data[i] = (data[i]-dmin)/(dmax-dmin)*255.0;
}

void ImageWindow3d::saveFile(const char *name, GLint format) 
{
  FILE *fp = fopen(name, "w");

  GLint vp[4];
  
  if (format == GL2PS_PS) // make it square and centered on an 8.5x11 sheet of papter
    {
      vp[0] = 0;
      vp[1] = 90;
      vp[2] = 612;
      vp[3] = 612;
    }
  else // otherwise, just make it 4x4 " (it's scalable anyway!)
    {
      vp[0] = 0;
      vp[1] = 0;
      vp[2] = 288;
      vp[3] = 288;
    }

  GLint buffersize = 1024*1024*128;  // where did this number come from...
  GLint rval; // return value

  GLint options = GL2PS_NONE;
  //options |=  GL2PS_NO_PS3_SHADING;
  //options |=  GL2PS_DRAW_BACKGROUND;
  //options |=  GL2PS_OCCLUSION_CULL;
  //options |=  GL2PS_USE_CURRENT_VIEWPORT;
  
  rval = gl2psBeginPage(name, "spyview3d", 
			vp, 
			format, GL2PS_SIMPLE_SORT, options, 
			GL_RGBA, 0, 
			NULL,
			0, 0, 0, 
			buffersize, fp, 
			name);
  if (rval != GL2PS_SUCCESS)
    fprintf(stderr, "Error with gl2psBeginPage.\n");

  glViewport(vp[0], vp[1], vp[2], vp[3]);
  draw();
  glViewport(0,0,w(),h());
  rval = gl2psEndPage();
  if (rval != GL2PS_SUCCESS)
    fprintf(stderr, "Error with gl2psEndPage.\n");

  fclose(fp);
}

void ImageWindow3d::op_ac() 
{
  inform("doing autocorrelation with data_matrix");
  data_matrix->zero_pad = true;
  data_matrix->op_ac();
  data_matrix->zero_pad = false;
  //update_data_array();
  inform("done with autocorrelation");
}

void ImageWindow3d::op_cc(char *filename) 
{
  inform("starting op_cc");
  inform("doing crosscorrelation.");
  data_matrix->zero_pad = true;
  data_matrix->op_cc(filename);
  data_matrix->zero_pad = false;
  inform("done with crosscorrelation.");
}

void ImageWindow3d::op_fft() 
{
  inform("doing fft with data_matrix");
  data_matrix->zero_pad = false;
  data_matrix->op_fft();
  data_matrix->zero_pad = false;
  inform("done with fft");
}

void ImageWindow3d::op_lbl() 
{
  double laverage;
  int i, j;
  for (i=0; i < height; i++)
    {
      laverage = 0;
      for (j=0; j<width; j++)
	laverage += get_data(i,+j);
      laverage = laverage / (float) width;
      //fprintf(stderr, "laverge %d %6.0f\n", i, laverage);
      for (j=0; j<width; j++)
	{
	  data[i*width+j] = data[i*width+j] - (int) laverage + LMAX/2;
	  if (data[i*width+j] < 0)  data[i*width+j] = 0;
	  if (data[i*width+j] > LMAX)  data[i*width+j] = LMAX;
	}
    }
}

void ImageWindow3d::op_log() 
{
  data_matrix->op_log();
}

int ImageWindow3d::handle(int event) 
{
  int n;
  switch(event) 
    {
    case FL_PUSH:
      n = Fl::event_button();
      lastxp = Fl::event_x();
      lastyp = Fl::event_y();
      button_pushed = n;
      return 1;
      break;
    case FL_DRAG:
      xp = Fl::event_x();
      yp = Fl::event_y();
      if (button_pushed == 1) { //rotation
	psi += (float)(-xp+lastxp);
	theta += (float)(-yp+lastyp);
	redraw();
      }
      if (button_pushed == 2) { //zoom
	scalex *= 1.0+(float)(xp-lastxp)/w();
	scaley *= 1.0+(float)(xp-lastxp)/w();
	scalez *= 1.0+(float)(xp-lastxp)/w();
	scalez *= 1.0+(float)(-yp+lastyp)/h();
	redraw();
      }
      if (button_pushed == 3 ) {//translation
	translatex +=  (float)(xp-lastxp)/100.0;
	translatey += (float)(-yp+lastyp)/100.0;
	redraw();
      }
      lastxp = xp;
      lastyp = yp;
      if (external_update != NULL)
	(*external_update)();
      return 1;
      break;
    case FL_RELEASE:
      button_pushed = 0;
      return 1;
      break;
    }
  return 0;
}

void ImageWindow3d::fitPlane() //calculates plane_a, plane_b, and plane_c
{
  // Formula for the plane: Z = a*X + b*Y + c
  double a,b,c;
  // calculate the moments
  int N = width*height;
  double Zavg = 0;
  double Xavg = 0;
  double Yavg = 0;
  double sXZ = 0;
  double sYZ = 0;
  double sXX = 0;
  double sYY = 0;

  for (int x=0; x < width; x++)
    {
      for (int y=0; y < height; y++)
	{
	  Zavg += data[y*width + x];
	  Xavg += (x-width/2);
	  Yavg += (y-height/2);
	  sXZ += data[y*width + x] * (x-width/2);
	  sYZ += data[y*width + x] * (y-height/2);
	  sXX += (x-width/2)*(x-width/2);
	  sYY += (y-height/2)*(y-height/2);
	}
    }

  Xavg /= N;
  Yavg /= N;
  Zavg /= N;

  a = (sXZ - N*Xavg*Zavg)/(sXX - N*Xavg*Xavg);
  b = (sYZ - N*Yavg*Zavg)/(sYY - N*Yavg*Yavg);
  c = Zavg - a*Xavg - b*Yavg - 128;
  
  //fprintf(stderr, " a %10.2f\n b %10.2f\n c %10.2f\n", a, b, c);

  // What's up with this? Why does this work?
  plane_a = b;
  plane_b = a;
  plane_c = 0; //better for the 3d viewer.
};


inline void inform(char *message) 
{
  //fflush(stderr);
  //fprintf(stderr, "%s\n", message);
  //fflush(stderr);
}
