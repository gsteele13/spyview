#include "spypal_import.H"
#include <stdio.h>
#include <math.h>
using namespace std;

// isnan in math.h does not work under OSX (?) so you have to use
// std:isnan. However, std:isnan does not cross compile properly for
// win32. However, if we include math.h and "using namespace std",
// both seem to compile...

double sqr(double x) { return x*x; };
static int worst_point;
static double worst_error;
double spypal_worst_error() { return worst_error; };
static double fidelity(unsigned char *c1, unsigned char *c2, unsigned l)
{
  double e = 0;
  worst_error = 0; 
  worst_point = -1;
  for(unsigned i = 0; i < l; i++)
    {
      double se = sqr(c2[3*i]-c1[3*i])+sqr(c2[3*i+1]-c1[3*i+1])+sqr(c2[3*i+2]-c1[3*i+2]);
      if(se > worst_error)
	{
	  worst_point = i;
	  worst_error = se;
	}
      e += se;
    }
  return e;
}

static void color_waypoints(std::vector<SpypalWaypoint> &wps, unsigned char *c1, unsigned l)
{
  for(unsigned i = 0 ; i < wps.size(); i++)
    {
      double r,g,b;
      r = c1[3*wps[i].ind+0]/255.0;
      g = c1[3*wps[i].ind+1]/255.0;
      b = c1[3*wps[i].ind+2]/255.0;
      //      printf("%d %g %g %g %d %d %d\n",wps[i].ind,r,g,b,c1[3*wps[i].ind+0],c1[3*wps[i].ind+1],c1[3*wps[i].ind+2]);
      cc_sRGB.set(wps[i].c,r,g,b);
    }
}
static double try_colormap(std::vector<SpypalWaypoint> &wps, const ccspace *cs, unsigned  char *c1, unsigned l)
{
  sort(wps.begin(),wps.end());

  generate_go(wps,OPTS_STRAIGHT,true,cs,cs,l);
  if(cmap.size() != l)
    return NAN;
  dump_colormap_memory();
  return fidelity(c1,spypal_colormap,l);
}

// Try to adjust the waypoints by starting from three waypoints, then building
// up, adding the additional waypoint at the point where the error is largest
// each time.
double spypal_bisect_anneal(SpypalWaypoints_t &wps_out, const ccspace *cs, unsigned char *c1, unsigned l)
{
  size_t size = wps_out.size();
  assert(size >= 2);

  if(size == 2)
    return spypal_anneal(wps_out,cs,c1,l);

  double err;
  SpypalWaypoints_t wps(3);

  // Initialize the guess.
  for(unsigned i = 0; i < wps.size(); i++)
    {
      wps[i].loc = ((double)i)/(wps.size()-1);
      wps[i].ind = wps[i].loc * (l-1);
      wps[i].locked = true;
    }
  color_waypoints(wps,c1,l);
  err = spypal_anneal(wps,cs,c1,l,false);

  // Add a waypoint at the worst point and anneal until we reach the right size
  while(wps.size() < size)
    {
      SpypalWaypoint w;
      if(worst_point >= 0)
	w.ind = worst_point;
      else
	w.ind = l/2;
      w.loc = ((double)w.ind) / (l-1);
      w.locked = true;
      wps.push_back(w);
      color_waypoints(wps,c1,l);
      err = spypal_anneal(wps,cs,c1,l,false);
    }
  wps_out = wps;
  return try_colormap(wps,cs,c1,l);
}

// Try to adjust the waypoints in waypoints to maximize the fidelity.
// Return the fidelity.
double spypal_anneal(SpypalWaypoints_t &wps, const ccspace *cs, unsigned char *c1, unsigned l, bool init)
{
  double best;
  SpypalWaypoints_t wps_best;

  if(init)
    {
      // Initialize the guess.
      for(unsigned i = 0; i < wps.size(); i++)
	{
	  wps[i].loc = ((double)i)/(wps.size()-1);
	  wps[i].ind = wps[i].loc * (l-1);
	  wps[i].locked = true;
	}
      color_waypoints(wps,c1,l);
    }

  // Get an initial fidelity
  best = try_colormap(wps,cs,c1,l);
  if(isnan(best))
    return best;
  wps_best = wps;

  // Anneal; try sliding each waypoint left then right.
  bool improved;
  int iter = 1000;
  int stepsize = l/(2.0*wps.size());
  if(stepsize < 1)
    stepsize = 1;
  do
    {   
      improved = false;
      for(unsigned i = 1; i < wps.size()-1; i++)
	{
	  if(wps[i].ind > stepsize)
	    {
	      SpypalWaypoints_t wpst = wps;
	      wpst[i].ind -= stepsize;
	      wpst[i].loc = wpst[i].ind / (l - 1.0);
	      color_waypoints(wpst,c1,l);
	      double f = try_colormap(wpst,cs,c1,l);
	      if(f < best)
		{
		  best = f;
		  wps_best = wpst;
		  improved = true;
		  wps = wpst;
		  break;
		}
	    }
	  if(((int)wps[i].ind) < (l-stepsize))
	    {
	      SpypalWaypoints_t wpst = wps;
	      wpst[i].ind += stepsize;
	      wpst[i].loc = wpst[i].ind / (l - 1.0);
	      color_waypoints(wpst,c1,l);
	      double f = try_colormap(wpst,cs,c1,l);
	      if(f < best)
		{
		  best = f;
		  wps_best = wpst;
		  improved = true;
		  wps = wpst;
		  break;
		}
	    }
	}      
      if(best == 0.0)
	break;
      if(!improved)
	{
	  if(stepsize == 1)
	    break;
	  else
	    stepsize /= 2.0;
	}
      //      printf("%5d %3d %g\n",iter,stepsize,best);
    }
  while(iter--);
  wps = wps_best;
  // One extra calc cycle to get worst_error right.
  return best; 
}
