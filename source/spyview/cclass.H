/****
  cclass.H/C
 
  classes to make it easy to manipulate colors in different colorspaces.
  (c) 2009 Oliver Dial
***/
#ifndef __cclass_h__
#define __cclass_h__
#include <math.h>
#include <vector>
#include <stdio.h>
#include "myboost.h"
#include <algorithm>
// The base color space for spypal is CIELAB color space.
// We convert to RGB, CMYK, etc. from that
// See http://en.wikipedia.org/wiki/CIELAB
// We store values internally in CIEXYZ color space, as it's
// easy to convert to/from CIEXYZ to RGB, etc.

class ccolor
{
public:
  double X,Y,Z;
  ccolor(double Xp=0, double Yp=0, double Zp=0) : X(Xp), Y(Yp), Z(Zp) {};
  void dump(const char *name) { printf("%s %g,%g,%g\n",name,X,Y,Z); };
  bool operator==(const ccolor &c) { return X == c.X && Y == c.Y && Z == c.Z; };
  bool operator!=(const ccolor &c) { return X != c.X || Y != c.Y || Z != c.Z; };
  
  template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
  {
    ar & X;
    ar & Y;
    ar & Z;
  };
};

// This class encapsulates a color space.
class ccspace
{
public:
  // ccspace(){}
  static const double eps;
protected:
  inline bool bound(double &x) const // Return true if number was clipped
  {
    bool clipped = false;
    if(x < 0.0)
      {
	if(x < -eps)
	  clipped=true;
	x = 0;
      }
    else if(x > 1.0)
      {
	if(x > 1.0+eps)
	  clipped = true;
	x = 1;
      }
    return clipped;
  }

public: 
  virtual bool get(const ccolor &c,double &c1, double &c2, double &c3) const = 0; // Get the color. Return true if color was clipped
  virtual void set(ccolor &c, double c1, double c2, double c3) const = 0;

  bool clip(ccolor &t) const // Clip a color to be in gamut for this colorspace
  {
    double a,b,c;
    bool clipped = get(t,a,b,c);
    if(clipped)
      set(t,a,b,c);
    return clipped;
  };

  virtual void c1r(double &min, double &max) const = 0; // Get some estimate for the meaningful size of this color.
  virtual void c2r(double &min, double &max) const = 0; // This should be the dimensions of the smallest cube that
  virtual void c3r(double &min, double &max) const = 0; // encloses the sRGB color space in this color space.

  // Provide the c2,c3 coordinates of the visible gamut's bounding line given c1. (If available).
  virtual void visible_gamut(double c1, std::vector<double> &c2, std::vector<double> &c3) const { c2.clear(); c3.clear(); } 

  virtual const char *axes() const = 0;  // Return axes as a 3 letter string, ie. "RGB"
  virtual double size() const = 0; // Return some useful estimate for the meaningful size of the colorspace.  This should
// be roughly 110% of the length of the longest dimension of the visible gamut in this colorspace, or the longest
// dimension of the colorspace if it does not span the visible gamut.  Ie, in CIELAB this will be roughly
// sqrt(500^2+2*200^2)*1.1 or in sRGB will be ~sqrt(3.0)*1.1
}; 

extern ccolor cc_WhitePoint;

// This is the old CIE color def'n.
// Note the weird parameter order to  put luminosity first.
class cc_CIEXYZ_class : public ccspace
{
public:
  cc_CIEXYZ_class(){}
  bool get(const ccolor &c, double &y, double &x, double &z) const { x=c.X; y=c.Y; z=c.Z; return false; };
  void set(ccolor &c, double y, double x, double z) const{ c.X=x; c.Y=y; c.Z=z; };
  
  void c1r(double &min, double &max) const { min=0; max=1; };
  void c2r(double &min, double &max) const { min=0; max=1; };
  void c3r(double &min, double &max) const { min=0; max=1; };
  const char *axes() const { return "YXZ"; };
  double size() const { return sqrt(3.0)*1.1; };

// We don't currently provide the visible gamut in CIEXYZ.				
};
extern const cc_CIEXYZ_class cc_CIEXYZ;

// Classic 1931 CIE color space.  
// Note the weird parameter order to  put luminosity first.
class cc_CIExyY_class : public ccspace
{
public:
  cc_CIExyY_class(){}
  bool get(const ccolor &c, double &Y, double &x, double &y) const  // NOTE Yxy order!
    { double s = c.X + c.Y + c.Z; x = c.X / s; y = c.Y / s; Y = c.Y; return false;};
  void set(ccolor &c, double Y, double x, double y) const
    { c.Y = Y; c.X = Y*x/y; c.Z = Y*(1-x-y)/y; };
  
  void c1r(double &min, double &max) const { min=0; max=1; };
  void c2r(double &min, double &max) const { min=0; max=0.8; };
  void c3r(double &min, double &max) const { min=0; max=0.8; };
  const char *axes() const { return "Yxy"; };
  double size() const { return sqrt(3.0)*1.1; };
};
extern const cc_CIExyY_class cc_CIExyY;
// We don't currently provide the visible gamut in CIEXYZ.				

// CIELuv is perceptually uniform
class cc_CIELuv_class : public ccspace
{
protected:
  static constexpr double un = 0.2009;
  static constexpr double vn = 0.4610;
public:
  cc_CIELuv_class(){}
  bool get(const ccolor &c, double &L, double &u, double &v) const
  {
    static const double m1 = pow(6.0/29.0,3.0);
    if(c.Y/cc_WhitePoint.Y > m1)
      L = 116.0 * pow(c.Y/cc_WhitePoint.Y,1.0/3.0)-16.0;
    else
      L = pow(29.0/3.0,3.0) * (c.Y / cc_WhitePoint.Y);
    double den =(c.X + 15.0*c.Y + 3.0 * c.Z);
    if(fabs(den) < 1e-6)
      den = 1e-6;
    double up = 4.0*c.X/den;
    double vp = 9.0*c.Y/den;
    u = 13.0*L*(up - un);
    v = 13.0*L*(vp - vn);
    return false;
  }
  void set(ccolor &c, double L, double u, double v) const
  {
    if(L <= 0)
      L += 1e-6;
    double up = u/(13.0*L) + un;
    double vp = v/(13.0*L) + vn;
    if(L <= 8.0)
      c.Y = cc_WhitePoint.Y * L * pow(3.0/29.0,3);
    else
      c.Y = cc_WhitePoint.Y * pow((L+16.0)/116.0,3);
    c.X = c.Y*(9.0*up/(4.0*vp));
    c.Z = c.Y*((12.0-3.0*up-20.0*vp)/(4.0*vp));
  }
  void c1r(double &min, double &max) const { min=0; max=100.0; };     // L
  void c2r(double &min, double &max) const { min=-100.0; max=100.0; }; // a 
  void c3r(double &min, double &max) const { min=-100.0; max=100.0; };  // b

  const char *axes() const { return "Luv"; };
  double size() const { return sqrt(100.0*100.0+200.0*200.0*2.0)*1.1; };
  virtual void visible_gamut(double c1, std::vector<double> &c2, std::vector<double> &c3) const;
};
extern const cc_CIELuv_class cc_CIELuv;

// CIELAB is perceptually uniform ; distance is roughly equal to how
// different the colors are
class cc_CIELAB_class : public ccspace
{
private:
  // Crap for LAB-XYZ conversions.  Lotsa magic numbers.
  static constexpr double delta=6.0/29.0;
  inline double labF(double x) const // From CIELAB def'n
  {
    if(x > delta*delta*delta)
      return pow(x,1.0/3.0);
    else
      return 1.0/(3.0*delta*delta)*x + 2.0*delta/3.0; 
  };
  
  inline double labFi(double x) const // Inverse...
  {
    if(x > delta)
      return x*x*x;
    else
      return 3*delta*delta*(x-16.0/116.0);
  }
public:
  cc_CIELAB_class(){}
  bool get(const ccolor &c, double &L, double &a, double &b) const
  {
    L=116.0*labF(c.Y/cc_WhitePoint.Y)-16;
    a=500.0*(labF(c.X/cc_WhitePoint.X)-labF(c.Y/cc_WhitePoint.Y));
    b=200.0*(labF(c.Y/cc_WhitePoint.Y)-labF(c.Z/cc_WhitePoint.Z));
    return false;
  }; 
  void set(ccolor &c, double L, double a, double b) const
  {
    double fy =(L+16.0)/116.0;
    double fx = fy + a/500.0;
    double fz = fy - b/200.0;
    c.Y = cc_WhitePoint.Y * labFi(fy);
    c.X = cc_WhitePoint.X * labFi(fx);
    c.Z = cc_WhitePoint.Z * labFi(fz);
  };

  void c1r(double &min, double &max) const { min=0; max=100.0; };     // L
  void c2r(double &min, double &max) const { min=-107.0; max=94.0; }; // a 
  void c3r(double &min, double &max) const { min=-98.0; max=86.0; };  // b

  virtual void visible_gamut(double c1, std::vector<double> &c2, std::vector<double> &c3) const;
  const char *axes() const { return "Lab"; };
  double size() const { return sqrt(500*500.0+200*200*2.0)*1.1; };
};

extern const cc_CIELAB_class cc_CIELAB;

// sRGB is the w3c standard for how RGB data is stored.  It
// should look pretty good on monitors, etc.  It does not capture
// the full gamut of colors possible on a good printer.
// NOTE: This uses a d65 white point for the conversion!
class cc_sRGB_class : public ccspace
{
private:
  static constexpr double a = 0.055;
  static constexpr double c=0.0031308;

  // Crap for sRGB-XYZ conversions. Lotsa magic.				
  double srgbF(double x) const
  {
    if(x < c)
	return 12.92*x;
    else
      return (1+a)*pow(x,1.0/2.4)-a;
  };
  double srgbFi(double x) const
  {
    const double ci = 12.92*c;
    if(x < ci)
      return x/12.92;
    else
      return pow((x+a)/(1.0+a),2.4);
  };
public: 
  cc_sRGB_class(){}
  bool get(const ccolor &c, double &r, double &g, double &b) const// return true if clipped
  {
    double Rl = 3.2410*c.X-1.5374*c.Y-0.4986*c.Z;
    double Gl = -0.9692*c.X+1.8760*c.Y+0.0416*c.Z;
    double Bl = 0.0556*c.X-0.2040*c.Y+1.0570*c.Z;   

    r = srgbF(Rl);
    g = srgbF(Gl);
    b = srgbF(Bl);

    bool clipped = bound(r); // Evade short-circuit evaluation.
    clipped = bound(g) || clipped;
    clipped = bound(b) || clipped;

    return clipped;
  };
  void set(ccolor &c, double r, double g, double b) const
  {
    double Rl = srgbFi(r);
    double Gl = srgbFi(g);
    double Bl = srgbFi(b);
    c.X = 0.412381*Rl+0.357573*Gl+0.180452*Bl;
    c.Y = 0.212620*Rl+0.715138*Gl+0.072150*Bl;
    c.Z = 0.019343*Rl+0.119212*Gl+0.950507*Bl;
  };

  void c1r(double &min, double &max) const { min=0; max=1.0; };
  void c2r(double &min, double &max) const { min=0; max=1.0; };
  void c3r(double &min, double &max) const { min=0; max=1.0; };
  const char *axes() const { return "RGB"; };
  double size() const { return sqrt(3.0)*1.1; };
};
extern const cc_sRGB_class cc_sRGB;

// HSV is a pretty standard color space. 
// http://en.wikipedia.org/wiki/HSL_and_HSV#General_approach
// As a bit of a hack, we convert to/from HSV through RGB.
// Here we treat H,S,and V as varying from 0...1, (not 0-360 for hue)
class cc_HSV_class : public ccspace
{
public:
  cc_HSV_class(){}
  bool get(const ccolor &c, double &h, double &s, double &v) const
  {
    double r,g,b;
    bool clip=cc_sRGB.get(c,r,g,b);
    double x = std::min(std::min(r,g),b);
    v = std::max(std::max(r,g),b);
    if(fabs(x-v) < 1e-4)
      {
	h=0;
	s=0.0; 
	v=v;
	return clip;
      }
    double f = (r == x) ? g-b : ((g == x) ? b-r : r-g);
    double i = (r == x) ? 3 : ((g == x) ? 5 : 1);
    h = (i-f/(v-x))/6.0;
    s = (v-x)/v;
    return clip;
  };

  void set(ccolor &c, double h, double s, double v) const
  {
    h = h - floor(h);
    h *= 6.0;
    int i = floor(h);
    double f = h-i;
    if(!(i&1)) f = 1-f;
    double m=v*(1-s);
    double n=v*(1-s*f);
    double r,g,b;
    switch(i)
      {
      default:
      case 0:
	r=v; g=n; b=m;	break;
      case 1:
	r=n; g=v; b=m;	break;
      case 2:
	r=m; g=v; b=n;	break;
      case 3:
	r=m; g=n; b=v;	break;
      case 4:
	r=n; g=m; b=v;	break;
      case 5:
	r=v; g=m; b=n; break;
      }
    cc_sRGB.set(c,r,g,b);
  };

  void c1r(double &min, double &max) const { min=0; max=1.0; };
  void c2r(double &min, double &max) const { min=0; max=1.0; };
  void c3r(double &min, double &max) const { min=0; max=1.0; };

  const char *axes() const { return "HSV"; };
  double size() const { return 1.1*sqrt(3.0); };
};

extern const cc_HSV_class cc_HSV;

// Castrated CIECAM02 using neutral gray assumptions.
class cc_CIECAM02_LMS_class : public ccspace 
{
public:
  cc_CIECAM02_LMS_class(){}
  static constexpr double F = 1.0;
  static constexpr double c_const = 0.525;
  static constexpr double Nc = 1.0;

  static constexpr double Lw = 300; // Luminance of typical display.
  static constexpr double La = 60.0; // "Gray world"; Lw/5.0

  // We skip color adaption/spectral sharpening.  Not relevant here.
  // Hunt-Pointer-Estevez transform, equal energy illuminants.  
  static void toLMS(double X, double Y, double Z, double &L, double &M, double &S)
  {
    L = 0.38971*X+0.68898*Y-0.07868*Z;
    M =-0.22981*X+1.18340*Y+0.04641*Z;
    S = Z;
  }
  static void fromLMS(double &X, double &Y, double &Z, double L, double M, double S)
  {
    X=1.91020*L-1.11212*M+0.201908*S;
    Y=0.37096*L+0.62905*M-0.0000081*S;
    Z = S;
  }
  static inline double compress(double p, double Fl) 
  {
    double t = pow(Fl*p/100.0,0.24);
    return 400.0*t/(27.13+t)+0.1;
  }
  static inline double decompress(double p, double Fl)
  {
    double alpha=(p-0.1)/400.0;
    double t = 27.13*alpha/(1.0-alpha);
    return (100.0/Fl)*pow(t,1.0/0.24);
  }
  bool get(const ccolor &c, double &L, double &M, double &S) const
  {
    static const double k = 1.0/(5.0*La+1.0);
    static const double k4 = pow(k,4.0);
    static const double Fl = 0.2*k4*(5.0*La)+0.1*(1.0-k4)*(1.0-k4)*pow(5.0*La,1.0/3.0);
    toLMS(c.X,c.Y,c.Z,L,M,S);

    L = compress(L,Fl);
    M = compress(M,Fl);
    S = compress(S,Fl);
    return false;
  }
  void set(ccolor &c, double L, double M, double S) const
  {
    static const double k = 1.0/(5.0*La+1.0);
    static const double k4 = pow(k,4.0);
    static const double Fl = 0.2*k4*(5.0*La)+0.1*(1.0-k4)*(1.0-k4)*pow(5.0*La,1.0/3.0);
    L = decompress(L,Fl);
    M = decompress(M,Fl);
    S = decompress(S,Fl);

    fromLMS(c.X,c.Y,c.Z,L,M,S);
  }
  const char *axes() const { return "LMS"; };
  void c1r(double &min, double &max) const { min=0; max=100.0; };     // L
  void c2r(double &min, double &max) const { min=-107.0; max=94.0; }; // a 
  void c3r(double &min, double &max) const { min=-98.0; max=86.0; };  // b
  double size() const { return sqrt(500*500.0+200*200*2.0)*1.1; };
};

class cc_CIECAM02_Jab_class : public ccspace 
{
public:
  cc_CIECAM02_Jab_class(){}
  static constexpr double F = 1.0;
  static constexpr double c_const = 0.525;
  static constexpr double Nc = 1.0;

  static constexpr double Lw = 300.0; // Luminance of typical display.
  static constexpr double La = 60.0; // "Gray world"; Lw/5.0

  // We skip color adaption/spectral sharpening.  Not relevant here.
  // Hunt-Pointer-Estevez transform, equal energy illuminants.  
  static void toLMS(double X, double Y, double Z, double &L, double &M, double &S)
  {
    L = 0.38971*X+0.68898*Y-0.07868*Z;
    M =-0.22981*X+1.18340*Y+0.04641*Z;
    S = Z;
  }
  static void fromLMS(double &X, double &Y, double &Z, double L, double M, double S)
  {
    X=1.91020*L-1.11212*M+0.201908*S;
    Y=0.37096*L+0.62905*M-0.0000081*S;
    Z = S;
  }
  static inline double compress(double p, double Fl) 
  {
    if(p < 0)
      p = 0;
    double t = pow(Fl*p/100.0,0.42);
    return 400.0*t/(27.13+t)+0.1;
  }
  static inline double decompress(double p, double Fl)
  {
    double alpha=(p-0.1)/400.0;
    double t = 27.13*alpha/(1.0-alpha);
    if(t < 0)
      t = 0;
    return (100.0/Fl)*pow(t,1.0/0.42);
  }

  bool get(const ccolor &c, double &J, double &a, double &b) const
  {
    double L,M,S;
    static const double k = 1.0/(5.0*La+1.0);
    static const double k4 = pow(k,4.0);
    static const double Fl = 0.2*k4*(5.0*La)+0.1*(1.0-k4)*(1.0-k4)*pow(5.0*La,1.0/3.0);
    toLMS(c.X,c.Y,c.Z,L,M,S);

    L = compress(L,Fl);
    M = compress(M,Fl);
    S = compress(S,Fl);

    static const double n = 1.0/5.0;
    static const double Nbb=0.725*pow(n,-0.2);
    static const double Aw=(2.+1.+1./20.0-0.305)*Nbb;
    // Convert to a,b,lightness
    a = L - 12.0/11.0*M+1.0/11.0*S;
    b = (L+M-2.0*S)/9.0;
    double A = (2*L+M+S/20.0-0.305)*Nbb;

    J =  100.0*pow(A/Aw,c_const*(1.48+sqrt(n)));
    if(J < 0)
      J = 0;
    return false;
  }
  void set(ccolor &c, double J, double a, double b) const
  {
    static const double n = 1.0/5.0;
    static const double Nbb=0.725*pow(n,-0.2);
    static const double Aw=(2.+1.+1./20.0-0.305)*Nbb;    
    if(J < 0)
      J = 0;
    double Ap = pow(J/100.0,1.0/(c_const*(1.48+sqrt(n))))*Aw/Nbb + 0.305;

    b *= 9.0;
    double L = 0.3278689*Ap+0.321454027*a+0.02280826*b;
    double M = 0.3278689*Ap-0.635067712*a-0.02066999*b;
    double S = 0.3278689*Ap-0.156806842*a-0.49893086*b;
    static const double k = 1.0/(5.0*La+1.0);
    static const double k4 = pow(k,4.0);
    static const double Fl = 0.2*k4*(5.0*La)+0.1*(1.0-k4)*(1.0-k4)*pow(5.0*La,1.0/3.0);
    L = decompress(L,Fl);
    M = decompress(M,Fl);
    S = decompress(S,Fl);

    fromLMS(c.X,c.Y,c.Z,L,M,S);
  }
  const char *axes() const { return "LMS"; };
  void c1r(double &min, double &max) const { min=0; max=100.0; };     // L
  void c2r(double &min, double &max) const { min=-107.0; max=94.0; }; // a 
  void c3r(double &min, double &max) const { min=-98.0; max=86.0; };  // b
  double size() const { return sqrt(500*500.0+200*200*2.0)*1.1; };
};

extern const cc_CIECAM02_Jab_class cc_CIECAM02;
#endif
