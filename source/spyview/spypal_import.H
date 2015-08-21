#ifndef __SPYPAL_IMPORT_H__
#define __SPYPAL_IMPORT_H__
#include "spypal.h"
double spypal_bisect_anneal(SpypalWaypoints_t &wps_out, const ccspace *cs, unsigned char *c1, unsigned l);
double spypal_anneal(SpypalWaypoints_t &wps, const ccspace *cs, unsigned char *c1, unsigned l, bool init=true);
double spypal_worst_error(); // Worst error on last fit.
#endif
