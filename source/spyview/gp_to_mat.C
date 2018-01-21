#include <stdlib.h>
#include <string>
#include <vector>
#include <assert.h>
#include <map>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "../config.h"

using namespace std;

char *skipwhite(char *s, bool space=true) // skip white if space=true,
  // Skip text if space is false
{
  while((*s != 0) && ((isspace(*s) != 0) == space))
    s++;
  return s;
}

void getfields(char *str, std::vector<string> &fields)
{
  bool quit = false;
  fields.clear();
  // First, delete any comment
  char *c = strchr(str,'#');
  if(c != NULL)
     *c = 0;
  while(!quit)
    {
      if(*str == 0)
	break;
      str = skipwhite(str,true);
      char *p = skipwhite(str,false);
      if(p == str)
	break;
      if(*p != 0)
        *p++ = 0;
      fields.push_back(string(str));
      str = p;
    }
}

void dumpfields(vector<string> s)
{
  for(vector<string>::iterator i = s.begin(); i != s.end(); i++)
    fprintf(stderr,"[%s] ",i->c_str());
  fprintf(stderr,"\n");
}

typedef map<double,int> axis_t;

class triple
{
public:
  double x,y;
  string z;
  triple(double xp=NAN, double yp=NAN, const string &zp="NAN")
    : x(xp), y(yp), z(zp) {};
};

int setup_axis(axis_t &a, double tol) // Return number of unique cols
{
  int ysize=0;
  double y0=a.begin()->first;
  a.begin()->second=0;
  assert(tol > 0);
  for(axis_t::iterator i = a.begin(); i != a.end(); i++)
    {
      if(fabs(i->first-y0) < tol)
	i->second=ysize;
      else
	i->second=++ysize;
      y0=i->first;	 	  
      //      fprintf(stderr,"-- %g %d\n",i->first, i->second);
    }
  return ysize+1;
}

void usage(char **argv)
{
  printf("%s: Take data in a somewhat more robust pm3d format\n"
	 "  and map it into a matrix, with special comments for\n"
	 "  dat2pgm.  Data can be missing rows and columns, and\n"
	 "  can be in any order.\n"
	 " Usage: %s [l]xcol [l]ycol zcol [xstep] [ystep]\n"
	 "  where xcol, ycol, and zcol are integer column numbers\n"
	 "  l before any column means take the log first.\n"
	 "  If xstep or ystep is not NAN, we step through the data"
	 "  with that increment, allowing this to be used with\n"
	 "  unevenly spaced data\n",
	 argv[0],argv[0]);
	 
}
int main(int argc, char **argv)
{
  if(argc < 4)
    {
      usage(argv);
      return 1;
    }

  int xc,yc,zc;
  bool logx=false, logy=false;
  double ystep=NAN, xstep=NAN;
  if(argv[1][0] == 'l')
    {
      logx = true;
      xc = atoi(argv[1]+1) -1;
    }
  else
    xc = atoi(argv[1]) -1;

  if(argv[2][0] == 'l')
    {
      logy = true;
      yc = atoi(argv[2]+1) -1;
    }
  else
    yc = atoi(argv[2]) -1;
  zc = atoi(argv[3]) -1;
  if(argc >= 5)
    xstep=atof(argv[4]);
  if(argc >= 6)
    ystep=atof(argv[5]);

  assert(xc >= 0);
  assert(yc >= 0);
  assert(zc >= 0);
  int maxc = max(max(xc,yc),zc);

  //  fprintf(stderr,"X: %d Y: %d Z: %d\n",xc,yc,zc);
  char buf[65536];
  vector<string> fields;

  axis_t xs;
  axis_t ys;  
  vector<triple> pts;
  // Accumulate the data.  xs will get a list of unique x coords,
  // yp will get a list of unique y coords
  // pts will get all our 3D triples.
  bool x_increasing = false, x_increasing_set = false;
  bool y_increasing = false, y_increasing_set = false;
  double lx=NAN, ly=NAN;
  int line=0;
  while(fgets(buf,sizeof(buf),stdin) != NULL)
    {     
      getfields(buf,fields);
      line++;
      if((int)fields.size() == 0)
	continue;
      if((int)fields.size() <= maxc)
	{
	  fprintf(stderr,"Warning: too few fields (%d<%d) on line %d\n",
		  fields.size(), maxc, line);
	  continue;
	}
      double xp = atof(fields[xc].c_str());
      double yp = atof(fields[yc].c_str());
      if(logx)
	xp = log10(xp);
      if(logy)
	yp = log10(yp);
      if(!x_increasing_set)
	{
	  if(isnan(lx)) 
	    lx=xp;
	  else
	    if(lx != xp)
	      {
		x_increasing = xp > lx;
		x_increasing_set = true;
	      }
	}
      if(y_increasing_set)
	{
	  if(isnan(ly)) 
	    ly=yp;
	  else
	    if(ly != yp)
	      {
		y_increasing = yp > ly;
		y_increasing_set = true;
	      }
	}

      //      printf("%g,%g: %s\n",xp,yp,fields[zc].c_str());
      xs[xp]=-1;
      ys[yp]=-1;
      pts.push_back(triple(xp,yp,fields[zc]));
    }

  // Set up our X & Y column maps, as well as our 3D data array.
  // Find the bounds
  double xmin = xs.begin()->first;
  double xmax = xs.rbegin()->first;
  double ymin = ys.begin()->first;
  double ymax = ys.rbegin()->first;
  fprintf(stdout,"#pre-cull size: ",_STF,",",_STF,"\n",xs.size(),ys.size());

  // Setup our mapping from double to column number
  int xsize,ysize;
  ysize = setup_axis(ys,fabs(ymax-ymin)*1e-10);
  xsize = setup_axis(xs,fabs(xmax-xmin)*1e-10);

  //  fprintf(stdout,"#xsize,ysize %d,%d\n",xsize,ysize);
  vector<string> data(xsize*ysize,"NAN");
  for(vector<triple>::iterator i = pts.begin(); i != pts.end(); i++)
    {
      int x = xs[i->x];
      int y = ys[i->y];
      assert(x >= 0);
      assert(y >= 0);
      assert(x < xsize);
      assert(y < ysize);
      data[y*xsize+x]=i->z;
    }
// x-interp
  for(int y = 0; y < ysize; y++)
    {
      for(int x=1; x < xsize; x++)
	{
	  if(isnan(atof(data[y*xsize+x].c_str())))
	    data[y*xsize+x]=data[y*xsize+x-1];
	}
    }

  if(!x_increasing)
    {
      swap(xmin,xmax);
      xstep = -xstep;
    }
  if(!y_increasing)
    {
      swap(ymin,ymax);
      ystep = -ystep;
    }

  if(isnan(ystep) && isnan(xstep))
    {
      for(int y = y_increasing ? 0 : ysize-1; 
	  y_increasing ? y < ysize : y >= 0; y_increasing ? y++ : y--)
	{
	  for(int x = x_increasing ? 0 : xsize-1; 
	      x_increasing ? x < xsize : x >= 0; x_increasing ? x++ : x--)
	    {
	      fprintf(stdout,"%s ",data[y*xsize+x].c_str());
	    }
	  fprintf(stdout,"\n");
	}
    }
  else if(isnan(ystep))
    {	    
      for(int y = y_increasing ? 0 : ysize-1; 
	  y_increasing ? y < ysize : y >= 0; y_increasing ? y++ : y--)
	{
	  for(double x = xmin; x*xstep < xstep*(xmax+xstep/2.0); x += xstep)
	    {
	      axis_t:: iterator xb = xs.lower_bound(x-fabs(xstep)/2.0);
	      int ix = xb->second;
	      fprintf(stdout,"%s ",data[y*xsize+ix].c_str());
	    }
	  fprintf(stdout,"\n");
	}
    }
  else if(isnan(xstep))
    {	    
      for(double y = ymin; y*ystep < ystep*(ymax+ystep/2.0); y += ystep)
	{
	  axis_t:: iterator yb = ys.lower_bound(y-fabs(ystep)/2.0);
	  int iy = yb->second;
	  //	  fprintf(stderr,"%g %d (%g)\n",y,iy,yb->first);
	  for(int x = x_increasing ? 0 : xsize-1; 
	      x_increasing ? x < xsize : x >= 0; x_increasing ? x++ : x--)
	    {
	      fprintf(stdout,"%s ",data[iy*xsize+x].c_str());
	    }
	  fprintf(stdout,"\n");
	}      
    }
  else
    {	    
      for(double y = ymin; y*ystep < ystep*(ymax+ystep/2.0); y += ystep)
	{
	  axis_t:: iterator yb = ys.lower_bound(y-fabs(ystep)/2.0);
	  int iy = yb->second;	  
	  //	  fprintf(stderr,"%g %d (%g)\n",y,iy,yb->first);
	  for(double x = xmin; x*xstep < xstep*(xmax+xstep/2.0); x += xstep)
	    {
	      axis_t:: iterator xb = xs.lower_bound(x-fabs(xstep)/2.0);
	      int ix = xb->second;
	      fprintf(stdout,"%s ",data[iy*xsize+ix].c_str());
	    }
	  fprintf(stdout,"\n");
	}
    }
  // Output our structured comments.
  fprintf(stdout,"#xmin %g\n#xmax %g\n",xmin,xmax); 
  fprintf(stdout,"#ymin %g\n#ymax %g\n",ymax,ymin);
  return 0;
}

