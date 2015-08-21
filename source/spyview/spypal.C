#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <string.h>
#include <FL/Fl_Image.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/fl_ask.H>
#include <errno.h>
#include "spypal_interface.H"
#include "bisector.H"

cmap_t cmap;
std::vector<bool> spypal_clipped;

static const char *ppmname = "cmap.ppm";
static const char *textname = "cmap.txt";

std::string spypal_sharedir; // Directory where documentation lives.

unsigned char *spypal_colormap = NULL;
size_t spypal_colormap_size = 0;

static bool warned = false;

/***********************************************************************/
/* Output functions; dump colormap in various handy ways.              */
/***********************************************************************/

// Plop the colormap in a memory buffer shared with spyview.
void dump_colormap_memory()
{
  if(spypal_colormap_size != cmap.size())
    {
      spypal_colormap_size = cmap.size();
      if(spypal_colormap != NULL)
	free(spypal_colormap);
      spypal_colormap = reinterpret_cast<unsigned char *>(malloc(spypal_colormap_size*sizeof(char)*3));
      assert(spypal_colormap);
      spypal_clipped.resize(cmap.size());
    }
  for(unsigned i = 0; i < cmap.size(); i++)
    {
      double R,G,B;
      spypal_clipped[i] = cc_sRGB.get(cmap[i],R,G,B);
      spypal_colormap[i*3] = (int)round(255*R);
      spypal_colormap[i*3+1] = (int)round(255*G);
      spypal_colormap[i*3+2] = (int)round(255*B);
      //      printf("%g %g %g %x %x %x\n",R,G,B,spypal_colormap[i*3],spypal_colormap[i*3+1],spypal_colormap[i*3+2]);      
    }
  //  printf("\n\n\n");
}

// Precondition; dump_colomap_memory has already been called.
// Save the colormap as a ppm file.
void dump_colormap_ppm(const char *fname)
{
  FILE *f=fopen(fname,"w+");
  if(f == NULL)
    {
      if(!warned)
	fl_alert("Unable to open file %s for writing: %s\n",fname,strerror(errno));
      warned = true;
      return;
    }
  fprintf(f,"P3\n");
  fprintf(f,"1 %d\n",cmap.size());
  fprintf(f,"255\n");
  for(unsigned i = 0; i < cmap.size(); i++)
    {
      fprintf(f,"%d %d %d\n", spypal_colormap[i*3], spypal_colormap[i*3+1], spypal_colormap[i*3+2]); 
    }
  fclose(f);
}

// Print out the color map in a .txt file.
void dump_colormap_text(const char *fname)
{
  FILE *f = fopen(fname,"w+");
  if(f == NULL)
    {
      if(!warned)
	fl_alert("Unable to open file %s for writing: %s\n",fname,strerror(errno));
      warned = true;
      return;
    }
  for(unsigned i= 0; i < cmap.size(); i++)
    {
      double R,G,B;
      int r,g,b;	  
      bool clip = cc_sRGB.get(cmap[i],R,G,B);
      r = round(255*R);
      g = round(255*G);
      b = round(255*B);
      fprintf(f,"%-7.5f %-7.5f %-7.5f #%02x%02x%02x %d %d %d %s\n",R,G,B,r,g,b,r,g,b, clip ? "(clipped)" : "");
    }
  fclose(f);
}

void (*colormap_callback)() = NULL; // Called whenever colormap changes.

void update_text()
{
  static Fl_Text_Buffer b;
  b.text("");
  b.appendfile(textname);
  cmap_text_display->buffer(b);
}

void show_as_text()
{
  update_text();
  spypal_text_window->show();
}

using namespace std;


// Path generation
// This is the heart of spypal -- where we actually generate smooth gradients
/*************************************************************************/
/* Begin spypal core logic                                               */
/*************************************************************************/
// Nasty special case for HSV; take the shortest direction in H space,
// including the fact that it's circular.
inline void closest_HSV(double &h1, double &h2)
{
  if(h1 - h2 > 0.5)
    h1 -= 1.0;
  if(h1 - h2 < -0.5)
    h1 += 1.0;
}

class dmetric
{
public:
  virtual double distance(const ccspace &s, ccolor a, ccolor b) const = 0;
};

class dm_euclid_class : public dmetric
{
public:
  double distance(const ccspace &s, ccolor a, ccolor b) const
  {
    double a1,a2,a3,b1,b2,b3;
    s.get(a,a1,a2,a3);
    s.get(b,b1,b2,b3);
    if(&s == &cc_HSV) closest_HSV(a1,b1);
    a1 -= b1; a2 -= b2; a3 -= b3;
    return sqrt(a1*a1+a2*a2+a3*a3);
  };
} dm_euclid;

// Return how long the last step will be.
// Go from src to dst, not including the last point.
double smooth_subpath(const cmap_t &src, cmap_t &dst, const ccspace &cs, dmetric *metric, double step, unsigned strt, unsigned end, double &plen)
{
  static const double eps = 1e-4;
  plen = 0;
  assert(strt < src.size());
  assert(end <= src.size());

  dst.push_back(src[strt]);
  if(dst.size() > 1) // Make sure we get the length right on "1-step" paths.
    plen += metric->distance(cs,dst[dst.size()-2],dst[dst.size()-1]);
  for(unsigned i = strt+1; i < end; i++)
    {
      double d1 = metric->distance(cs,dst.back(),src[i]);
      if(d1 > step)
	{
	  double d2;
	  int j;
	  for(j = -1; ((int)i) + j >= 0; j--)
	    {
	      d2 = metric->distance(cs,dst.back(),src[i+j]);
	      if(fabs(d1-d2) > eps)
		break;
	    }
	  if(fabs(d1-d2) < eps)
	    {
	      dst.push_back(src[i]);
	      plen += d1;
	      continue;
	    }
	  double a1,a2,a3;
	  double b1,b2,b3;
	  cs.get(src[i],a1,a2,a3);
	  cs.get(src[i+j],b1,b2,b3);
	  double lambda = (d1-step)/(d1-d2);
	  ccolor tmp;
	  cs.set(tmp,  b1*lambda+a1*(1.0-lambda),
		       b2*lambda+a2*(1.0-lambda),
		       b3*lambda+a3*(1.0-lambda));
	  plen += metric->distance(cs,dst.back(),tmp);
	  dst.push_back(tmp);
	}
    }
  return metric->distance(cs,dst.back(),src[end-1]);
}

double generate_spiral_path(int dir, ccolor *c1, ccolor *c2, cmap_t &cmap, int steps, const ccspace *cc)
{
  double dk = 1.0/(steps+0.5);
  ccolor tmp;
  double len = 0;

  double a1,a2,a3;
  double b1,b2,b3;
  cc->get(*c1,a1,a2,a3);
  cc->get(*c2,b1,b2,b3);
  if(cc == &cc_HSV) closest_HSV(a1,b1);
  double at = atan2(a2,a3);
  double bt = atan2(b2,b3);
  double ar = sqrt(a2*a2+a3*a3);
  double br = sqrt(b2*b2+b3*b3);
  double dt = bt-at;
  double dr = br-ar;
  double dl = b1-a1;
  while(dir < 0 && dt > 0)
    dt -= 2.0*M_PI;
  while(dir > 0 && dt < 0)
    dt += 2.0*M_PI;
  for(double k = 0; k < 1.0-(1.5*dk); k += dk) 
    {
      double l = a1 + dl*k;
      double r = ar + dr*k;
      double t = at + dt*k;
      cc->set(tmp,l,r*sin(t),r*cos(t));
      cmap.push_back(tmp);
      len += dk*sqrt(dl*dl + dr*dr + dt*r);
    }
  return len;
}

double generate_straight_path(ccolor *c1, ccolor *c2, cmap_t &cmap, int step, const ccspace *cc)
{
  double a1,a2,a3;
  double b1,b2,b3;
  cc->get(*c1,a1,a2,a3);
  cc->get(*c2,b1,b2,b3);
  if(cc == &cc_HSV) closest_HSV(a1,b1);
  double len = sqrt((a1-b1)*(a1-b1)+(a2-b2)*(a2-b2)+(a3-b3)*(a3-b3));
  double d1 =(b1-a1)/(step+1.0);
  double d2 =(b2-a2)/(step+1.0);
  double d3 =(b3-a3)/(step+1.0);  
  ccolor tmp;
  for(int i = 0; i < step; i++)
    {
      cc->set(tmp,a1,a2,a3);
      cmap.push_back(tmp);
      a1 += d1;
      a2 += d2;
      a3 += d3;
    } 
  return len;
}

double generate_path(int direction, ccolor *c1, ccolor *c2, cmap_t &cmap, int len, const ccspace *cc)
{
  switch(direction)
    {
    case OPTS_CW:
      return generate_spiral_path(1,c1,c2,cmap,len,cc);
    case OPTS_CCW:
      return generate_spiral_path(-1,c1,c2,cmap,len,cc);
    case OPTS_STRAIGHT:
      return generate_straight_path(c1,c2,cmap,len,cc);
    }
  assert(0);
  return 0;
}

static void assign_locs(SpypalWaypoints_t &wps, std::vector<double> &lengths, unsigned s, unsigned e, int steps)
{
  // First, check to see if any waypoints are locked.  If so, divide and conquer.
  for(unsigned i = s+1; i != e; i++)
    if(wps[i].locked) 
      {
	assign_locs(wps,lengths,s,i,steps);
	assign_locs(wps,lengths,i,e,steps);
	return;
      }

  // Figure out index of start and end of range
  wps[e].ind = round(wps[e].loc*(steps-1));
  steps = wps[e].ind - wps[s].ind; 
  // Steps is now number of steps in our subrange.

  // Figure out total pathlength of this range.
  double pl = 0;
  for(unsigned i = s; i < e; i++)
    pl += lengths[i];
  if(s+1 != e && pl <= 0)
    {
      fprintf(stderr,"Illegal path length: %g (%d-%d)\n",pl,s,e);
      for(unsigned i = s; i < e; i++)
	fprintf(stderr," L[%d] = %g\n",i,lengths[i]);
    }
  // Figure out location of intermediate points.  Loc is a percentage of way down slider.
  for(unsigned i = s+1; i < e; i++)
    {
      wps[i].loc = wps[i-1].loc + (lengths[i-1]/pl)*(wps[e].loc-wps[s].loc);
    }
  if(wps[e-1].loc > wps[e].loc)
    {
      fprintf(stderr,"Weird: wps[e-1].loc > wps[e].loc was true.  %g > %g, delta=%g\n",
	      wps[e-1].loc, wps[e].loc, wps[e-1].loc-wps[e].loc);
    }

  // Assign indexes.  Be careful of rounding.
  for(unsigned i = s+1; i < e; i++)
    {
      wps[i].ind = wps[i-1].ind + round((wps[e].ind-wps[i-1].ind)*(wps[i].loc-wps[i-1].loc)/
					(wps[e].loc-wps[i-1].loc));
    }

}

/* Rearrange the indices to try to make sure each waypoint occurs (make the indices unique) */
static void uniquify_indices(std::vector<SpypalWaypoint> &wps)
{
  if(wps.size() < 1)
    return;
  unsigned e = wps.size()-1;
  unsigned s = 0;
  bool reloc=false;
  // Go back through and try to make all indices unique.
 retry:
  for(unsigned i = s+1; i <= e; i++)
    {
      if(wps[i].ind != wps[i-1].ind)
	continue;

      // Strategy; try to bump the index forward.  If that's not possible, bump the index backward.
      for(int j = i; j < e; j++)
	if(wps[j+1].ind > wps[j].ind+1)
	  {
	    for(int k = j; k >= i; k--)
	      {
		wps[k].ind++;
		reloc = true;
	      }
	    goto retry;
	  }

      for(int j = i-1; j > s; j--)
	if(wps[j-1].ind < wps[j].ind-1)
	  {
	    for(int k = j; k < i; k++)
	      {
		wps[k].ind--;
		reloc = true;
	      }
	    goto retry;
	  } 
      // This should only happen if there are more waypoints than the length of the colormap.
      fprintf(stderr,"Warning: found an unresolvable indexing conflict at %d.  Palette length will be bumped\n",i);
      wps[e].ind++;
      reloc = true;
      goto retry;
    }
  if(reloc)
    for(int i = s; i < e ; i++)
      wps[i].loc = ((double)wps[i].ind)/wps[e].ind;

}

double pathlength(cmap_t::iterator s, cmap_t::iterator e, const ccspace *metricspace)
{
  if(s == e)
    return 0.0;
  double l = 0;
  cmap_t::iterator i = s;  // I points to next, p points to previous
  i++;
  for(cmap_t::iterator p = s; i != e; i++,p++)
    l += dm_euclid.distance(*metricspace,*p,*i);
  return l;
}
// Generate a smooth path through all the colors colors, in direction direction.
// If force gamut, treat out-of-gamut colors intelligently.
// If even, space waypoints evenly instead of perceptually equal steps.
// Do distance calcs. in color space cspace.
// Locs gets the locations of the various waypoints on output.
void generate_go(std::vector<SpypalWaypoint> &waypoints, int direction, bool force_gamut, const ccspace *cspace, const ccspace *metricspace, int steps)
{
  static cmap_t long_cmap;         // A long version of the colormap for interp.
  assert(waypoints.size() >= 2);
  std::vector<double> lengths;     // Lengths of waypoint subpaths.
  std::vector<unsigned> indices;   // Indexes of waypoints in long colormap.
  std::vector<int> sizes;          // Number of points in the output for each path segment.
  std::vector<int> old_sizes;      // Number of points in the output that we actually have.
  indices.push_back(0);
  long_cmap.clear();
  waypoints[0].ind = 0;
  waypoints[0].loc = 0;
  waypoints[waypoints.size()-1].ind = steps-1;
  waypoints[waypoints.size()-1].loc = 1.0;
    
  // Generate an inital path that's quite long (4x longer than target), where each path
  // takes up the same length.
  for(unsigned i = 0; i < waypoints.size()-1; i++)
    {
      size_t start = long_cmap.size();
      double l1 =generate_path(direction,&waypoints[i].c,&waypoints[i+1].c,long_cmap, steps*4, cspace);
      if(metricspace != cspace)
	lengths.push_back(pathlength(long_cmap.begin()+start,long_cmap.end(),metricspace));
      else
	lengths.push_back(l1);
      indices.push_back(long_cmap.size());
    }
  
  // Clip the gamut on the long path
  if(force_gamut)
    for(unsigned i = 0; i < long_cmap.size(); i++)
      cc_sRGB.clip(long_cmap[i]);

  // Try to find a set of step-sizes for each subsection of the colormap which equalizes the lengths.
  int tries;
  for(tries = 0; tries < 6; tries++)
    {
      old_sizes = sizes;
      sizes.clear();

      // Assign locations of colors.
      assign_locs(waypoints,lengths,0,waypoints.size()-1,steps);
      uniquify_indices(waypoints);

#if 0
      fprintf(stderr,"Location dump\n");       	 
      for(unsigned i = 0; i < waypoints.size(); i++) 	 
	fprintf(stderr,"\t%g %d\n",waypoints[i].loc, waypoints[i].ind); 	 
      fprintf(stderr,"\n");
#endif

      // Check to see if we've picked the same locations.  If so, we're done!
      for(unsigned i = 0; i < lengths.size(); i++)
	sizes.push_back(waypoints[i+1].ind-waypoints[i].ind);

      if(!old_sizes.empty()) // If we managed to get a consistent set of sizes.
	{
	  assert(sizes.size() == old_sizes.size());
	  bool good = true;
	  for(unsigned i = 0; i < sizes.size(); i++)
	    {
	      if(sizes[i] != old_sizes[i])
		good = false;
	    }
	  if(good)
	    break;
	}

      // Try to figure out step sizes that give correct number of colors.
      cmap.clear();
      for(unsigned i = 1; i < waypoints.size(); i++)
	{
	  cmap_t old = cmap;

	  // Work out how many steps would make this "exact", then
	  int substeps = sizes[i-1];
	  double ssize = lengths[i-1]/(substeps);
	  static const double acc=1e-2;
	  magic_bisector b(ssize, -lengths[i-1]/(ssize*ssize),acc);
	  int tries=20;	  
	  double obs;
	  // zero-length traces -- some idiot picked the same color twice.
	  if(ssize <= 0)
	    {
	      for(int j = 0; j < substeps; j++)
		cmap.push_back(waypoints[i].c);
	      lengths[i-1] = 0;
	    }
	  else
	    while(ssize > 0 && !b.x(ssize) && (tries-- > 0))
	      {
		cmap = old;
		double err = smooth_subpath(long_cmap, cmap, *metricspace, &dm_euclid, ssize, indices[i-1], indices[i], lengths[i-1]);
		//		printf("substeps: %d  size: %d  err/ssize: %g  ssize: %g\n",substeps,cmap.size()-old.size(),err/ssize,ssize);
		obs = (double)(cmap.size()-old.size()) - substeps + err/ssize - 1.0 + acc*3.0;
		waypoints[i].ind=cmap.size();
		b.o(ssize, obs);
	      }
	  if(tries <= 0)
	    fprintf(stderr,"Warning: bisection failure (|%g| > %g).\n",obs,acc);
	  if(i == waypoints.size()-1)
	    {
	      cmap.push_back(waypoints[i].c);
	      waypoints[i].ind = cmap.size();
	    }
	}
    }

    // Fill the "locs" output.
    waypoints[0].loc =  0;
    waypoints[0].ind = 0;
    for(int i = 1; i < waypoints.size(); i++)
      waypoints[i].loc = ((double)waypoints[i].ind)/(cmap.size()-1);
    if(cmap.size() != steps)
      fprintf(stderr,"Warning: colormap length mismatch: %d != target (%d) after %d tries\n",
	      cmap.size(),steps,tries);
}

  /***********************************************************************/
  /* Spypal begin/end code; this differs if we're standalone or part of  */
  /* Spyview                                                             */
  /***********************************************************************/
spypal_wizard *spypal;

void init_spypal()
{
  spypal_sharedir=".";
  Fl::visual(FL_RGB8|FL_DOUBLE);
  spypal = new spypal_wizard();  
  make_spypal_text_window();
  spypal->init();
  spypal->do_update();
}

#ifdef SPYPAL_STANDALONE
std::string userdir;

int main(int argc, char **argv)
{
  spypal_sharedir=".";
  userdir=".";
  init_spypal();
  // Special: if the first arg is --convert, the remainder are colormaps to
  // load then save.
  if(argc > 1 && strcmp(argv[1],"--convert") == 0)
    {
      fprintf(stderr,"Converting .spp files to latest version.\n");
      for(int i = 2; i < argc; i++)
	{
	  fprintf(stderr,"Converting %s\n",argv[i]);
	  spypal->load(argv[i]);
	  spypal->save(argv[i]);
	}
      return 1;
    }
  spypal->win->show();

  Fl::run();
  return 0;
}
#endif
