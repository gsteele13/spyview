#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <string>
using namespace std;

#define LINESIZE 1024*1024

void usage()
{
  fprintf(stderr, "dat2pgm [-i{d,m,z,x}] [-l] [-v] [-m] [-x] [-p percent (50)] < infile.dat > outfile.pgm\n\n");
  fprintf(stderr, "    -l take log?\n");
  fprintf(stderr, "    -v verbose?\n");
  fprintf(stderr, "    -p percent of dynamic range to use in output image [50]\n");
  fprintf(stderr, "    -m generate a matlab ascii matrix file\n");
  fprintf(stderr, "    -x generate a spyview compatible mtx file instead\n");
  fprintf(stderr, "    -i Behaviour on incorrect columns (Drop line, fill with Mean, fill with Zero, [eXit])\n");

  exit(0);
}

int main(int argc, char **argv)
{
  int w, col;
  int h, row;
  int dolog = 0;
  int verbose = 0;
  typedef enum { DROP = 'd', FILL_MEAN = 'm', FILL_ZERO = 'z', EXIT = 'x'} incorrect_column_t;
  incorrect_column_t incorrect_column = EXIT;
  bool interp=false;
  bool matlab = false; // If set to true, output a text file appropriate for matlab instead of a pgm file.
  bool mtxout = false;

  char c;
  double percent = 50;
  
  typedef vector<string> comments_t;
  comments_t comments;
    
  while ((c = getopt(argc, argv, "lvhp:i:mxI")) != -1)
    {
      switch (c)
	{
        case 'I':
	  interp=true;
	  break;
	case 'i':
	  incorrect_column = static_cast<incorrect_column_t>(optarg[0]);
	  break;
	case '?':
	  usage();
	  break;
	case 'h':
	  usage();
	  break;
	case 'l':
	  dolog = 1;
	  break;
	case 'x':
	  mtxout = true;
	  break;
        case 'm':
          matlab = true;
	  break;
	case 'v':
	  verbose = 1;
	  break;
	case 'p':
	  if (sscanf(optarg, "%lf", &percent) != 1)
	    usage();
	  break;
	}
    }
  
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;

  vector<double> data;

  char linebuffer[LINESIZE];
  char *p;

  double datamax = -1e100;
  double datamin = 1e100;
  double datatmp;

  double absdatamin = 1e100;
  
  char sep[] = " \t\n\r";

  row = col = w = h = 0;
  
  while (1)
    {
      if (fgets(linebuffer, LINESIZE, stdin) == NULL)
	break;
   
      // Figure out what kind of line this is
      for(p = linebuffer; *p != 0 && isspace(*p); p++)
	;
      if(*p == '#')
	{
	  comments.push_back(p);
	  continue;
	}
      else if(*p == '\n' || *p == 0)
	continue;

      col = 0;
      p = strtok(linebuffer, sep);
      while (p != NULL)
	{
	  if (sscanf(p, "%lf", &datatmp) != 1)
	    {
	      fprintf(stderr, "dat2pgm: invalid data at row %d col %d: \"%s\"\n", row, col, p);
	      exit(-1);
	    }
	  data.push_back(datatmp);
	  if (datatmp < datamin) datamin = datatmp;
	  if (datatmp > datamax) datamax = datatmp;
	  if (fabs(datatmp) < absdatamin && datatmp != 0.0) absdatamin = fabs(datatmp);
	  col++;
	  p = strtok(0, sep);
	}
      if (row == 0)
	w = col;
      else if (w != col)  // Col is the number of columns we have read, w is the number the other lines have had.
	{
	  if (col == 0)
	    break;      // ignore empty lines at the end.
	  if(col < w) // This row is too short
	    {
	      double d = 0; // This is used for ZERO; Mean updates the value so they can share insertion code.	 
	      switch(incorrect_column)
		{
		case EXIT:	      
		  fprintf(stderr, "number of columns %d at row %d does not match width of previous rows (%d)\n", col, row, w);
		  exit(-1);
		case DROP:
		  data.erase(data.begin()+(row*w),data.end());
		  row--;
		  break;
		case FILL_MEAN:
		  for(int i = 0; i < col; i++)
		    d += data[row*w + i];
		  d = d / col;
		case FILL_ZERO: // Warning! Falling case!
		  assert(col < w);
		  for(int i = 0; i < (w-col); i++)
		    data.push_back(d);
		  break;
		default:
		  fprintf(stderr, "Invalid argument for -i: %c\n",incorrect_column);
		  exit(-1);
		}
	    }
	  else // This row is too long
	    {
	      double d = 0; // This is used for ZERO; Mean updates the value so they can share insertion code.	 
	      switch(incorrect_column)
		{
		case EXIT:	      
		  fprintf(stderr, "number of columns %d at row %d does not match width of previous rows (%d)\n", col, row, w);
		  exit(-1);
		case FILL_MEAN:		  
		  fprintf(stderr, "number of columns %d at row %d does not match (too big, used to be %d); fill_mean is not ready yet for this case.\n",col,row,w);
		  fprintf(stderr, "Using fill_zero.");
		case FILL_ZERO: // Warning! Falling case!		  
		  {
		    fprintf(stderr, "number of columns %d at row %d does not match (too big, used to be %d)\n",col,row,w);
		    vector<double> dtmp;
		    dtmp.resize((row+1)*col);
		    for(int i = 0; i < row; i++)
		      {
			for(int j = 0; j < w; j++) // This is very inefficent, but easy to understand.
			  dtmp[i*col+j] = data[i*w+j];
			for(int j = w; j < col; j++)
			  dtmp[i*col+j] = 0.0;
		      }
		    for(int j = 0; j < col; j++) // Don't forget the last line.
		      dtmp[row*col + j] = data[row*w+j];
		    w = col;
		    data = dtmp;
		  }
		  break;
		case DROP:
		  data.erase(data.begin(),data.begin()+(row*w));
		  row = 0;
		  w = col;
		  break;
		default:
		  fprintf(stderr, "Invalid argument for -i: %c\n",incorrect_column);
		  exit(-1);
		}
	      
	    }
	  
	}
      row++;
      assert(data.size() == row * w);
    }
  h = row;

  if (verbose)
    fprintf(stderr, "%d x %d points\nMax = %e\nMin = %e\nAbsdatamin = %e\n", 
	    w, h, datamax, datamin, absdatamin);
  
  unsigned char tmp;
  
  if(interp)
    {
      double lastd=0;
      for(int i = 0; i < h*w; i++)
	if(isnan(data[i]))
	  {
	    data[i]=lastd; //(datamax+datamin)/2.0;
	    lastd=0;
	  }
	else
	  lastd=data[i];
    }
    else      
      for(int i = 0; i < h*w; i++)
	if(isnan(data[i]))
	  data[i]=0; //(datamax+datamin)/2.0;

  if (dolog)
    {
      datamax = -1e100;
      datamin = 1e100;
      for (int i=0; i<h; i++)
	{
	  for (int j=0; j<w; j++)
	    {  
	      data[i*w+j] = log10(fabs(data[i*w+j])+absdatamin);
	      if (data[i*w+j] < datamin) datamin = data[i*w+j];
	      if (data[i*w+j] > datamax) datamax = data[i*w+j];
	    }
	}
      if (verbose)
	fprintf(stderr, "LogMax = %g\nLogMin = %g\n", datamax, datamin);
    }

  if (mtxout)
    {
      fprintf(stdout, "1 %d %d 8\n", h, w);
      for (int i=0; i<h; i++)
	{
	  for(int j=w-1; j>-1; j--)
	    {
	      double t = data[i*w+j];
	      fwrite(&t, sizeof(t), 1, stdout);
	    }
	}
      return 0;
    }

  if(matlab) // This is not quite a null-op; we strip comments and fill out the lines to all be the same length.
    {
      for(int i = 0; i < h; i++)
	{
	  for(int j = 0; j < w; j++)
	    {
	      double t = data[i*w+j];
	    fprintf(stdout,"%.9g ",t);
	    }
	  fprintf(stdout,"\n");
	}
      return 0;
    }

  // Rescale data min and max to remap to the requested dynamic range 
  double center = (datamax+datamin)/2;
  double half_width  = (datamax-datamin)/2;

  datamin = center - half_width * 100 / percent;
  datamax = center + half_width * 100 / percent;

 
  fprintf(stdout, "P5\n%d %d\n#zmin %e\n#zmax %e\n",w,h, datamin, datamax);
  fprintf(stdout, "#xmin %d\n#xmax %d\n#ymin %d\n#ymax %d\n", 0, h, w, 0);
  fprintf(stdout, "#xunit X\n#yunit Y\n#zunit Z\n");

  for(comments_t::iterator i = comments.begin(); i != comments.end(); i++)
    fprintf(stdout,"%s",i->c_str());
  fprintf(stdout,"65535\n");

  for (int i=0; i<h; i++)
    {
      for (int j=0; j<w; j++)
	{
	  datatmp = (data[i*w+j] - datamin)/(datamax - datamin) * 65535.0;
	  //fprintf(stdout, "%.0f ", datatmp);
	  if (datatmp < 0.0 || datatmp > 65535.0 + 1e-6)
	    {
	      fprintf(stderr, "\n%d %d %f ", i,j, datatmp);
	    }
	  tmp = (unsigned char) ((int)datatmp / 256);
	  fwrite(&tmp, 1, 1, stdout);
	  tmp = (unsigned char) ((int)datatmp % 256);
	  fwrite(&tmp, 1, 1, stdout);
	}
      //fprintf(stdout,"\n");
    }
  
  if (verbose)
    fprintf(stderr, "Done.\n");
}
