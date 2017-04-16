#ifndef __bisector_h__
#define __bisector_h__
#include <assert.h>
#include <math.h>

#include <cmath>
using std::isnan;

class bisector
{
public:
  static const double factor=0.5;
  double x1, x2, fx1, fx2;
  double acc;
  int iter;
  bisector(double x1p, double fx1p, double x2p, double fx2p, double acct=1e-6) : x1(x1p), x2(x2p), fx1(fx1p), fx2(fx2p), acc(acct*fabs(x1p-x2p))
  {
    iter =0;
  };

  bool x(double &xp) // Return next x coordinate to evaluate.  Return true if we've converged.
  {
    if(fx1*fx2 > 0)
      {
	if(fabs(fx1) < fabs(fx2))
	  {
	    xp = x1;
	    return true;
	  }
	else
	  {
	    xp = x2;
	    return true;
	  }
      }
    if(fabs(x2-x1) < acc)
      {
	xp = (x1+x2)/2.0;
	return true;
      }
    if(iter++ > 100)
      {
	fprintf(stderr,"x1=%g, x2=%g, acc=%g; bailing\n",
		x1,x2,acc);
	xp=(x1+x2)/2.0;
	return true;
      }
    xp = x1+factor*(x2-x1);
    return false;
  };

  inline void o(double x, double fx) // Add an observation.
  {
    if(fx*fx1 <= 0.0)
      {
	x2 = x;
	fx2 = fx;
      }
    else
      {
	assert(fx*fx2 <= 0.0);
	x1 = x;
	fx1 = fx;
      }
  };
};

/* Newton step until we bracket, then bisect */
class magic_bisector
{
public:
  static const double factor=0.5;
  bool bisection;
  double nx, nfx;
  double x1, fx1, x2, fx2, mest;
  double acc;
  int iter;
  magic_bisector(double x_start, double m_est_p, double acc_p=1e-6) : nx(x_start), mest(m_est_p), acc(acc_p) 
  { 
    x1 = NAN;
    x2 = NAN;
    nfx = 10*acc_p; 
    bisection = false;
    iter = 0;
  };
  bool x(double &xp)
  {
    xp = nx;    
    return (fabs(nfx) < acc) || (iter++ > 32);
  }

  void o(double xp, double fxp)
  {
    nfx = fxp;
    if(bisection)
      {
	if(fxp < 0)
	  {
	    x1 = xp;
	    fx1 = fxp;
	  }
	else
	  {
	    x2 = xp;
	    fx2 = fxp;
	  }
      }
    else
      {
	if(fxp < 0 && (isnan(x1) || (fxp > fx1)))
	  {
	    x1 = xp;
	    fx1 = fxp;
	  }
	else if(fxp > 0 && (isnan(x2) || (fxp < fx2)))
	  {
	    x2 = xp;
	    fx2 = fxp;
	  }
      }
    bisection = !(isnan(x1) || isnan(x2));
    if(bisection)
      {
	//	printf("Bisection step: [%g,%g] {%g,%g}\n", x1,x2,fx1,fx2);
	nx = (x1+x2)/2.0;
      }
    else
      {
	nx -= nfx/mest;
	//	printf("Newton step to %g (%g/%g)\n",nx,nfx,mest);
      }
  }
};
#endif
