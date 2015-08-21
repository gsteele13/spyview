#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <set>
#include <algorithm>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include "../config.h"

#ifndef DBL_MAX
#define DBL_MAX 1e100
#endif 

static const char *delim=" \t,";

typedef struct
{
  double minx, maxx;
  int width;
  int height;
  double zfill;
  double zrad;
  int xcol;
  int ycol;
  int zcol;
  double minz;
  double maxz;
  typedef enum { CLOSEST, BILINEAR } interp_t;
  interp_t interp;
} config_t;
static config_t config = { -1,-1,200, 200, 0.0, -2, 0, 1, 2 , 0,0,config_t::CLOSEST};

void usage()
{
  fprintf(stderr,"grid_data -Xminx,maxx -c minz -C maxz -x xcol -y ycol -z zcol -w width -h height -[b]ilinear< data.in > data.mat\n");
}

inline static double sqr(double x) { return x*x; };

class point
{
public:
  double x,y,z;
  inline bool operator<(const point &p) const
  {
    return (y < p.y || (y == p.y && x < p.x));
  }

  inline void normalize()
  {
    *this /= sqrt(*this * *this);
  }
  inline const point& operator*=(double s)
  {
    x *= s;
    y *= s;
    z *= s;
    return *this;
  }

  inline const point &operator/=(double s)
  {
    return operator*=(1.0/s);
  }

  inline point operator-(const point &p2) const
  {
    return point(x-p2.x,y-p2.y,z-p2.z);
  }
  
  inline double operator*(const point &p1) const
  {
    return x*p1.x + y*p1.y + z*p1.z;
  }
  
  inline point cross(const point &p1) const
  {
    return point(y*p1.z-z*p1.y,z*p1.x-x*p1.z,x*p1.y-y*p1.x);
  }
  
  inline double d(const point &p) const { return sqrt(sqr(p.x-x) + sqr(p.y-y) + sqr(p.z-z)); };
  inline double dxy(const point &p) const { return sqrt(sqr(p.x-x) + sqr(p.y-y)); };
  inline double dsxy(const point &p) const { return sqr(p.x-x) + sqr(p.y-y); };
  void max(const point &p) 
  {
    if(p.x > x)
      x = p.x;
    if(p.y > y)
      y = p.y;
    if(p.z > z)
      z = p.z;
  }
  void min(const point &p)
  {
    if(p.x < x)
      x = p.x;
    if(p.y < y)
      y = p.y;
    if(p.z < z)
      z = p.z;
  }

  point(double xp = 0, double yp = 0, double zp = 0) : x(xp), y(yp) , z(zp) {};
};

typedef std::set<point> data_t;
data_t data;

data_t::iterator find_closest_xy(const point &p)
{
  data_t::iterator init = data.lower_bound(p);
  data_t::iterator id = init;
  double d = p.dsxy(*id);

  for(data_t::iterator i = init; i != data.end(); i++)
    {
      double dy = p.y - i->y;
      if(dy*dy >= d)
	break;
      double dp = p.dsxy(*i);
      if(dp < d)
	{
	  id = i;
	  d = dp;
	}
    }
  
  for(data_t::iterator i = init; ; i--)
    {
      double dy = p.y - i->y;
      if(dy*dy >= d)
	break;
      double dp = p.dsxy(*i);
      if(dp < d)
	{
	  id = i;
	  d = dp;
	}
      if(i == data.begin())
	break;
    }

  return id;
}

inline bool bounded(double x1, double x2, double xb)
{
  if(x1 > x2)
    return (x1 >= xb) && (xb >= x2);
  else
    return (x2 >= xb) && (xb >= x1);
}

data_t::iterator find_next_closest_xy(const point &p, data_t::iterator j)
{
  assert(data.size() > 2);
  data_t::iterator init = data.lower_bound(p);
  if(init == j)
    {
      init++;
      if(init == data.end())
	{
	  init=j;
	  init--;
	}
    }
  data_t::iterator id = data.end();
  double d = DBL_MAX;

  for(data_t::iterator i = init; i != data.end(); i++)
    {
      if(i == j)
	continue;
      double dy = p.y - i->y;
      if(dy*dy >= d)
	break;
      double dp = p.dsxy(*i);
      if((dp < d) && (bounded(i->x, j->x, p.x) || bounded(i->y, j->y, p.y)))
	{
	  id = i;
	  d = dp;
	}
    }
  for(data_t::iterator i = init; ; i--)
    {
      if(i == j)
	continue;
      double dy = p.y - i->y;
      if(dy*dy >= d)
	break;
      double dp = p.dsxy(*i);
      if(dp < d && (bounded(i->x, j->x, p.x) || bounded(i->y,j->y,p.y)))
	{
	  id = i;
	  d = dp;
	}
      if(i == data.begin())
	break;
    }
  return id;
}

inline bool double_bounded(const point &p1, const point &p2, const point &p3, const point &pb)
{
  if(bounded(p1.x, p2.x, pb.x))
    {
      if(bounded(p1.y, p3.y, pb.y) || bounded(p2.y, p3.y, pb.y))
	return true;
    }
  if(bounded(p1.y, p2.y, pb.y))
    {
      if(bounded(p1.x, p3.x, pb.x) || bounded(p2.x, p3.x, pb.x))
	return true;
    }
  return false;
}
static const double ortho_cutoff=M_PI*60.0/180.0;

data_t::iterator find_close_ortho_point(const point &p, data_t::iterator  i, data_t::iterator j)
{
  const point &p1=*i;
  const point &p2=*j;
  data_t::iterator init = data.lower_bound(p);
  while(init == j || init == i)
    {
      init++;
      if(init == data.end())
	{
	  init = data.lower_bound(p);
	  break;
	}
    }
  while(init == j || init == i)
    {
      assert(init != data.begin());
      init--;
    }
  data_t::iterator id = data.end();
  double d = DBL_MAX;

// construct the vector normal to the line between the two points.
  point zhat(0,0,1);
  point norm(zhat.cross(p2-p1));
//  fprintf(stderr,"Normal: %f %f %f\n",norm.x, norm.y, norm.z);

  for(data_t::iterator k = init; k != data.end(); k++)
    {
      if((k == i) || (k == j))
	continue;
      double dy = p.y - k->y;
      if(dy*dy >= d)
	break;
      const point &p3=*k;
      double dp = p.dsxy(p3);
      if(dp < d  && double_bounded(*i, *j, *k, p))
	{
	  point d1(p3-p1);
	  d1.z = 0;
	  double theta1 = acos(fabs((d1*norm)/(sqrt(d1*d1)*sqrt(norm*norm))));
	  point d2(p3-p2);
	  d2.z = 0;
	  double theta2 = acos(fabs((d2*norm)/(sqrt(d2*d2)*sqrt(norm*norm))));
//	  fprintf(stderr,"%f %f %f\n", theta1, theta2, ortho_cutoff);
	  if((fabs(theta1) < ortho_cutoff) || (fabs(theta2) < ortho_cutoff))
	    {
	      d = dp;
	      id = k;
	    }
	}
    }
  for(data_t::iterator k = init;; k--)
    {
      if((k != i) && (k != j))
	{
	  double dy = p.y - k->y;
	  if(dy*dy >= d)
	    break;
	  const point &p3=*k;
	  double dp = p.dsxy(p3);
	  if(dp < d  && double_bounded(*i, *j, *k, p))
	    {
	      point d1(p3-p1);
	      d1.z = 0;
	      double theta1 = acos(fabs((d1*norm)/(sqrt(d1*d1)*sqrt(norm*norm))));
	      point d2(p3-p2);
	      d2.z = 0;
	      double theta2 = acos(fabs((d2*norm)/(sqrt(d2*d2)*sqrt(norm*norm))));
//	  fprintf(stderr,"%f %f %f\n", theta1, theta2, ortho_cutoff);
	      if((fabs(theta1) < ortho_cutoff) || (fabs(theta2) < ortho_cutoff))
		{
		  d = dp;
		  id = k;
		}
	    }
	}
      if(k == data.begin())
	break;
    }

  return id;
}

double bilinear_interp(const point &p, data_t::iterator p1i, data_t::iterator p2i, data_t::iterator p3i)
{
  const point &p1 = *p1i;
  const point &p2 = *p2i;  
  const point &p3 = *p3i;

//  fprintf(stderr,"%d,%d,%d\n",p1i,p2i,p3i);
//  fprintf(stderr,"%f,%f %f,%f %f,%f\n",p1.x,p1.y, p2.x,p2.y, p3.x,p3.y);
  point v1(p2-p1);
  point v2b(p3-p1);
  point v2(v2b-v1*(v1*v2b)/(v1*v1));
  double v1z = v1.z;
  double v2z = v2.z;
  v1.z = 0;
  v2.z = 0;
  point dv(p-p1);
  dv.z = 0;
  double c1 = (dv*v1)/(v1*v1);
  double c2 = (dv*v2)/(v2*v2);
  double z = p1.z + v1z*c1 + v2z*c2;
  return z;
}

int main(int argc, char **argv)
{
  int maxcol;
  int c;
  while((c = getopt(argc, argv, "x:y:z:w:h:c:C:bX:")) != -1)
  {
    switch(c)
      {
      case 'b':
	config.interp = config_t::BILINEAR;
	break;
      case 'X':
	sscanf(optarg,"%lf,%lf",&config.minx, &config.maxx);
	break;
      case 'c':
	config.minz = atof(optarg);
	break;
      case 'C':
	config.maxz = atof(optarg);
	break;
      case 'x':
	config.xcol = atoi(optarg)-1;
	break;
      case 'y':
	config.ycol = atoi(optarg)-1;
	break;
      case 'z':
	config.zcol = atoi(optarg)-1;
	break;
      case 'w':
	config.width = atoi(optarg)-1;
	break;
      case 'h':
	config.height = atoi(optarg)-1;
	break;
      default:
	usage();
	exit(0);
      }
  }
  maxcol = config.xcol;
  if(config.ycol > maxcol)
    maxcol = config.ycol;
  if(config.zcol > maxcol)
    maxcol = config.zcol;

  assert(config.xcol >= 0);
  assert(config.ycol >= 0);
  assert(config.zcol >= 0);


// Read in the data file
  char buf[4096];
  std::vector<double> row;
  while(fgets(buf,sizeof(buf),stdin) != NULL)
    {
      char *p = strchr(buf,'#');
      if(p != NULL)
	*p = 0;
      if(buf[0] == 0) // special optimization for blank lines and comment lines
	continue;
      row.clear();
      p = strtok(buf,delim);
      while(p != NULL)
	{
	  double d;
	  if(sscanf(p,"%lf",&d) != 1)
	    d = 0.0;
	  row.push_back(d);
	  p = strtok(NULL,delim);
	}
      if((int)row.size() > maxcol)
	{
	  if((config.minz == 0 || row[config.zcol] > config.minz) && (config.maxz == 0 || row[config.zcol] < config.maxz))
	    data.insert(point(row[config.xcol],row[config.ycol],row[config.zcol]));	
	}
    }

  point min(*data.begin()), max(*data.begin());
  for(data_t::iterator i = data.begin(); i != data.end(); i++)
    {
      min.min(*i);
      max.max(*i);
    }
  if(config.minx != -1)
    min.x = config.minx;
  if(config.maxx != -1)
    max.x = config.maxx;
  
//  sort(data.begin(), data.end());
  fprintf(stderr,"Read in " _STF " points\n",data.size());
  fprintf(stderr,"Data range is [%f-%f],[%f-%f],[%f-%f]\n",min.x,max.x,min.y,max.y,min.z,max.z);

  printf("#xmin %lf\n",min.x);
  printf("#ymin %lf\n",max.y);
  printf("#xmax %lf\n",max.x);  
  printf("#ymax %lf\n",min.y);
  double out[config.width][config.height];
  for(int j = 0; j < config.height; j++)
    {
      double y = j * (max.y-min.y)/config.height + min.y;
      fprintf(stderr,"%d/%d\r",j,config.height);
      for(int i = 0; i < config.width; i++)
	
	{	  
	  double x = i * (max.x-min.x)/config.width + min.x;
	  double z;
	  switch(config.interp)
	    {
	    case config_t::CLOSEST:
	      z = find_closest_xy(point(x,y,0))->z;
	      break;
	    case config_t::BILINEAR:
	      {
		data_t::iterator p1 = find_closest_xy(point(x,y,0));
		data_t::iterator p2 = find_next_closest_xy(point(x,y,0),p1);
		if(p2 == data.end())
		  z = 0;
		else
		  {
		    data_t::iterator p3 = find_close_ortho_point(point(x,y,0),p1,p2);
		    if(p3 == data.end())
		      z = 0;
		    else 
		      z = bilinear_interp(point(x,y,0),p1,p2,p3);
		  }
	      }
	      break;
	    }
	  out[i][j] = z;
	  printf("%.20g ",out[i][j]);
	}
      printf("\n");
    }
  fprintf(stderr,"\n");
}
