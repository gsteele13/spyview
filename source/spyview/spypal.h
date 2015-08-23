#ifndef __spypal_h__
#define __spypal_h__
#include <string>
#include "cclass.H"
#include <vector>
class spypal_wizard;

class SpypalWaypoint
{
 public:
  ccolor c;     // Color of waypoint
  double loc;   // pcntage of way through colormap
  int ind;      // Actual index of point
  bool locked;  // true=don't move point.
  
 SpypalWaypoint() : loc(0), locked(false)
    {};

  // Needed for sort
  bool operator<(const SpypalWaypoint &c2)
    const { return loc < c2.loc; };
};

typedef std::vector<SpypalWaypoint> SpypalWaypoints_t;

// Functions for manipulating general colormap stats.
void show_as_text();
void update_text();

void dump_colormap_text(const char *fname);
void dump_colormap_ppm(const char *fname);
void dump_colormap_memory();

// Part of core logic; this function actually makes colormap.s
void generate_go(std::vector<SpypalWaypoint> &waypoints, 
		 int direction, bool force_gamut, const ccspace *cspace, 
		 const ccspace *metricspace, int steps);

extern void (*colormap_callback)(); // Called whenever colormap changes.
extern unsigned char *spypal_colormap; // Current colormap
extern size_t spypal_colormap_size ; // Colormap size.

void init_spypal(); // Set everything up.
extern spypal_wizard *spypal;

extern std::string spypal_sharedir; // Directory where documentation lives.

// Global arrays for spypal to manipulate
typedef std::vector<ccolor> cmap_t;
extern cmap_t cmap;
extern std::vector<bool> spypal_clipped;

#define OPTS_EQUIANGLE 1
#define OPTS_EUCLID 2
#define OPTS_HUE 3

#define OPTS_STRAIGHT 1
#define OPTS_CW 2
#define OPTS_CCW 3
#endif
