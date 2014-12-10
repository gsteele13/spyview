#include <assert.h>
#include "ImageData.H"
#include "string.h"
#include <Fl/filename.H>
#include <stdlib.h>
#include <FL/Fl.H>
#include <time.h>
#include "misc.h"
#include "mypam.h"
#include "../config.h"

#include <algorithm>
// From http://www.redhat.com/docs/manuals/enterprise/RHEL-3-Manual/gcc/variadic-macros.html

#define badfile(A, ...) {info(A, ## __VA_ARGS__); return -1;}
#define badfilec(A, ...) {fclose(fp); info(A, ## __VA_ARGS__); return -1;} // ALso close the file.

#define LINESIZE 1024*1024

#define MTX_HEADER_SIZE 256*10

// mingw32 does not seem to implement NAN properly: it seems to be set to 0.
#ifdef WIN32
#define NAN INFINITY
#define isnan(A) isinf(A)
#endif

//some handy local functions

double parse_reading(char *line, int col);
double nextreading(FILE *fp, int col, int &lnum);
int nextline(FILE *fp, char *buf);

ImageData::ImageData()
{
  data_loaded = 0;
  orig_data = NULL;
  qmin = xmin = ymin = 0;
  qmax = xmax = ymax = 1;
  auto_quant = 1;
  auto_quant_percent = 50.0;
  incorrect_column = COPY_ADJACENT;
  mtx_cut_type = XY;
  mtx_index = 0;
  do_mtx_cut_title = false;
  data3d = false;
  gpload_type = COLUMNS;
  gp_column = 2;
  datfile_type = MATRIX;
  fallbackfile_type = PGM;
}

ImageData::~ImageData()
{
  clear();
}

void ImageData::clear()
{
  if (orig_data != NULL) //was using "data_loaded", but we had a big memory leak...
    {
      //info("clearing image data arrays\n");
      delete [] orig_data;
      delete [] raw_data;
      delete [] quant_data;
      delete [] threshold_reject;
      orig_data = NULL;
    }
  data_loaded = 0;
}

void ImageData::reallocate()
{
  clear();
  orig_data = new double[width*height];
  raw_data = new double[width*height];
  quant_data = new int[width*height];
  threshold_reject = new bool[width*height];
  data_loaded = 1;
}

void ImageData::resize_tmp_arrays(int new_width, int new_height)
{
  // Only ever make things bigger
  if (new_width*new_height > orig_width*orig_height)
    {
      delete [] quant_data;
      delete [] threshold_reject;
      quant_data = new int[new_width*new_height];
      threshold_reject = new bool[new_width*new_height];
    }
}

void ImageData::reset()
{
  memcpy(raw_data, orig_data, sizeof(double)*orig_width*orig_height);
  width = orig_width;
  height = orig_height;
  xmin = orig_xmin;
  ymin = orig_ymin;
  xmax = orig_xmax;
  ymax = orig_ymax;
  xname = orig_xname;
  yname = orig_yname;
}

void ImageData::saveMTX(const char *filename)
{
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL)
    {
      info("error opening file %s", filename);
      return;
    }

  fprintf(fp, "Units, %s,"
	  "%s, %e, %e,"
	  "%s, %e, %e," 
	  "Nothing, 0, 1\n",
	  zname.c_str(), 
	  xname.c_str(), xmin, xmax,
	  yname.c_str(), ymax, ymin); 
  fprintf(fp, "%d %d 1 8\n", width, height);

  for (int i=0; i<width; i++)
    for (int j=0; j<height; j++) 
      fwrite(&raw(i,j), sizeof(double), 1, fp);
  fclose(fp);
}

void ImageData::shift_data(int after_row, int offset)
{
  double *new_row = new double[width];   // less efficient, but easier to read the code
  for (int j=0; j<height; j++)
    {
      if (j>=after_row)
	{
	  for (int i=0; i<width; i++)
	    {
	      int i2 = i - offset; 
	      if (i2 > width-1) i2 = i2 % (width);
	      if (i2 < 0) i2 = i2 + (-i2/width+1)*width;
	      if (i2>=0 && i2 < width) 
		new_row[i] = raw(i2,j);
	      else
		new_row[i] = -1e2; // we should never get here if we
				   // did the bounds checking right
	    }
	  for (int i=0; i<width; i++)
	    raw(i,j) = new_row[i];
	}
    }
  delete [] new_row;
}

void ImageData::rescale_data(double new_qmin, double new_qmax)
{
  for (int i = 0; i<width*height; i++)
    raw_data[i] = new_qmin + (raw_data[i]-qmin)/(qmax-qmin)*(new_qmax-new_qmin);
  qmax = new_qmax;
  qmin = new_qmin;
}

void ImageData::store_orig()
{
  memcpy(orig_data, raw_data, sizeof(double)*width*height);
  orig_width = width;
  orig_height = height;
  orig_xmin = xmin;
  orig_ymin = ymin;
  orig_xmax = xmax;
  orig_ymax = ymax;
  orig_xname = xname;
  orig_yname = yname;
}

void ImageData::load_int(int *data, 
			 int w, int h,
			 double x1, double x2,
			 double y1, double y2,
			 double z1, double z2)
{
  xmin = isnan(x1) ? x1 : 0;
  xmax = isnan(x2) ? x2 : width-1;
  ymin = isnan(y1) ? y1 : 0;
  ymax = isnan(y2) ? y2 : height-1;
  qmin = isnan(z1) ? z1 : 0;
  qmax = isnan(z2) ? z2 : QUANT_MAX;
  width = w;
  height = h;

  reallocate();
 
  xname = "X";
  yname = "Y";
  zname = "Data";
  
  // Perform conversion to floating point at very first step, even if
  // the data is integer: we will then requantize it by calling
  // quantize().

  // We may want to have a UI option to preserve orginal quantization:
  // turning off auto_quant should work... (although it will always be
  // preserved anyway in the raw_data, and so also in the line cuts)

  for (int i=0; i<width*height; i++)
    raw_data[i] = qmin + 1.0*data[i]*(qmax-qmin)/QUANT_MAX;
  
  store_orig();
}

void ImageData::load_raw(double *data, 
			 int w, int h,
			 double x1, double x2, 
			 double y1, double y2)
{
  xmin = isnan(x1) ? x1 : 0;
  xmax = isnan(x2) ? x2 : width-1;
  ymin = isnan(y1) ? y1 : 0;
  ymax = isnan(y2) ? y2 : height-1;
  width = w;
  height = h;

  reallocate();
  
  xname = "X";
  yname = "Y";
  zname = "Data";

  memcpy(raw_data, data, width*height*sizeof(double));
  store_orig();
}

int ImageData::load_file(const char *name)
{
  // We will identify the relevant helper function by the filename
  // extension

  const char *ext = strrchr(name, '.');

  data_loaded = 0;
  if (ext == NULL)
    badfile("Unsupported extension on file: %s\n", name);
      
  if (strcmp(ext, ".Stm") == 0)
    return load_STM(name);
  else if (strcmp(ext, ".pgm") == 0)
    return load_PGM(name);
  else if (strcmp(ext, ".mtx") == 0)
    return load_MTX(name);
  else if (strcmp(ext, ".dat") == 0)
    {
      if (datfile_type == MATRIX)
	return load_DAT(name);
      else if (datfile_type == GNUPLOT)
	return load_GP(name);
      else if (datfile_type == DELFT_LEGACY)
	return load_Delft(name);
      else if (datfile_type == DAT_META)
	return load_DAT_meta(name);
    }
  else if (strcasecmp(ext, ".TIF") == 0)
    return load_XL30S_TIF(name);
  else 
    {
      if (fallbackfile_type == PGM)
	return load_PGM(name);
      else if (fallbackfile_type == MTX)
	return load_MTX(name);
      else //if (fallbackfile_type == DAT)
	{
	  if (datfile_type == MATRIX)
	    return load_DAT(name);
	  else if (datfile_type == GNUPLOT)
	    return load_GP(name);
	  else if (datfile_type == DELFT_LEGACY)
	    return load_Delft(name);
	  else if (datfile_type == DAT_META)
	    return load_DAT_meta(name);
	}
    }
}

int ImageData::load_STM(const char *name)
{
  // variables for binary read
  int m = 0;
  unsigned char tmp[2];
  short unsigned int datatmp;

  // variables for header info
  int w, h;
  double xvrange, yvrange, xcal, ycal;    
  int chnum;
  char buf1[256], buf2[256];
  char ch_name[256];
  char ch_units[256];
  double ch_scale;

  // Some Stm headers (older ones, v2.5.1 software, at least), have 0x800 byte headers.
  // Version 2.6.0 also have 0x800 byte headers.
  // The newer ones, v 2.6.3, have 0x1000 byte headers.
  // Any space not used by the header info is filled with 0x2e up until the last byte
  // before the image data starts, which is always 0x00.

  char header[0x1000];
  int hdrlen;

  FILE *fp = fopen(name, "rb");
  if(fp == NULL)
    badfile("Unable to open file \"%s\": %s\n",name,strerror(errno));

  // Read the second line of the file, which has the software version
  int s1, s2, s3;
  if (fgets(buf1, 256, fp) == NULL)
    badfilec("Short file error\n");
  if (fgets(buf1, 256, fp) == NULL)
    badfilec("Short file error\n");
  if (sscanf(buf1, "SwV %d.%d.%d", &s1, &s2, &s3) != 3)
      hdrlen = 0x800;
  else 
    {
      if ((s2 > 5) && (s3 > 2))
	hdrlen = 0x1000;
      else
	hdrlen = 0x800;
    }
  
  // Rewind and now read the correct header length
  rewind(fp);
  assert(hdrlen > 0);
  if (fread(header, 1, hdrlen, fp) < static_cast<unsigned>(hdrlen))
    badfilec("Invalid STM file");
	
  if (sscanf(strstr(header, "Pix"), "Pix %d", &w) != 1)
    badfilec("Invalid width\n");

  if (sscanf(strstr(header, "Lin"), "Lin %d", &h) != 1)
    badfilec("Invalid height\n");

  if (sscanf(strstr(header, "SR0"), "SR0 %lf", &xvrange) != 1)  // range in volts
    badfilec("Invalid xrange\n");

  if (sscanf(strstr(header, "SR1"), "SR1 %lf", &yvrange) != 1)
    badfilec("Invalid yrange\n");
  
  if (sscanf(strstr(header, "S00"), "S00 %lf", &xcal) != 1) // scanner calibration in Ang/V
    badfilec("Invalid xscale\n");

  if (sscanf(strstr(header, "S10"), "S10 %lf", &ycal) != 1)
    badfilec("Invalid xscale\n");

  if (sscanf(strstr(header, "ImC"), "ImC %d", &chnum) != 1)
    badfilec("Invalid chnum\n");

  sprintf(buf1, "A%dN", chnum);
  sprintf(buf2, "A%dN %%256\[^\n]", chnum);
  if (sscanf(strstr(header, buf1), buf2, ch_name) != 1)
    badfilec("Invalid channel name\n");

  sprintf(buf1, "A%dU", chnum);
  sprintf(buf2, "A%dU %%256[^\n]", chnum);
  if (sscanf(strstr(header, buf1), buf2, ch_units) != 1)
    badfilec("Invalid channel units\n");

  sprintf(buf1, "A%dV", chnum);
  sprintf(buf2, "A%dV %%lf\n", chnum);
  if (sscanf(strstr(header, buf1), buf2, &ch_scale) != 1)
    badfilec("Invalid channel scale\n");


  // Now update the ranges and stuff
  
  xmin = 0;
  xmax = xvrange * xcal / 10000; // in microns
  xname = "Microns";

  ymin = 0;
  ymax = yvrange * ycal / 10000; // in microns
  yname = "Microns";
  
  // Let's leave the STM file Z scale just as 0 to 65535
  // (It's usually just the +/- 10V anyway, which is not any more physical...)

  zname = "DAC Value";

  width = w; height = h;
  
  reallocate();

  for (int i=0; i<width*height; i++)
    {
      m=fread(&tmp[1], 1, 1, fp); // byteswap from the SGI
      m=fread(&tmp[0], 1, 1, fp);
      if (m == 0)
	badfilec( "Read error row %d!\n", i/w);
      memcpy(&datatmp, &tmp, 2); 
      datatmp = datatmp + 32768; // overflow the short unsigned int to convert from 16-bit signed to 16-bit unsigned
      raw_data[i] = datatmp;

      if (raw_data[i] > 65535 || raw_data[i] < 0)
	{
	  warn("%4d %4d Data %02x %02x %.1f %6d\n", i/w, i%w, tmp[0], tmp[1], raw_data[i], datatmp);
	  //getchar();
	}
    }
  fclose(fp);
  store_orig();
  data3d = false;
  return 0;
}


int ImageData::load_PGM(const char *name) 
{ 
  char buf[1024];
  char *p;

  char xunit[256];
  char yunit[256];
  char zunit[256];

  int maxval;

  //FILE *fp = fopen(name, "rb");
  // This is very annoying! pgm_readpgm exits on error...
  // I will have to write a more fault tolerant routine myself.
  //gray **image;
  //gray maxval;
  //image = pgm_readpgm(fp, &width, &height, &maxval);
  //reallocate();
  //fclose(fp);

  FILE *fp = fopen(name, "rb");
  if(fp == NULL)
    badfile("Unable to open file \"%s\": %s\n",name,strerror(errno));

  qmin = 0;
  qmax = 65535;
  xmin = 0;
  xmax = width-1;
  ymin = 0;
  ymax = height-1;

  sprintf(xunit, "Pixels");
  sprintf(yunit, "Pixels");
  sprintf(zunit, "Gray Value");

  // Get the first two lines: I always put comments below the image size 
  if (fgets(buf,sizeof(buf),fp) == NULL)
    badfilec("no characters read: empty or short file?");
  if (strncmp(buf, "P5", 2) != 0)
    badfilec("wrong magic number %s: only raw pgm files are supported", buf);
  if (fgets(buf,sizeof(buf),fp) == NULL)
    badfilec("no characters read: empty or short file?");
  while (buf[0] == '#') // ignore any non-spyview comments
    fgets(buf,sizeof(buf),fp);
  if (sscanf(buf, "%d %d", &width, &height) != 2)
    badfilec("error reading width and height from line %s", buf);

  // Allocate the arrays now that we have the width and height
  reallocate();
  
  // Now get any lines that contain comments
  while (true)
    {
      fgets(buf, 1024, fp);
      if (buf[0] != '#')
	break;
      p = &buf[1];
      while (*p == ' ') p++;
      if (parse_pgm_comments("zmin", "%lf", &qmin, p, name)) continue;
      if (parse_pgm_comments("zmax", "%lf", &qmax, p, name)) continue;
      if (parse_pgm_comments("xmin", "%lf", &xmin, p, name)) continue;
      if (parse_pgm_comments("xmax", "%lf", &xmax, p, name)) continue;
      if (parse_pgm_comments("ymin", "%lf", &ymin, p, name)) continue;
      if (parse_pgm_comments("ymax", "%lf", &ymax, p, name)) continue;
      if (parse_pgm_comments("xunit", "%s", xunit, p, name)) continue;
      if (parse_pgm_comments("yunit", "%s", yunit, p, name)) continue;
      if (parse_pgm_comments("zunit", "%s", zunit, p, name)) continue;
    }
  xname = xunit;
  yname = yunit;
  zname = zunit;

  // The next item is the maxval, which should already be in the
  // buffer: we need to know if the data is one byte or two.
  if (sscanf(buf, "%d", &maxval) != 1) 
    badfilec("error reading maxval");
  
  unsigned char b1;
  unsigned char b2;
  int data;
  // Now lets read the data
  for (int i=0; i<width*height; i++)
    {
      if (fread(&b1, 1, 1, fp) != 1)
	badfilec("short file at pixel %d", i);
      if (maxval > 255)
	{
	  if (fread(&b2, 1, 1, fp) != 1)
	    badfilec("short file at pixel %d", i);
	  data = b1*256+b2;
	}
      else 
	data = b1;
      raw_data[i] =  qmin + 1.0*data*(qmax-qmin)/QUANT_MAX;
    }

  store_orig();
  fclose(fp);
  data3d = false;
  return 0;
}

int ImageData::parse_pgm_comments(const char *ident, const char *fmt, void *val, char *p, const char *filename)
{
  if (strncmp(p, ident, 3) == 0)
    {
      p = strchr(p, ' ');
      if (strcmp(fmt, "%s") == 0) 
	{ 
	  char *cval = (char *)val;
	  while (*p == ' ') p++;
	  strcpy(cval, p); 
	  for(int i = strlen(cval)-1; i > 0 && isspace(cval[i]); i--)
	    cval[i] = 0; // get rid of blank spaces at the end
	  return 1; 
	}
      if (sscanf(p, fmt, val) != 1)
	badfile( "Invalid %s %s in file %s\n", ident, p, filename);
      return(1);
    }
  return(0);
}
  
int ImageData::load_MTX(const char *name)
{
  if (mtx.load_file(name) == -1) return -1;
  load_mtx_cut();
  data3d = true;
  return 0;
}

int ImageData::load_GP(const char *name)
{
  if (gpload_type == COLUMNS)
    {
      if (mtx.load_gp_cols(name) == -1) return -1;
      // Good default settings for gp_cols
      mtx_index = gp_column;
      mtx_cut_type = 0;
      do_mtx_cut_title = false;
    }
  if (gpload_type == INDEX)
    {
      if (mtx.load_gp_index(name, gp_column) == -1) return -1;
      //mtx_index = 0;
      //mtx_cut_type = 2;
      do_mtx_cut_title = true;
    }
  
  load_mtx_cut();
  data3d = true;
  return 0;
}

int ImageData::load_DAT_meta(const char *name)
{
  int n = mtx.load_dat_meta(name, gp_column);
  if (n == -1)
    {
      warn("Error reading metadata file\n");
      return -1;
    }
  else if (n == -2)
    {
      warn("Error reading data file\n");
      return -1;
    }
  // Why would I set these?
  //mtx_index = 0;
  //mtx_cut_type = 2;  // Range checking should be done in load_mtx_cut(), which it is.
  load_mtx_cut();
  data3d = true;
  return 0;
}
    

void ImageData::load_mtx_cut()
{
  if (!mtx.data_loaded)
    error("load_mtx_cut called with no mtx data loaded!");

  mtx_cut_type = static_cast<mtxcut_t>(mtx_cut_type % 3); //in case the user set it to an integer instead of using the enums

  //info("loading mtx cut type %d (YZ = 0, XY = 1, XZ = 2)\n", mtx_cut_type);

  if (mtx_index<0) mtx_index = 0;
  if (mtx_index > mtx.size[mtx_cut_type]-1) mtx_index = mtx.size[mtx_cut_type]-1;
  
  int xaxis = (mtx_cut_type+1)%3;
  int yaxis = (mtx_cut_type+2)%3;
  int zaxis = (mtx_cut_type)%3;

  width = mtx.size[xaxis];
  height = mtx.size[yaxis];

  //info("xaxis is %d width %d\n", xaxis, width);
  //info("yaxis is %d height %d\n", yaxis, height);

  reallocate();

  //warn( "mtx_cut_type = %d, xaxis = %d, yaxis = %d\n", mtx_cut_type, xaxis, yaxis);
  //warn( "width %d height %d\n", width, height);
  //warn( "loading index %d type %d\n", mtx_index, mtx_cut_type);

  for (int i=0; i<width; i++)
    for (int j=0; j<height; j++)
      {
	if (mtx_cut_type == 0)
	  raw(i,j) = mtx.getData(mtx_index, i, j);
	else if (mtx_cut_type == 1)
	  raw(i,j) = mtx.getData(j, mtx_index, i);
	else 
	  raw(i,j) = mtx.getData(i, j, mtx_index);
      }

  xname = mtx.axisname[xaxis];
  yname = mtx.axisname[yaxis];
  
  if (do_mtx_cut_title)
    {
      char buf[256];
      snprintf(buf,256,"%g", mtx.get_coordinate(zaxis,mtx_index));
      zname = mtx.dataname + " at " + mtx.axisname[zaxis] + " = " + buf;
    }
  else
    zname = mtx.dataname;
  
  // This is the proper way to do it:

  xmin = mtx.get_coordinate(xaxis, 0);
  xmax = mtx.get_coordinate(xaxis, width-1);
  
  // Y is always flipped
  ymin = mtx.get_coordinate(yaxis, height-1);
  ymax = mtx.get_coordinate(yaxis, 0);
  
  store_orig();
}

int ImageData::load_Delft(const char *name)
{
  vector<double> data;
  char linebuffer[LINESIZE];
  FILE *fp = fopen(name, "rb");
  if(fp == NULL)
    badfile("Unable to open file \"%s\": %s\n",name,strerror(errno));
  int nread = 0;
  int num_points = 0;
  int points_per_sweep = -1;
  int num_sweeps = -1;
  double sweep_min = NAN;
  double sweep_max = NAN;
  bool found_sweep_max = false;
  double last_sweep;
  double last_data;
  string first_sweep_header;
  string last_sweep_header;
  string second_sweep_header;

  // This file format is a pain in the ass.  In particular since there
  // are so many variations of it. I find the whole thing unbelievably
  // retarded.
  //
  // The basic structure something like this:
  //
  // 0 XL GATE VOLTAGE (mV)
  // 0 YL CURRENT (pA),  Y2L CURRENT (pA)
  // 0 T T.U. Delft, TCS & Magnet PMS & dewar gnd disconnected, still heater from battery ,6-10-2008, 13:12
  // -1 L B=0.000T T=19.9mK sw=DAC6 D1=5.00 D2=0.00 D3=-1000.00 D4=0.00 D5=0.00 D6=-1000.00 D7=0.00 D8=0.00
  // [sweep data...]
  // 0 XL GATE VOLTAGE (mV)
  // 0 YL CURRENT (pA),  Y2L CURRENT (pA)
  // 0 T T.U. Delft, TCS & Magnet PMS & dewar gnd disconnected, still heater from battery ,6-10-2008, 13:14
  // -2 L B=0.000T T=19.9mK sw=DAC6 D1=5.00 D2=0.00 D3=-998.00 D4=0.00 D5=0.00 D6=600.00 D7=0.00 D8=0.00
  // [sweep data...]
  //
  // Parsing is as follows:
  //   
  //   - look for "0 XL" to indicate new sweep
  //   - sweep value is from col 2
  //   - data value is from following columns

  if (mtx.progress_gui)
    mtx.open_progress_gui();

  while (1)
    {
      nread++;
      if (fgets(linebuffer, LINESIZE, fp) == NULL)
	break;

      if (nread % 100 == 0) 
	{
	  static char buf[256];
	  snprintf(buf, sizeof(buf), "Lines read: %d", nread);
	  if (mtx.progress_gui)
	    {
	      mtx.msg->value(buf);
	      Fl::check();
	    }
	}

      if (strstr(linebuffer, "0 XL") != NULL)
	{
	  //info("found sweep, line %d", nread);getchar();
	  if (num_sweeps == -1) // this is the XL line at top of file
	    {
	      if (fgets(linebuffer, LINESIZE, fp) == NULL) break;
	      if (fgets(linebuffer, LINESIZE, fp) == NULL) break;
	      if (fgets(linebuffer, LINESIZE, fp) == NULL) break;
	      nread += 3;
	      first_sweep_header = linebuffer;
	      num_sweeps = 0;
	      continue;
	    }
	  else
	    {
	      // if (isnan(sweep_max)) doesn't seem to work under win32?
	      if (!found_sweep_max)
		{
		  sweep_max = last_sweep;
		  found_sweep_max = true;
		  info("settings sweep max to %e from line %d\n", sweep_max, nread+4-1);
		}
	      num_sweeps++;
	      if (points_per_sweep == -1)
		points_per_sweep = num_points;
	      else if (num_points != points_per_sweep)
		badfilec("%d points in sweep %d doesn't match previous value %d\nline number %d\n", 
			 num_points, num_sweeps, points_per_sweep, nread);
	      num_points = 0;
	      
	      if (fgets(linebuffer, LINESIZE, fp) == NULL) break;
	      if (fgets(linebuffer, LINESIZE, fp) == NULL) break;
	      if (fgets(linebuffer, LINESIZE, fp) == NULL) break;
	      nread+=3;
	      last_sweep_header = linebuffer;

	      if (num_sweeps == 1)
		second_sweep_header = last_sweep_header;

	      continue;
	    }
	  
	}
      
      if (strchr("#\n\r",linebuffer[0]) != NULL)
	continue;

      //if (sscanf(linebuffer, "%*f\t%lf\t%lf", &last_sweep, &last_data) != 2)
      last_data = parse_reading(linebuffer, gp_column);
      if (isnan(last_data)) 
	{
	  //info("line\n%s\ncgp_column %d\nval %e\n", linebuffer, gp_column, last_data);
	  badfilec("invalid data in delft file at line %d\nline: %s\n", nread, linebuffer);
	}

      data.push_back(last_data);
      if (num_points == 0) sweep_min = last_sweep;
      num_points++;
    }
  
  if (mtx.progress_gui)
    mtx.close_progress_gui();

  num_sweeps++;
  if (num_points != points_per_sweep)
    {
      info("Incomplete last sweep: \n"
	   "%d points in sweep %d doesn't match previous value %d\n", 
	   num_points, num_sweeps, points_per_sweep);
      num_sweeps--;
    }
      

  info("points_per_sweep %d num_sweeps %d\n", points_per_sweep, num_sweeps);

  unsigned int i,j;
  i = first_sweep_header.find("sw=", 0);
  j = first_sweep_header.find("D1", i);

  string sweepname = first_sweep_header.substr(i+3,j-i-4);
  
  info("i %d j %d sweepname set to _%s_", i, j, sweepname.c_str());
  
  info("\n\n%s%s%s\n", 
       first_sweep_header.c_str(), 
       second_sweep_header.c_str(), 
       last_sweep_header.c_str());

  height = points_per_sweep;
  width = num_sweeps;
  reallocate();

  info("data " _STF " w*h %d\n", data.size(), width*height);
  
  //for (int i=0; i<width*height; i++)
  //raw_data[i] = data[i];

  for (int i=0; i<width; i++)
    for (int j=0; j<height; j++)
      raw(i,j) = data[i*height+(height-1-j)];
  
  data3d = false;
  ymin = sweep_min;
  ymax = sweep_max;
  xmin = 0; 
  xmax = num_sweeps;
  xname = "Loop Variable";
  yname = sweepname;
  zname = "Current (pA)";
  store_orig();

  fclose(fp);
  return 0;
}  
  

// copied from fei2pgm test code

int fei_error(char *msg, const char *name)
{
  warn(msg, name);
  return -1;
}

int ImageData::load_XL30S_TIF(const char *name)
{
  char buf[0x1200];
  double mag;
  char *p;

  int size = 712*484; // we support only low res tifs for now

  // First get the header

  FILE *fp = fopen(name, "r");

  fread(buf, 1, 0x1200, fp);

  // Read the magnification

  for (int i=0; i<0x1200; i++)
    if ( (p = strstr(buf+i, "Magni")) != 0)
      break;
  if (p == NULL) 
    return fei_error("error: could not find magnification tag in XL30S TIF file %s", name);

  if (sscanf(p, "Magnification = %lf", &mag) == 0)
    return fei_error("error: could not read magnification in XL30S TIF file %s", name);

  info("XL30S TIF Mag is %g\n", mag);

  // Check for XLFileType tag to see if this is really an XL30s TIF
  p = NULL;
  for (int i=0; i<0x1200; i++)
    if ( (p = strstr(buf+i, "XLFileType")) != 0)
      break;
  if (p == NULL)
    return fei_error("file %s does not seem to be and XL30s TIFF", name);

  // Try to seek to start of the data (always last chunk of file?)

  if (fseek(fp, -size, SEEK_END) == -1)
    return fei_error("Error seeking in file: %s", name);
  
  xname = "Microns";
  yname = "Microns";
  zname = "SEM Signal";
  xmin = 0; ymin = 0;
  xmax = 712.0/215.0/mag*29736.822;
  ymax = 484.0/215.0/mag*29736.822;
  width = 712;
  height = 484;
  reallocate();
  
  unsigned char tmp[size];
  fread(tmp, 1, size, fp);
  fclose(fp);
  
  for (int i=0; i<size; i++)
    raw_data[i] = tmp[i];

  store_orig();
  data3d = false; 
  return 0;
}
  
// Copied from dat2pgm
int ImageData::load_DAT(const char *name)
{
  int w, col;
  int h, row;
  
  vector<double> data;

  char linebuffer[LINESIZE];
  char *p;

  double datatmp;

  char sep[] = " \t\n\r";

  FILE *fp = fopen(name, "rb");
  if(fp == NULL)
    badfile("Unable to open file \"%s\": %s\n",name,strerror(errno));

  row = col = w = h = 0;
  
  while (1)
    {
      if (fgets(linebuffer, LINESIZE, fp) == NULL)
	break;
   
      // Figure out what kind of line this is
      for(p = linebuffer; *p != 0 && isspace(*p); p++)
	;

      if(*p == '\n' || *p == 0 || *p == '#')
	continue;

      col = 0;
      p = strtok(linebuffer, sep);
      while (p != NULL)
	{
	  if (sscanf(p, "%lf", &datatmp) != 1)
	    warn( "load_DAT: invalid data at row %d col %d: \"%s\", copying last read value\n", row, col, p);
	  data.push_back(datatmp);
	  col++;
	  p = strtok(0, sep);
	}

      if (row == 0)
	w = col;
      else if (w != col)
	{
	  if (col == 0)
	    break;      // ignore empty lines at the end.
	  if(col < w) // This row is too short
	    {
	      warn( "number of columns %d at row %d does not match width of previous rows (%d)\n", col, row, w);
	      double d = 0; // This is used for ZERO; Mean updates the value so they can share insertion code.	 
	      switch(incorrect_column)
		{
		case EXIT:	      
		  exit(-1);
		case COPY_ADJACENT:
		  d = data[data.size()];
		  for (int i=0; i < (w-col); i++)
		    data.push_back(d);
		  break;
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
		  badfilec( "Invalid setting for incorrect_column width: %c\n",incorrect_column);
		}
	    }
	  else // This row is too long
	    {
	      switch(incorrect_column)
		{
		case EXIT:	      
		  badfilec( "number of columns %d at row %d does not match width of previous rows (%d)\n", col, row, w);
		case COPY_ADJACENT:
		case FILL_ZERO: // Warning! Falling case!
		case FILL_MEAN:
		  warn( "number of columns %d at row %d does not match; fill_mean and fill_zero no ready yet for this case.\n",col,row);
		  warn( "dropping column.");
		  while(col > w)
		    {
		      data.pop_back();
		      col--;
		    }
		  break;
		case DROP:
		  data.erase(data.begin(),data.begin()+(row*w));
		  row = 0;
		  w = col;
		  break;
		default:
		  badfilec( "Invalid argument for -i: %c\n",incorrect_column);
		}
	      
	    }
	  
	}
      row++;
      assert((int)data.size() == row * w);
    }
  h = row;

  fclose(fp);

  width = w; 
  height = h;
  reallocate();

  for (int i=0; i<width*height; i++)
    raw_data[i] = data[i];

  data3d = false;
  store_orig();
  return 0;
}

void ImageData::find_raw_limits()
{
  rawmin = INFINITY; 
  rawmax = -INFINITY;
  
  for (int i=0; i<width*height; i++)
    {
      if (raw_data[i] < rawmin) rawmin = raw_data[i];
      if (raw_data[i] > rawmax) rawmax = raw_data[i];
    }
}

void ImageData::quantize()
{
  if (auto_quant)
    {
      find_raw_limits();
      qmin = (rawmin+rawmax)/2 - (rawmax-rawmin)/2/auto_quant_percent*100;
      qmax = (rawmin+rawmax)/2 + (rawmax-rawmin)/2/auto_quant_percent*100;
    }

  for (int i=0; i<width*height; i++)
    quant_data[i] = raw_to_quant(raw_data[i]);
}

void ImageData::log10(bool do_offset, double new_offset)
{
  // Refresh the rawmin and rawmax variables
  if (do_offset) find_raw_limits();
  
  if (do_offset)
    for (int i=0; i<width*height; i++)
      raw_data[i] = log(raw_data[i]-rawmin+new_offset)/log(10.0);
  else
    for (int i=0; i<width*height; i++)
      raw_data[i] = log(raw_data[i])/log(10.0);
}

void ImageData::magnitude()
{
  for (int i=0; i<width*height; i++)
    raw_data[i] = fabs(raw_data[i]);
}
 
void ImageData::neg()
{
  for (int i=0; i<width*height; i++)
    raw_data[i] = -raw_data[i];
}
 
void ImageData::offset(double offset, bool do_auto)
{
  if (do_auto) find_raw_limits();

  for (int i=0; i<width*height; i++)
    raw_data[i] = raw_data[i]+offset-(do_auto ? rawmin : 0);

}

void ImageData::scale(double factor)
{ 
  for (int i=0; i<width*height; i++)
    raw_data[i] = raw_data[i]*factor;
}

void ImageData::power2(double x)
{
    for (int i=0; i<width*height; i++)
      raw_data[i] = pow(x,raw_data[i]);
}

void ImageData::gamma(double gamma, double epsilon)
{ 
  // This is tricky: what to do if we have a negative power and we get zero?
  // To handle this nicely, we should add an epsilon.
  double v1,v2;
  for (int i=0; i<width*height; i++)
    {
      v1 = raw_data[i];
      // problems with 1/0
      if (fabs(v1) < fabs(epsilon) && gamma<0)
	{
	  if (v1<0) v1 = -fabs(epsilon);
	  else if (v1>0) v1 = fabs(epsilon);
	  else v1 = fabs(epsilon);
	}
      v2 = pow(v1, gamma);
      //if (!isfinite(v2))
      //info("v1 %e v2 %e eps %e\n", v1, v2, epsilon);
      if (isnan(v2))
	raw_data[i] = 0;
      else
	raw_data[i] = v2;
	
    }
}

// calculate a 2D histogram of the dataset
// For each x-value, tranform the y axis of the dataset into a histogram of the measured datavalues

void ImageData::hist2d(double dmin, double dmax, int num_bins)
{
  int new_height = num_bins;
  int histogram[new_height];  
  double tmp;
  int i,j;

  // never shrink the arrays (leads to segfaults somewhere...)
  double *new_data;
  if (width * new_height > orig_width*orig_height) 
    new_data = new double[width*new_height];
  else
    new_data = new double[orig_width*orig_height];

  for (i=0; i<width; i++)
    {
        for (j=0; j<new_height; j++)
	  histogram[j] = 0;
	for (j=0; j<height; j++)
	  {
	    tmp = (raw(i,j)-dmin)/(dmax-dmin)*new_height;
	    if (tmp >= 0 && tmp < new_height)
	      histogram[(int)tmp]++;
	  }
	for (j=0; j<new_height; j++)
	  new_data[j*width+i] = histogram[new_height-1-j];
    }
  ymin = dmin;
  ymax = dmax;

  delete [] raw_data;
  raw_data = new_data;
  resize_tmp_arrays(width, new_height);
  height = new_height;
}


// The perfect superconducting junction function!
// Convert a VI trace into an IV trace
// This is actually very similar to the hist2d function (which nobody really understood)
// The function will only work on data that is monotonically increasing or decreasing. 
// (obviously, because otherwise we will have multivalued problems...)
// The data should be arranged with the I_bias axis vertical 
// Let's also assume that the user has arranged it so that the measured voltage (and the 
// bias current) is increasing from bottom to top. If not, it will give nonsense.
// Later I may add some "intelligence" to try to figure out automatically which way things
// are pointing.

void ImageData::vi_to_iv(double vmin, double vmax, int num_bins)
{
  int new_height = num_bins;
  double I_bias[new_height];
  double last_I_bias;
  double last_v;

  int last_updated_new_j;
    
  int i,old_j,new_j;

  // never shrink the arrays (leads to segfaults somewhere...)
  double *new_data;
  if (width * new_height > orig_width*orig_height) 
    new_data = new double[width*new_height];
  else
    new_data = new double[orig_width*orig_height];

  for (i=0; i<width; i++) // loop over columns
    {
      // Annoying, but ymin is actually the top of the window
      old_j = height-1;
      last_I_bias = getY(old_j); // reset for each column 
      last_updated_new_j = new_height-1; // For keeping track for linear interpolation

      for (new_j=new_height-1; new_j>=0; new_j--)
	{
	  last_v = vmin + 1.0*(vmax-vmin)*(new_height-1-new_j)/(new_height-1);
	  if (last_v > raw(i,old_j))
	    {
	      // We got a new data point. Let's first loop through an
	      // do linear interpolation since the last new data
	      // point.
	      double tmp = last_I_bias;
	      if (old_j >0) old_j--;
	      last_I_bias = getY(old_j);
	      if ((last_updated_new_j != (new_j-1)) && (last_updated_new_j != (new_height-1)))
	       	{
	       	  //info("try interpolating: from new_j %d to %d Ibias %e to %e", last_updated_new_j, new_j, tmp, last_I_bias);
		  //getchar();
		  for (int new_j_interp = last_updated_new_j-1; new_j_interp > new_j; new_j_interp--)
		    {
	       	      new_data[new_j_interp*width+i] = tmp - 
	       		1.0*(new_j_interp-last_updated_new_j) *
	       		(tmp-last_I_bias) /
	       		(new_j - last_updated_new_j);
		      //info("%d %e", new_j_interp, new_data[new_j_interp*width+i]);
		      //getchar();
	       	    }
	       	}
	      last_updated_new_j = new_j;
	    }
	  //info("new_j %d old_j %d last_v %e raw(i,old_j) %e last_I_bias %e\n", new_j, old_j, last_v, raw(i,old_j), last_I_bias);
	  new_data[new_j*width+i] = last_I_bias;
	}
    }

  ymin = vmin;
  ymax = vmax;

  delete [] raw_data;
  raw_data = new_data;
  resize_tmp_arrays(width, new_height);
  height = new_height;
}

void ImageData::interpolate(int new_width, int new_height, int type)
{
  if (new_width == width && new_height == height)
    return;

  // Had some trouble with segfaults. It's safest to just only make
  // matrices bigger if needed (don't shrink ones).
  double *new_data;
  if (new_width * new_height > orig_width*orig_height) 
    new_data = new double[new_width*new_height];
  else
    new_data = new double[orig_width*orig_height];

  double x_step = (double)width/(double)new_width;
  double y_step = (double)height/(double)new_height;

  for (int j=0; j<new_height; j++)
    for (int i = 0; i<new_width; i++)
      new_data[j*new_width+i] = raw_interp(i*x_step,j*y_step);
  
  delete [] raw_data;
  raw_data = new_data;

  resize_tmp_arrays(new_width, new_height);

  width = new_width;
  height = new_height;
}

void ImageData::calculate_thresholds(int type, 
				     double low, double high, 
				     double bottom_limit, double top_limit)
{
  // Only data points that lie within {min,max} will be counted,
  // others will be rejected.
  
  double min, max;

  // type = 0 = image percentiles
  // type = 1 = line percentiles, with optional limits
  // type = 2 = column percentiles, with optional limits
  // type = 3 = manual value thresholding

  // Easy one: a single threshold for the whole image
  if (type == 0 || type == 3)
    {
      if (type == 0) 
	find_image_threasholds(low, high, min, max);
      else if (type == 3)
	{
	  min = low; max = high;
	  if (min > max) min = max;
	}
      for (int i=0; i<width*height; i++)
	{
	  if (raw_data[i] > min && raw_data[i] < max)
	    threshold_reject[i] = 0;
	  else 
	    threshold_reject[i] = 1;
	}
    }
  
  // The more difficult ones: thresholds based on percentages for a
  // given row or column

  else if (type == 1)
    {
      vector <double> line_data;
      for (int j=0; j<height; j++)
	{
	  line_data.clear();
	  for (int i=0; i<width; i++)
	    if (raw(i,j) > bottom_limit && raw(i,j) < top_limit)
	      line_data.push_back(raw(i,j));
	  find_threasholds(line_data, low, high, min, max);
	  for (int i=0; i<width; i++)
	    {
	      if(raw(i,j) > min && raw(i,j) < max)
		threshold_reject[j*width+i] = 0;
	      else
		threshold_reject[j*width+i] = 1;
	    }
	}
    }
  else //if (type == 2)
    {
      vector <double> col_data;
      for (int i=0; i<width; i++)
	{
	  col_data.clear();
	  for (int j=0; j<height; j++)
	    if (raw(i,j) > bottom_limit && raw(i,j) < top_limit)
	      col_data.push_back(raw(i,j));
	  find_threasholds(col_data, low, high, min, max);
	  for (int j=0; j<height; j++)
	    {
	      if(raw(i,j) > min && raw(i,j) < max)
		threshold_reject[j*width+i] = 0;
	      else
		threshold_reject[j*width+i] = 1;
	    }
	}
    }
}
  
// A handy simple way to calculate threasholds for things like line
// cuts. Note that you have to put the data first into an STL
// container. It is more expensive, though.

void ImageData::find_threasholds(vector <double> data, 
				double bottom_percent, double top_percent,
				double &low, double &high)
{
  if (bottom_percent < 0) bottom_percent = 0;
  if (bottom_percent > 100) bottom_percent = 100;

  if (top_percent < 0) top_percent = 0;
  if (top_percent > 100) top_percent = 100;

  sort(data.begin(), data.end());
  int low_index = (int) (data.size()*bottom_percent/100.0);
  int high_index = (int) (data.size()*(100-top_percent)/100.0-1);
  low = data[low_index];
  high = data[high_index];
}

// A function which calculates threasholds for the whole image using
// the same truncation method we use to calculate the histogram, which
// should be faster than putting the whole dataset in an stl container
// and sorting it.

void ImageData::find_image_threasholds(double bottom_percent, double top_percent,
				      double &low, double &high)
{
  int levels = 1000;
  int histogram[levels];

  double tmp;
  int i;
  int lowint, highint;
  int count;

  find_raw_limits();

  if (bottom_percent < 0) bottom_percent = 0;
  if (bottom_percent > 100) bottom_percent = 100;

  if (top_percent < 0) top_percent = 0;
  if (top_percent > 100) top_percent = 100;

  for (i=0; i<levels; i++)
    histogram[i] = 0;

  for (i=0; i<width*height; i++)
    {
      tmp = (raw_data[i]-rawmin)/(rawmax-rawmin)*levels;
      if (tmp < 0 || tmp > levels)
	{
	  info("tmp is %g \n", tmp);
	}
      histogram[(int)tmp]++;
    }

  count = 0;
  for (lowint=0; lowint<levels; lowint++)
    {
      count += histogram[lowint];
      if (count > bottom_percent/100.0*width*height) break;
    }
  low = rawmin + lowint*(rawmax-rawmin)/levels;

  count = 0;
  for (highint=levels-1; highint>=0; highint--)
    {
      count += histogram[highint];
      if (count > top_percent/100.0*width*height) break;
    }
  high = rawmin + highint*(rawmax-rawmin)/levels;

  info("image threashold: min %e max %e\n", low, high);
}

void ImageData::remove_lines(int start, int nlines)
{
  for (int y = start; y < height - nlines; y++)
    memcpy(raw_data+y*width, raw_data+(y+nlines)*width, sizeof(double)*width);
  double new_ymin = getY(height-nlines); 
  height = height-nlines;
  ymin = new_ymin;
}

void ImageData::lbl(double bp, double tp, 
		    bool whole_image_threashold, bool percentiles, 
		    double bottom_limit, double top_limit)
{
  double line_average;
  int navg;
  
  int type;

  if (percentiles & whole_image_threashold)
    type = 0;
  else if (percentiles)
    type = 1;
  else //if (!percentiles)
    type = 3;
  
  calculate_thresholds(type, bp, tp, bottom_limit, top_limit);
  double offset;
  
  for (int j=0; j<height; j++)
    {
      line_average = navg = 0;
      for (int i=0; i<width; i++)
	if (!threshold_reject[j*width+i])
	  {
	    line_average += raw(i,j);
	    navg++; 
	  }
      if (navg != 0)
	{
	  line_average /= navg;
	  offset = line_average;
	}
      for (int i=0; i<width; i++)
	raw(i,j) -= offset;
    }
}

void ImageData::cbc(double bp, double tp, 
		    bool whole_image_threashold, bool percentiles, 
		    double bottom_limit, double top_limit)
{
  double col_average;
  int navg;

  int type;

  if (percentiles & whole_image_threashold)
    type = 0;
  else if (percentiles)
    type = 2;
  else //if (!percentiles)
    type = 3;
  
  calculate_thresholds(type, bp, tp, bottom_limit, top_limit);

  for (int i=0; i<width; i++)
    {
      col_average = navg = 0;
      for (int j=0; j<height; j++)
	if (!threshold_reject[j*width+i])
	  {
	    col_average += raw(i,j);
	    navg++; 
	  }
      if (navg != 0) col_average /= navg;

      for (int j=0; j<height; j++)
	raw(i,j) -= col_average;
    }
}

void ImageData::outlier_line(bool horizontal, int pos)
{
  int cnt=0; // Number of lines averaged
  if(horizontal)
    {
      if(pos > height || pos < 0)
	return;
      for(int i = 0; i < width; i++)
	raw(i,pos)=0;
      if(pos-1 >= 0)
	{
	  for(int i = 0; i < width; i++)
	    raw(i,pos)=raw(i,pos)+raw(i,pos-1);
	  cnt++;
	}
      if(pos+1 < height)
	{
	  for(int i = 0; i < width; i++)
	    raw(i,pos)=raw(i,pos)+raw(i,pos+1);
	  cnt++;
	}
      if(cnt > 1)
	for(int i = 0; i < width; i++)
	  raw(i,pos) = raw(i,pos)/cnt;
    }
  else
    {
      if(pos > width || pos < 0)
	return;
      for(int i = 0; i < height; i++)
	raw(pos,i)=0;
      if(pos-1 >= 0)
	{
	  cnt++;
	  for(int i = 0; i < height; i++)
	    raw(pos,i)=raw(pos,i)+raw(pos-1,i);
	}
      if(pos+1 < width)
	{
	  cnt++;
	  for(int i = 0; i < height; i++)
	    raw(pos,i)=raw(pos,i)+raw(pos+1,i);
	}
      if(cnt > 1)
	for(int i = 0; i < height; i++)
	  raw(pos,i) = raw(pos,i)/cnt;
    }
}
void ImageData::sub_linecut(bool horizontal, int pos)
{
  if(horizontal)
    {
      if(pos >= height || pos < 0)
	return;
      for(int j = 0; j < height; j++)
	for(int i = 0; i < width; i++)
	  if (j != pos) raw(i,j) -= raw(i,pos);
      for(int i = 0; i < width; i++)
	raw(i,pos) = 0;
    }
  else
    {
      if(pos >= width || pos < 0)
	return;
      for(int i = 0; i < width; i++)
	for(int j = 0; j < height; j++)
	  if (i != pos) raw(i,j) -= raw(pos,j);
      for(int j = 0; j < height; j++)
	raw(pos, j) = 0;
    }
}

void ImageData::norm_lbl()
{
  double min, max;
  for (int j=0; j<height; j++)
    {
      min = INFINITY;
      max = -INFINITY;
      for (int i=0; i<width; i++)
	{
	  if (raw(i,j) < min) min = raw(i,j);
	  if (raw(i,j) > max) max = raw(i,j);
	}
      for (int i=0; i<width; i++)
	raw(i,j) = (min != max) ? (raw(i,j) - min)/(max-min) : 0;
    }
}

void ImageData::norm_cbc()
{
  double min, max;

  for (int i=0; i<width; i++)
    {
      min = INFINITY;
      max = -INFINITY;
      for (int j=0; j<height; j++)
	{
	  if (raw(i,j) < min) min = raw(i,j);
	  if (raw(i,j) > max) max = raw(i,j);
	}
      for (int j=0; j<height; j++)
	raw(i,j) = (min != max) ? (raw(i,j) - min)/(max-min) : 0;
    }
}

void ImageData::fitplane(double bp, double tp, bool percentiles)
{
  // Formula for the plane: Z = a*X + b*Y + c
  double a,b,c;
  // calculate the moments
  int N = 0;
  double Zavg = 0;
  double Xavg = 0;
  double Yavg = 0;
  double sXZ = 0;
  double sYZ = 0;
  double sXX = 0;
  double sYY = 0;
  double val;

  double low, high;
  // In principle, we should call calculate_thresholds to be
  // consistent, but this is faster...
  if (percentiles)
    find_image_threasholds(bp, tp, low, high);
  else
    {
      low = bp; 
      high = tp;
    }
  
  for (int x=0; x < width; x++)
    {
      for (int y=0; y < height; y++)
	{
	  val = raw(x,y);
	  if (val > low && val < high)
	    {
	      Zavg += (double) val;
	      Xavg += (double) (x-width/2);
	      Yavg += (double) (y-height/2);
	      sXZ += (double) val * (x-width/2);
	      sYZ += (double) val * (y-height/2);
	      sXX += (double) (x-width/2)*(x-width/2);
	      sYY += (double) (y-height/2)*(y-height/2);
	      N++;
	    }
	}
    } 

  Xavg /= N;
  Yavg /= N;
  Zavg /= N;

  a = (sXZ - N*Xavg*Zavg)/(sXX - N*Xavg*Xavg);
  b = (sYZ - N*Yavg*Zavg)/(sYY - N*Yavg*Yavg);
  c = Zavg - a*Xavg - b*Yavg;
  
  for (int x=0; x<width; x++)
    for (int y=0; y<height; y++)
      raw(x,y) -=  a * (x-width/2) + b*(y-height/2) + c;

  //warn( "a,b,c %e %e %e\n", a, b, c);
}

void ImageData::plane(double b, double a)
{
  for (int x=0; x<width; x++)
    for (int y=0; y<height; y++)
      raw(x,y) -=  a * (x-width/2) + b*(y-height/2);
}

void ImageData::flip_endpoints(bool flipx, bool flipy)
{
  double tmp;
  if (flipx)
    {
      tmp = xmin;
      xmin = xmax;
      xmax = tmp;
    }
  if (flipy)
    {
      tmp = ymin;
      ymin = ymax;
      ymax = tmp;
    }
}

void ImageData::xflip()
{
  double tmp[width];
  for(int y = 0; y < height; y++)
    {
      memcpy(tmp,raw_data+y*width,sizeof(double)*width);
      for(int x = 0; x < width; x++)
	raw(x,y) = tmp[width-1-x];
    }
  double tmp2 = xmin;
  xmin = xmax;
  xmax = tmp2;
}

void ImageData::yflip()
{
  double tmp[width];
  for (int y=0; y < height/2; y++)
    {
      memcpy(tmp,&raw(0,y),sizeof(double)*width);      
      memcpy(&raw(0,y),&raw(0,height-y-1),sizeof(double)*width);
      memcpy(&raw(0,height-y-1),tmp,sizeof(double)*width);
    }
  double tmp2 = ymin; 
  ymin = ymax;
  ymax = tmp2;
}

void ImageData::rotate_cw()
{
  double *tmp = new double [width*height]; // should not use the stack for large arrays: use new instead
  memcpy(tmp, raw_data, sizeof(double)*width*height);
  
  for (int i=0; i<width; i++)
    for (int j=0; j<height; j++)
      raw_data[i*height+(height-1-j)] = tmp[j*width+i];
  
  double w = width;
  width = height;
  height = w;
  
  string tmpname = xname;
  xname = yname; 
  yname = tmpname;

  // just trial and error...
  double d1 = ymin;
  double d2 = ymax;
  ymin = xmax;
  ymax = xmin;
  xmin = d1;
  xmax = d2;
  delete [] tmp;
}  

void ImageData::rotate_ccw()
{
  double *tmp = new double [width*height]; // should not use the stack for large arrays: use new instead
  memcpy(tmp, raw_data, sizeof(double)*width*height);
  
  for (int i=0; i<width; i++)
    for (int j=0; j<height; j++)
      raw_data[(width-1-i)*height+j] = tmp[j*width+i];

  
  double w = width;
  width = height;
  height = w;
  
  string tmpname = xname;
  xname = yname; 
  yname = tmpname;

  // just trial and error...
  double d1 = ymax;
  double d2 = ymin;
  ymin = xmin;
  ymax = xmax;
  xmin = d1;
  xmax = d2;
  delete [] tmp;
}

void ImageData::pixel_average(int nx, int ny)
{
  if (nx == 0 || ny == 0) return;

  int w = width / nx;
  int h = height / ny;
  double tmp;

  // This should work in-place, since we're only overwriting data we
  // don't need anymore

  // For in-place to work, we must have the inner loop iterating over
  // i, since this is the data order axis!

  for (int j=0; j<h; j++)
    for (int i=0; i<w; i++)
      {
	tmp = 0;
	for (int m=0; m<nx; m++)
	  for (int n=0; n<ny; n++)
	    tmp += raw(nx*i+m,ny*j+n)/nx/ny;   
	raw_data[j*w+i] = tmp;
      }
  
  width = w;
  height = h;
}

void ImageData::switch_finder(double threshold, int avgwin, bool vert) //note: vert not yet implemented: is there a better way than cut and paste?
{
  int i,j,m,n;
  double offset = 0;
  double avg1 = 0 ;
  double avg2 = 0 ;
  threshold = fabs(threshold);
  if (avgwin < 1) avgwin = 1;

  // In principle, this is a very easy piece of code to write, but the
  // it could be really, really amazingly useful!
  for (j=0; j<height; j++)
    {
      offset = 0;
      for (i=1; i<width; i++)
	{
	  // Add running offset to current data
	  raw(i,j) += offset;
	  // Calculate avg from last "avgwin" points
	  for (m=1,n=0,avg1=0; m<=avgwin; m++)
	    if (i-m >= 0) 
	      {
		avg1 += raw(i-m,j);
		n++;
	      }
	  if (n>0) avg1 = avg1/n;
	  // If our next point deviates from the average by enough, then we've got switch
	  if (fabs(raw(i,j)-avg1) > threshold)
	    {
	      // Calculate the average for the next "avgwin" points too, and ignore subsequent points that might be a switch
	      avg2 = offset; n = 1;
	      for (m=0; m<=avgwin; m++)
		if (i+m < width && fabs(raw(i+m,j)+offset-raw(i,j)) < threshold)
		  {
		    avg2 += raw(i+m,j) + offset;
		    n++;
		  }
	      avg2 = avg2/n;
	      avg2 = raw(i,j);
	      offset += avg1 - avg2;
	      raw(i,j) += avg1 - avg2; 
	    }
	}
    }
}


void ImageData::xderv()
{
  int w = width-1;
  int h = height;

  double xstep = (xmax - xmin)/w;

  for (int j=0; j<h; j++)
    for (int i=0; i<w; i++)
      raw_data[j*w+i] = (raw(i+1,j) - raw(i,j))/xstep;
  //raw_data[j*w+i] = (xmin<xmax) ? (raw(i+1,j) - raw(i,j)) : (raw(i,j) - raw(i+1,j));
  
  xmin = xmin+(xmax-xmin)/width/2;
  xmax = xmax-(xmax-xmin)/width/2;
  width = w;
  height = h;
}

void ImageData::crop(int left, int right, int lower, int upper)
{
  // Let's be a bit clever: negative numbers on lower or right should
  // be interpreted as counting from the left or bottom. Also
  // implement smarter bounds checking (overflow large numbers).

  if (left > width-1) left = left % (width);
  if (right > width-1) right = right % (width);
  if (upper > height-1) upper = upper % (height);
  if (lower > height-1) lower = lower % (height);
  
  if (left < 0) left = left + (-left/width+1)*width - 1;
  if (upper < 0) upper = upper + (-upper/height+1)*height - 1;
  if (right <= 0) right = right + (-right/width+1)*width;
  if (lower <= 0) lower = lower + (-lower/height+1)*height;

  // Now check that things make sense
  // Modified April 2010 -- the below crashed if right was at edge already.  
  if (lower < upper) 
    std::swap(upper,lower);
  if (right < left) 
    std::swap(left,right);

  // Finally, make sure that we aren't cropping to nothing.
  if(lower == upper)
    if(upper > 0)
      upper--;
    else
      lower++;
  if(left == right)
    if(left > 0)
      left--;
    else
      right++;

  int w = (right-left);
  int h = (lower-upper);

  for (int j=0; j<h; j++)
   for (int i=0; i<w; i++)
     raw_data[j*w+i] = raw(i+left, j+upper);
  
  double newxmin = getX(left);
  double newxmax = getX(right-1);
  
  double newymin = getY(lower-1);
  double newymax = getY(upper);
  
  xmin = newxmin; xmax = newxmax;
  ymin = newymin; ymax = newymax;

  width = w;
  height = h;
}

void ImageData::even_odd(bool even, bool fwd_rev)
{
  int h = height/2;
  if (!even) h -= height%2;

  int off = even ? 0 : 1;

  if (even || !fwd_rev)
    {
      for (int j=0; j<h; j++)
	memcpy(&raw(0,j), &raw(0,2*j+off), sizeof(double)*width);
    }
  else
    {
      for (int j=0; j<h; j++)
	for (int i=0; i<width; i++)
	  raw(i,j) = raw(width-1-i,2*j+off);
    }
 

  height = h;
}
  

void ImageData::yderv()
{
  int w = width;
  int h = height-1;

  // Another one of those negative signs...
  double ystep = -(ymax - ymin)/h;

  for (int j=0; j<h; j++)
    for (int i=0; i<w; i++)
      raw_data[j*w+i] = (raw(i,j+1) - raw(i,j))/ystep;
      //raw_data[j*w+i] = (ymin>ymax) ? (raw(i,j+1) - raw(i,j)) : (raw(i,j) - raw(i,j+1));
  
  ymin = ymin+(ymax-ymin)/height/2;
  ymax = ymax-(ymax-ymin)/height/2;
  width = w;
  height = h;
}

void ImageData::ederv(double pscale, double nscale)
{
  int w = width;
  int h = height-1;
  int h0 = -ymin * h / (ymax - ymin);
  printf("ymin is %g, ymax is %g, h is %d, h0 is %d\n",ymin,ymax,h,h0);
  for (int j=0; j<h; j++)
    for (int i=0; i<w; i++)
      {
	int sign=1;
	if(j < h0)
	  sign=-1;
	else if ( j == h0 || j+1 == h0) // Not clear what to do exactly at zero.
	  sign = 0;
	raw_data[j*w+i] = ((ymin>ymax) ? (raw(i,j+1) - raw(i,j)) : (raw(i,j) - raw(i,j+1))) * sign;
	if(raw_data[j*w+i] < 0)
	  raw_data[j*w+i] *= nscale;
	else
	  raw_data[j*w+i] *= pscale;
      }
  
  ymin = ymin+(ymax-ymin)/height/2;
  ymax = ymax-(ymax-ymin)/height/2;
  width = w;
  height = h;
}

void ImageData::grad_mag(double axis_bias)
{
  printf("%g bias\n",axis_bias);
  int w = width;
  int h = height;
  double *tmpx = (double *)malloc(sizeof(double) * width * height);

  memcpy(tmpx,raw_data,sizeof(double)*width*height);
  xderv();
  width = w;
  height = h;
  swap(tmpx,raw_data);
  yderv();
  width = w;
  height = h;
  double *result = (double *)malloc(sizeof(double) * (width-1) * (height-1));
  for(int x = 0; x < width-1; x++)
    for(int y = 0; y < height-1; y++)
      {
	double g1 = tmpx[x+(width-1)*y];
	double g2 = raw_data[x+width*y];
	result[x+(width-1)*y] = sqrt(g1*g1*(1.0-axis_bias)+g2*g2*axis_bias);
      }
  memcpy(raw_data,result,sizeof(double)*(width-1)*(height-1));
  free(result);
  free(tmpx);
  width = width-1;
  height = height-1;
}

void ImageData::equalize() // Hist. eq.. We work on the quantized data here for simplicity.
{
  quantize();
  int cumsum[QUANT_MAX+1];
  int mapping[QUANT_MAX+1];

  // Generate the cumulative sum.
  memset(cumsum, 0, sizeof(cumsum));
  int *p = quant_data;
  for(int i = 0; i < width*height; i++)
    {
      assert(*p <= QUANT_MAX && *p >= 0);
      cumsum[*p++]++;
    }
  for(int i = 1; i <= QUANT_MAX; i++)
    cumsum[i] += cumsum[i-1];

  // Find the minimum value.
  int minval = -1;
  for(int i = 0; i <= QUANT_MAX; i++)
    if(cumsum[i] != 0)
      {
	minval = cumsum[i];
	break;
      }
  assert(minval >= 0);
  
  // Generate the mapping; see http://en.wikipedia.org/wiki/Histogram_equalization
  for(int i = 0; i <= QUANT_MAX; i++)
    {
      mapping[i]=round((1.0*cumsum[i]-1.0*minval)*(QUANT_MAX*1.0)/(1.0*width*height-1.0*minval));
    }

  // Apply the mapping using an extra-clever linear interpolation on the floating point data!  Yay!
  for(int i = 0; i < width*height; i++)
    {
      double raw = raw_data[i];
      double v = 1.0*(raw-qmin)*QUANT_MAX/(qmax-qmin);
      assert(v >= 0 && floor(v) < QUANT_MAX);
      double dv = v-floor(v);
      assert(dv >= 0);
      assert(dv <= 1.0);
      double quant = mapping[(int)v]*(1.0-dv)+mapping[(int)(v+0.5)]*dv;
      raw_data[i] = quant_to_raw(quant);
    }
}

void ImageData::dderv(double theta) // theta in degrees!
{
  int w = width;
  int h = height;
  double *tmpx = (double *)malloc(sizeof(double) * width * height);

  memcpy(tmpx,raw_data,sizeof(double)*width*height);
  xderv();
  width = w;
  height = h;
  swap(tmpx,raw_data);
  yderv();
  width = w;
  height = h;
  double *result = (double *)malloc(sizeof(double) * (width-1) * (height-1));
  double t1=cos(theta*M_PI/180.0);
  double t2=sin(theta*M_PI/180.0);
  for(int x = 0; x < width-1; x++)
    for(int y = 0; y < height-1; y++)
      {
	double g1 = tmpx[x+(width-1)*y];
	double g2 = raw_data[x+width*y];
	result[x+(width-1)*y] = g1*t1+g2*t2;
      }
  memcpy(raw_data,result,sizeof(double)*(width-1)*(height-1));
  free(result);
  free(tmpx);
  width = width-1;
  height = height-1;
}


void ImageData::make_lowpass_kernel(double *data, double dev, int size, ImageData::lowpass_kernel_t type)
{
  double center = floor(size/2.0);
  double sum = 0.0;
  for(int i = 0; i < size; i++)
    {
      double dx = (static_cast<double>(i)-center);      
      dx /= dev;
      double y;
      switch(type)
	{
	case LOWPASS_GAUSS:
	  y = exp(-(dx*dx)/(2.0));
	  break;
	case LOWPASS_EXP:
	  y = exp(-fabs(dx)*sqrt(2.0)); 
	  break;
	case LOWPASS_LORENTZ:
	  // Lorentzian has no std. dev, so set FWHM = sigma
	  y = 1.0/(dx*dx+1.0); 
	  break;
	case LOWPASS_THERMAL: // Derivative of Fermi function; dev is temperature in pixels
	  y = exp(dx)/(dev*(1+exp(dx))*(1+exp(dx)));
	  break;
	default:
	  assert(0);
	}
      sum += y;
      data[i] = y;
    }
  sum = 1.0/sum;
  for(int i = 0; i < size; i++)
    data[i] *= sum;
}

// This executes a gaussian blur low pass filter
void ImageData::lowpass(double xsize, double ysize, ImageData::lowpass_kernel_t type, double mult)
{
  int kernel_size;
  double sum;

  if (xsize == 0 && ysize == 0)
    return;

  kernel_size = xsize*mult;
  if (kernel_size > 0 && kernel_size < mult) kernel_size = mult;
      
  if (kernel_size > 0)
    {
      // For really big images, I think it is better not to reallocate an entire 2D image array (?)
      double filtered[width]; 
      double kernel[kernel_size];
      make_lowpass_kernel(kernel, xsize, kernel_size,type);
      int kernel_offset = -kernel_size/2;

      for(int y = 0; y < height; y++)
	{
	  for(int x = 0; x < width; x++)
	    {
	      sum = 0.0;
	      for(int k = 0; k < kernel_size; k++)
		{
		  int nx = x + k + kernel_offset;
		  if(nx < 0) nx = 0;
		  if(nx >= width) nx = width-1;
		  sum += kernel[k]*raw(nx,y);
		}
	      filtered[x] = sum;
	    }
	  memcpy(&raw(0,y), filtered, sizeof(double)*width);
	}
    }

  kernel_size = ysize*mult;

  if (kernel_size > 0 && kernel_size < mult) kernel_size = mult;
  
  if (kernel_size > 0)
    {
      double filtered[height]; 
      double kernel[kernel_size];
      make_lowpass_kernel(kernel, ysize, kernel_size,type);
      int kernel_offset = -kernel_size/2;

      for(int x = 0; x < width; x++)
	{
	  for(int y = 0; y < height; y++)
	    {
	      sum = 0.0;
	      for(int k = 0; k < kernel_size; k++)
		{
		  int ny = y + k + kernel_offset;
		  if(ny < 0) ny = 0;
		  if(ny >= height) ny = height-1;
		  sum += kernel[k]*raw(x,ny);
		}
	      filtered[y] = sum;
	    }
	  for(int y = 0; y < height; y++)
	    raw(x,y) = filtered[y];  // can't use memcpy here
	}
    }
}

// This could be made much more memory efficent if need be; see lowpass.
void ImageData::highpass(double xsize, double ysize, double passthrough, ImageData::lowpass_kernel_t type, double mult)
{
  double *d2 = new double[width*height];
  assert(d2);
  memcpy(d2,raw_data,sizeof(double)*width*height);
  lowpass(xsize,ysize,type,mult);
  for(int x = 0; x < width; x++)
    for(int y = 0; y < height; y++)
      raw(x,y)=d2[y*width+x]-(1.0-passthrough)*raw(x,y);  
  delete[] d2;
}

void ImageData::notch(double xlow, double xhigh, double ylow, double yhigh, double mult) // width is width of mask to use measured in units of xsize, ysize
{
  lowpass(xlow,ylow,LOWPASS_GAUSS,mult);
  highpass(xhigh,yhigh,0.0,LOWPASS_GAUSS,mult);
}

// Despeckle an image with a median filter
// Helper functions
inline static int median_3(double a, double b, double c)
{
  if(((a <= b) && (b <= c)) || ((c <= b) && (b <= a)))
    return b;
  if(((b <= c) && (c <= a)) || ((a <= c) && (c <= b)))
    return c;
  return a;
}
static int compare_doubles(const void *ap, const void *bp)
{
  double a = *reinterpret_cast<const double *>(ap);
  double b = *reinterpret_cast<const double *>(bp);
  if(a < b)
    return -1;
  if(a == b)
    return 0;
  return 1;
}
inline static int median_3x3(double *r1, double *r2, double *r3)
{
  double tmp[9];
  memcpy(tmp, r1, sizeof(double)*3);
  memcpy(tmp+3, r2, sizeof(double)*3);
  memcpy(tmp+6, r3, sizeof(double)*3);
  qsort(tmp, 9, sizeof(double), compare_doubles);
  return tmp[4];
}
// Main functions
void ImageData::despeckle(bool d_x, bool d_y)
{
  if(d_x && d_y)
    {
      double *tmp = new double[width * height];
      memcpy(tmp,raw_data,sizeof(double)*width*height);
      for(int x = 1; x < width-1; x++)
	for(int y = 1; y < height-1; y++)
	  raw(x,y) = median_3x3(tmp+(y-1)*width+x-1,tmp+y*width+x-1, tmp+(y+1)*width+x-1);
      delete[] tmp;
    }
  else if(d_x)
    {
      double tmp[width];
      for(int y = 0; y < height; y++)
	{
	  for(int x = 1; x < (width-1); x++)
	    tmp[x] = median_3(raw(x-1,y),raw(x,y),raw(x+1,y));
	  for(int x = 1; x < (width-1); x++)
	    raw(x,y) = tmp[x];
	}
    }
  else if(d_y)
    {
      double tmp[height];
      for(int x = 0; x < width; x++)
	{
	  for(int y = 1; y < (height-1); y++)
	    tmp[y] = median_3(raw(x,y-1),raw(x,y),raw(x,y+1));
	  for(int y = 1; y < (height-1); y++)
	    raw(x,y) = tmp[y];
	}
    }   
}


MTX_Data::MTX_Data()
{
  size[0] = size[1] = size[2] = 0;
  data_loaded = 0;
  progress_gui = true;
  delft_raw_units = true;
  delft_settings = false;
  win = NULL;
}

MTX_Data::~MTX_Data()
{
  clear();
}

void MTX_Data::open_progress_gui()
{
  if (win == NULL)
    {
      win = new Fl_Double_Window(220,25, "Loading file...");
      win->begin();
      msg = new Fl_Output(0,0,220,25);
      msg->color(FL_BACKGROUND_COLOR);
      win->end();
    }
  win->show();
}

void MTX_Data::close_progress_gui()
{
  win->hide();
}


int MTX_Data::load_file(const char *name)
{
  FILE *fp = fopen(name, "rb");
  if(fp == NULL)
    badfile("Unable to open file \"%s\": %s\n",name,strerror(errno));
  filename = name;

  char buf[MTX_HEADER_SIZE];
  int i,j,k;
  fgets(buf, sizeof(buf), fp);
  int bytes = 8;

  char units_header[MTX_HEADER_SIZE]; // 256 characters is not long enough...
  int found_units = 0;

  // First read the header information, which include the axis ranges and names

  if (strncmp(buf, "Units", 5) == 0) 
    {
      found_units = 1;
      strncpy(units_header, buf, sizeof(buf));
      fgets(buf, sizeof(buf), fp); // fixme ; check for errors here.
    }  
  if (sscanf(buf, "%d %d %d", &size[0], &size[1], &size[2]) != 3)
    badfilec("Malformed mtx header: %s", filename.c_str());
  if (sscanf(buf, "%*d %*d %*d %d", &bytes) != 1)
    warn( "Legacy mtx file found (%s): assuming double data (bytes = %d)\n", filename.c_str(), bytes);

  clear();
  data = new double [size[0]*size[1]*size[2]]; 
  data_loaded = 1;


  bool progress = (size[0]*size[1]*size[2]) > 100*100*100*5;

  int t1 = time(NULL);

  if (progress && progress_gui)
    {
      open_progress_gui();
      msg->value("Reading file: 0%");
    }

  static char msgbuf[256];
  
  // Now actually read the data in from the file

  for (i=0; i<size[0]; i++)
    {
      for (j=0; j<size[1]; j++)
	for (k=0; k<size[2]; k++)
	  {
	    if (bytes == 4)
	      {
		float tmp;
		if (fread(&tmp, bytes, 1, fp) != 1) 
		  badfilec( "Short read on mtx file: %s", filename.c_str());
		if (isnan(tmp)) warn( "nan at %d %d %d", i, j, k);
		getData(i,j,k) = tmp;
	      }
	    else if (bytes == 8)
	      {
		double tmp;
		if (fread(&tmp, bytes, 1, fp) != 1) 
		  badfilec( "Short read on mtx file: %s", filename.c_str()); 
		getData(i,j,k) = tmp;
	      }
	    else 
	      badfile( "Unsupported number of bytes %d", bytes);
	  }
      if (progress_gui)
	Fl::check();
      if (progress)
	{
	  snprintf(msgbuf,sizeof(msgbuf), "Reading File: %.0f%%", 1.0*i/(size[0]-1)*100.0);
	  if (progress_gui)
	    msg->value(msgbuf);
	  else 
	    info("%s\r", msgbuf);
	}
    }

  if (!progress_gui && progress)
    info("\n");
  
  if (progress_gui && progress)
    close_progress_gui();

  int t2 = time(NULL);
  // Output timing info here if you want

  if (found_units)
    {
      char *p = strtok(units_header, ",");
      p = strtok(0, ",");
      while (isspace(*p)) p++;
      dataname = p;
      for (int i = 0; i<3; i++)
	{
	  p = strtok(0, ","); 
	  while (isspace(*p)) p++;
	  axisname[i] = p;
	  p = strtok(0, ","); 
	  while (isspace(*p)) p++;
	  sscanf(p, "%lf", &axismin[i]);
	  p = strtok(0, ","); 
	  while (isspace(*p)) p++;
	  sscanf(p, "%lf", &axismax[i]);
	}
    }
  else
    {
      dataname = "Data Value";
      axismin[0] = axismin[1] = axismin[2] = 0;
      axismax[0] = size[0]; axisname[0] = "X";
      axismax[1] = size[1]; axisname[1] = "Y";
      axismax[2] = size[2]; axisname[2] = "Z";
    }

  data_loaded = 1;
  fclose(fp);
  return 0;
} 


// Load data from a gnuplot formatted files into a 3D matrix. 
// Here, we use the index number as the third dimensiont of the 3d matrix. 
// The user must specify which column of the gnuplot file the data should be read from.

int MTX_Data::load_gp_index(const char *name, int colnum)
{
  vector<double> datavec;
  double datatmp;
  char linebuffer[LINESIZE];
  char *p;


  char sep[] = " \t\n\r";

  FILE *fp = fopen(name, "rb");
  if(fp == NULL)
    badfile("Unable to open file \"%s\": %s\n",name,strerror(errno));
  
  int lines_per_block; // size of the datablocks 
  int line;

  int blocks_per_index;
  int block;
  
  int num_indices;

  bool first_block = true;
  bool first_index = true;

  line = lines_per_block = 0;
  block = blocks_per_index = 0;
  num_indices = 0;

  int nread = 0;
  bool block_ended = false;
  bool index_ended = false;
  int nptread = 0;

  bool found_data;

  int t1 = time(NULL);
  if (progress_gui)
    {
      open_progress_gui();
      msg->value("Lines read: 0");
    }

  while (1)
    {
      nread++;
      if (fgets(linebuffer, LINESIZE, fp) == NULL)
	break;
	  
      // Figure out what kind of line this is
      // Get rid of spaces at beginning of line
      for(p = linebuffer; *p != 0 && isspace(*p); p++)
	;
      // Ignore comment lines
      if(*p == '#')
	continue;
      // A blank line signals the end of a datablock or of an index
      if (*p == '\n' || *p == 0)
	{
	  if (!block_ended) // We have reached the end of a block
	    {
	      if (first_block) //we found the end of the first block
		{ 
		  if (line == 0) //ignore blank lines at the top of the file
		    continue; 
		  lines_per_block = line;
		  first_block = false;
		}
	      if (line != lines_per_block) 
		{
		  info("block %d ended early, assuming incomplete file\n");
		  break;
		}
	      //badfilec( "block %d at line %d has %d lines < %d\n", 
	      //block, nread, line, lines_per_block);
	      line = 0;
	      block_ended = true; 
	      block++;
	      continue;
	    }
	  if (!index_ended)  // this means we have found two blank lines
	    {
	      if (first_index)
		{
		  blocks_per_index = block;
		  first_index = false;
		}
	      if (block != blocks_per_index) 
		badfilec( "index %d at line %d has %d blocks < %d\n", 
			  num_indices, nread, block, blocks_per_index);
	      block = 0;
	      index_ended = true;
	      num_indices++;
	      continue;
	    }
	  else //discard any addional blank lines after we finish an index
	    continue; 
	}
      
      // This line contains real data
      index_ended = block_ended = false;
      
      // Parse the line and perform the conversions
      int col = 0;
      found_data = false;
      p = strtok(linebuffer, sep);
      while (p != NULL)
	{
	  if (sscanf(p, "%lf", &datatmp) != 1)
	    warn("mtx gnuplot: invalid data at line %d col %d: \"%s\", copying last read value\n", nread, col, p);
	  if (col == colnum)
	    {
	      datavec.push_back(datatmp);
	      found_data = true;
	      break;
	    }
	  col++;
	  p = strtok(0, sep);
	}
      
      if (!found_data)
	badfile( "Failed to find data in column %d at line %d", colnum, nread);
      nptread++;
      line++;

      if (nread%100 == 0)
	{
	  static char buf[256];
	  snprintf(buf, sizeof(buf), "Lines read: %d", nread);
	  if (progress_gui)
	    {
	      msg->value(buf);
	      Fl::check();
	    }
	  else
	    info("%s\r", buf);
	}
    }

  if (progress_gui)
    close_progress_gui();
  else
    info("\n");

  fclose(fp);

  int t2 = time(NULL);

  info("points: %d of %d\n", nptread, lines_per_block * blocks_per_index * num_indices);
  info("lines: %d of %d\n", line, lines_per_block);
  info("blocks: %d of %d\n", block, blocks_per_index);
  info("indices: %d\n", num_indices);

  if (blocks_per_index == 0) // if we're still reading the first index...
    {
      blocks_per_index = block+1;
      info("not yet finished the first index, settigns blocks_per_index to %d", blocks_per_index);
    }

  //if (nptread < lines_per_block * blocks_per_index * num_indices)
  if (block != 0 || line != 0) // we didn't find the end of and index or the end of a line.
    {
      num_indices++;
      int npts_needed = lines_per_block * blocks_per_index * num_indices;
      int nadd = npts_needed - nptread;
      info("looks like an incomplete file\n");      
      info("will assume size is %d, %d, %d", lines_per_block, blocks_per_index, num_indices);
      info("read %d data points, need %d\n", nptread, npts_needed);
      info("adding %d points with last value found in file %e", nadd, datatmp);
      if (nadd > 0)
	for (int i = 0; i < nadd ; i++)
	  datavec.push_back(datatmp);
    }

//   info( "lines per block %d blocks per index %d num indices %d\n", lines_per_block, blocks_per_index, num_indices);

//   info("Number of points read: %d\n", nptread);
//   info("Number of points expe: %d\n", lines_per_block * blocks_per_index * num_indices);
//   info("Lines per block:    %d\n", lines_per_block);
//   info("Line in last block: %d\n", line);

  if (lines_per_block == 0 || blocks_per_index == 0 || num_indices == 0)
    return -1;

  // When loading data linearly into MTX matrix from gnuplot3d format, we get the following data ordering:
  // X = loop 2 = index nimber
  // Y = loop 1 = block
  // Z = sweep  = point number

  size[0] = lines_per_block; 
  size[1] = blocks_per_index; 
  size[2] = num_indices; 

  if (size[0]*size[1]*size[2] == 0)
    {
      info("file has no complete lines\n");
      return -1;
    }

  clear(); 
  data = new double [size[0]*size[1]*size[2]]; 
  data_loaded = 1; 

  for (int k=0; k<size[2]; k++)
    for (int j=0; j<size[1]; j++)
      for (int i=0; i<size[0]; i++)
	getData(i,j,k) = datavec[k*size[1]*size[0]+j*size[0]+i];

  dataname = "Data Value";
  axismin[0] = axismin[1] = axismin[2] = 0;
  axismax[0] = size[0]; axisname[0] = "X";
  axismax[1] = size[1]; axisname[1] = "Y";
  axismax[2] = size[2]; axisname[2] = "Z";
  if (parse_txt) parse_delft_txt(name, false);
  return 0;
}

// Again loading gnuplot formatted data into a 3d matrix
// However, this time, indices are ignored and the 3rd 
// dimension of the matrix is used to load data from 
// the different columns of the data file.
  
int MTX_Data::load_gp_cols(const char *name)
{
  vector<double> datavec;
  double datatmp;
  char linebuffer[LINESIZE];
  char *p;

  char sep[] = " \t\n\r";

  FILE *fp = fopen(name, "rb");
  if(fp == NULL)
    badfile("Unable to open file \"%s\": %s\n",strerror(errno));

  int col;
  int cols_per_line;
  
  int line;
  int lines_per_block;

  int num_blocks; // number of datablocks so far (note: we count
		     // indexes (ie. blocks separated by two blank
		     // lines) also as datablocks
  
  bool first_line = true;
  bool first_block = true;

  col = cols_per_line = 0;
  line = lines_per_block = 0;
  num_blocks = 0;

  int nread = 0;

  int t1 = time(NULL);
  if (progress_gui)
    {
      open_progress_gui();
      msg->value("Lines read: 0");
    }

  while (1)
    {
      nread++;
      if (fgets(linebuffer, LINESIZE, fp) == NULL)
	break;
      
      // Figure out what kind of line this is

      // Get rid of spaces at beginning of line
      for(p = linebuffer; *p != 0 && isspace(*p); p++)
	;
      
      // Ignore comment lines
      if(*p == '#')
	continue;

      // A blank line signals the end of a datablock
      if (*p == '\n' || *p == 0)
	{
	  if (line == 0) //ignore blank lines at the top of the file and extra blank lines between blocks
	    continue; 
	  num_blocks++;
	  if (first_block) //we found the end of the first block
	    { 
	      lines_per_block = line;
	      first_block = false;
	    }
	  if (line != lines_per_block) //this is possible if the dataset is not done yet, so we'll be nice and not exit
	    {
	      info("Block size does not match on datablock %d (line %d)!  Ending file read and discarding block.\n", num_blocks, nread);
	      num_blocks--;
	      break;
	    }
	  line = 0;
	  continue;
	}

      // Parse the line and perform the conversions
      col = 0;
      p = strtok(linebuffer, sep);
      while (p != NULL)
	{
	  if (sscanf(p, "%lf", &datatmp) != 1)
	    info("mtx gnuplot: invalid data at line %d col %d: \"%s\", copying last read value\n", nread, col, p);
	  datavec.push_back(datatmp);
	  col++;
	  if (!first_line && col == cols_per_line)
	    break;
	  p = strtok(0, sep);
	}
      if (first_line)
	{
	  first_line = false;
	  cols_per_line = col;
	  //info( "gp_cols: found %d columns\n", cols_per_line);
	}
      line++;
	  
      if (nread%100 == 0)
	{
	  static char buf[256];
	  snprintf(buf, sizeof(buf), "Lines read: %d", nread);
	  if (progress_gui)
	    {
	      msg->value(buf);
	      Fl::check();
	    }
	  else
	    info("%s\r", buf);
	}

      // We will ignore extra columns, but we will be cowardly and exit if
      // we don't find enough columns
      if (col < cols_per_line)
	{
	  info("Too few columns at line %d, assuming incomplete file\n", nread);
	  break;
	}
    }

  fclose(fp);

  if (progress_gui)
    close_progress_gui();
  else
    info("\n");

  //  info( "gp_cols: cols_per_line %d lines_per_block %d num_blocks %d\n", 
  //	  cols_per_line, lines_per_block, num_blocks);
  
  int t2 = time(NULL);
  
  //info("Read %d lines at %.3f thousand lines per second\n", nread, 1.0*nread/(t2-t1)/1e3);

  size[0] = cols_per_line;
  size[1] = lines_per_block;
  size[2] = num_blocks;

  if (size[0]*size[1]*size[2] == 0)
    {
      warn("file has no complete lines\n");
      return -1;
    }

  clear();
  data = new double [size[0]*size[1]*size[2]];
  data_loaded = 1;

  for (int k=0; k<size[2]; k++)
    for (int j=0; j<size[1]; j++)
      for (int i=0; i<size[0]; i++)
	getData(i,j,k) = datavec[k*size[1]*size[0]+j*size[0]+i];

  dataname = "Data Value";
  axismin[0] = axismin[1] = axismin[2] = 0;
  axismax[0] = size[0]; axisname[0] = "X";
  axismax[1] = size[1]; axisname[1] = "Y";
  axismax[2] = size[2]; axisname[2] = "Z";
  if (parse_txt) parse_delft_txt(name, true);
  return 0;
}

void MTX_Data::get_settings(const char *comments)
{
  char *p = strstr(comments, "Loop 2");
  p = strstr(p, "DAC");
  
  info("getting settings\n");
  char buf[256];
  settings = " at ";
  double val;
  for (int i=1; i<= 16; i++) 
    {
      //getchar();
      p = strchr(p, ' ');
      if (sscanf(p, "%lf", &val) == 1)
	{
	  if (val != 0.0)
	    {
	      sprintf(buf, "D%d=%g ", i, val);
	      settings += buf;
	    }
	}
      p = strstr(p, "DAC");
    }
  info("settings:\n%s\n", settings.c_str());
}
      
	    


// *ident is the text at the start of the line, line "Sweep " or "Loop 1".
  
void MTX_Data::parse_comments(char *comments, const char *ident, int *dac, int *size, char *name, double *range)
{
  double mult;
  char *p;
  char buf[256];

  // We never use the pulse stuff: recently modified a copy of the
  // labview to use the pulse drop down items for a second RF
  // generator.
  string extra_names[] = {
    "RF Freq",
    "RF Power",
    "RF2 Freq", 
    "RF2 Power",
    "Pulse amp min",
    "Pulse amp max",
    "Pulse amp",
    "Pulse offset",
    "Magnetic Field (T)"};
  
  // First get the "dac" number
  p = strstr(comments, ident);
  sscanf(p+strlen(ident), "%d", dac);

  // Now get the number of steps
  p = strstr(comments, ident);
  p = strstr(p, "in ");
  sscanf(p+3, "%d", size);

  // If we're sweeping "nothing", then this is easy.
  if (*dac == 0)
    {
      snprintf(name, 256, "%sNumber", ident); //something like "Loop 1 Number"
      range[0] = 0;
      range[1] = *size;
      return;
    }
      
  // If this is a real DAC, then find the DAC name and the multiplier
  // from the comments
  if (*dac < 17)
    {
      int n = *dac - 1; // in commments, DAC1 becomes DAC0...
      snprintf(buf, 256, "DAC%d \"", n);
      p = strstr(comments, buf);
      strncpy(name, p+strlen(buf), 256);
      p = strchr(name, '"');
      *p = 0;
  
      p = strstr(comments, buf);
      p = strchr(p, '"');
      p = strchr(p+1, '"');
      if (sscanf(p+1, "%lf", &mult) != 1) 
	{
	  warn("error reading mult\n");
	  mult = 1;
	}
    }
  else //otherwise, use the hard coded names for RF Freq, ..., Magnetic Field
    {
      mult = 1;
      int n = *dac-17;
      strncpy(name, extra_names[n].c_str(), 256);
    }
     
  // Now find the range
  p = strstr(comments, ident);
  p = strstr(p, "from ");
  sscanf(p+5, "%lf", &range[0]);
  p = strstr(p, "to ");
  sscanf(p+3, "%lf", &range[1]);

  if (!delft_raw_units)
    {
      range[0] *= mult;
      range[1] *= mult;
    }

  // We're done!
}

// what a mess...i should really rewrite the labview code to output meta.txt files...
  
void MTX_Data::parse_var2_comments(char *comments, const char *ident, int *dac, char *name, double *range)
{
  double mult;
  char *p;
  char buf[256];
  
  info("parsing var2 for %s\n", ident);

  string extra_names[] = {
    "RF Freq",
    "RF Power",
    "Pulse Width",
    "Pulse Freq",
    "Pulse amp min",
    "Pulse amp max",
    "Pulse amp",
    "Pulse offset",
    "Magnetic Field (T)"};
  
  // First get the "dac" number
  if ((p = strstr(comments, ident)) == NULL)
    {
      // bug in some versions of the labview...
      if (strcmp(ident, "Loop 1 var2") == 0)
	{
	  p = strstr(comments, "Loop 1");
	  p = strstr(p, "Sweep var2");
	  if (p == NULL)
	    {
	      info("failed to correct for buggy labview ident %e", ident);
	      return;
	    }
	}
      else if (strcmp(ident, "Loop 2 var2") == 0)
	{
	  p = strstr(comments, "Loop 2");
	  p = strstr(p, "Sweep var2");
	  if (p == NULL)
	    {
	      info("failed to correct for buggy labview ident %e", ident);
	      return;
	    }
	}
      else
	{
	  info("failed to find identifier %s\n", ident);
	  return;
	}
    }

  sscanf(p+strlen(ident), "%d", dac);
  
  // If we're sweeping "nothing", then this is easy.
  if (*dac == 0)
    {
      snprintf(name, 256, "%sNumber", ident); //something like "Loop 1 Number"
      range[0] = 0;
      range[1] = *size;
      return;
    }
      
  // If this is a real DAC, then find the DAC name and the multiplier
  // from the comments
  if (*dac < 17)
    {
      int n = *dac - 1; // in commments, DAC1 becomes DAC0...
      snprintf(buf, 256, "DAC%d \"", n);
      p = strstr(comments, buf);
      strncpy(name, p+strlen(buf), 256);
      p = strchr(name, '"');
      *p = 0;
  
      p = strstr(comments, buf);
      p = strchr(p, '"');
      p = strchr(p+1, '"');
      if (sscanf(p+1, "%lf", &mult) != 1) 
	{
	  warn("error reading mult\n");
	  mult = 1;
	}
    }
  else //otherwise, use the hard coded names for RF Freq, ..., Magnetic Field
    {
      mult = 1;
      int n = *dac-17;
      strncpy(name, extra_names[n].c_str(), 256);
    }
     
  // Now find the range
  if ((p = strstr(comments, ident)) == NULL)
    {
      // bug in some versions of the labview...
      if (strcmp(ident, "Loop 1 var2") == 0)
	{
	  p = strstr(comments, "Loop 1");
	  p = strstr(p, "Sweep var2");
	  if (p == NULL)
	    {
	      info("failed to correct for buggy labview ident %e", ident);
	      return;
	    }
	}
      else if (strcmp(ident, "Loop 2 var2") == 0)
	{
	  p = strstr(comments, "Loop 2");
	  p = strstr(p, "Sweep var2");
	  if (p == NULL)
	    {
	      info("failed to correct for buggy labview ident %e", ident);
	      return;
	    }
	}
      else
	{
	  info("failed to find identifier %s\n", ident);
	  return;
	}
    }

  p = strstr(p, "from ");
  sscanf(p+5, "%lf", &range[0]);
  p = strstr(p, "to ");
  sscanf(p+3, "%lf", &range[1]);

  if (!delft_raw_units)
    {
      range[0] *= mult;
      range[1] *= mult;
    }

  // We're done!
}

void MTX_Data::parse_delft_txt(const char *name, bool columns)
{
  char txtname[256];
  char *p;
  strncpy(txtname, name, 256);
  if ((p = strstr(txtname, ".dat")) == NULL)
    {
      warn("filename %s does not contain .dat", name);
      return;
    }
  *p=0;
  strcat(txtname, ".txt");

  //info("Parsing delft txt file %s\n", txtname);
  FILE *fp = fopen(txtname, "r");
  if (fp == NULL)
    {
      warn("Error opening file %s: %s\n", txtname, strerror(errno));
      return;
    }
	   
  char comments[1024*10];
  fread(comments, 1, 1024*10, fp);
  fclose(fp);

  // Read sweep settings
  int sweep_steps;
  int loop1_steps;
  int loop2_steps;

  double sweep_range[2];
  double loop1_range[2];
  double loop2_range[2];

  int sweep_dac;
  int loop1_dac;
  int loop2_dac;

  char sweep_name[256];
  char loop1_name[256];
  char loop2_name[256];

  parse_comments(comments, "Sweep ", &sweep_dac, &sweep_steps, sweep_name, sweep_range);
  parse_comments(comments, "Loop 1 ", &loop1_dac, &loop1_steps, loop1_name, loop1_range);
  parse_comments(comments, "Loop 2 ", &loop2_dac, &loop2_steps, loop2_name, loop2_range);

  double sweep_var2_range[2];
  double loop1_var2_range[2];
  double loop2_var2_range[2];

  int sweep_var2_dac;
  int loop1_var2_dac;
  int loop2_var2_dac;

  char sweep_var2_name[256];
  char loop1_var2_name[256];
  char loop2_var2_name[256];

  parse_var2_comments(comments, "Sweep var2", &sweep_var2_dac, sweep_var2_name, sweep_var2_range);
  parse_var2_comments(comments, "Loop 1 var2", &loop1_var2_dac, loop1_var2_name, loop1_var2_range);
  parse_var2_comments(comments, "Loop 2 var2", &loop2_var2_dac, loop2_var2_name, loop2_var2_range);

  char buf[256];
  string sweep_extra = "";
  if (sweep_var2_dac != 0)
    {
      snprintf(buf, 256, "%.3f to %.3f", sweep_var2_range[0], sweep_var2_range[1]);
      sweep_extra = sweep_extra + " and " + sweep_var2_name + " from " + buf;
    }

  string loop1_extra = "";
  if (loop1_var2_dac != 0)
    {
      snprintf(buf, 256, "%.3g to %.3g", loop1_var2_range[0], loop1_var2_range[1]);
      loop1_extra = loop1_extra + " and " + loop1_var2_name + " from " + buf;
    }

  string loop2_extra = "";
  if (loop2_var2_dac != 0)
    {
      snprintf(buf, 256, "%.3g to %.3g", loop2_var2_range[0], loop2_var2_range[1]);
      loop2_extra = loop2_extra + " and " + loop2_var2_name + " from " + buf;
    }
      

  string tmp;
    if (delft_raw_units)
    tmp = " (raw mV)";
  else
    tmp = " (mV)";

  //if (delft_settings)
  get_settings(comments);

  if (!columns)
    {
      axisname[0] = sweep_name;
      axisname[0].append(tmp);
      axisname[0] += sweep_extra;
      axisname[1] = loop1_name;
      axisname[1].append(tmp);
      axisname[1] += loop1_extra;
      axisname[2] = loop2_name;
      axisname[2].append(tmp);
      axisname[2] += loop2_extra;
      

      // set the sweep range
      axismin[0] = sweep_range[0];
      axismax[0] = sweep_range[1];

      // flip is now done correctly in load_mtx_cut, so we no longer need to flip...
      axismin[1] = loop1_range[0];
      axismax[1] = loop1_range[1];

      // this one is fine.
      axismin[2] = loop2_range[0];
      axismax[2] = loop2_range[1];
    }

  else 
    {
      axisname[0] = "Column Number";
      axismin[0] = 1;
      axismax[0] = size[0] + 1;
      
      axisname[1] = sweep_name;
      axisname[1].append(tmp);
      axisname[1] += sweep_extra;

      axismin[1] = sweep_range[0];
      axismax[1] = sweep_range[1];
      
      // Check for unfinished loop
      if (loop1_steps != size[2])
	{
	  //info("loop1 %f %f steps %d size %d\n", loop1_range[0],
	  //loop1_range[1], loop1_steps, size[2]);
	       
	  double tmp  = loop1_range[0] 
	    + (loop1_range[1] - loop1_range[0])
	    * 1.0*size[2]/loop1_steps;
	  //info("Found incomplete loop: %d vs %d steps\n"
	  //"Adjusting endpoint from %e to %e\n",
	  //size[2], loop1_steps, loop1_range[1], tmp);
	  loop1_range[1] = tmp;
	}
      
      axisname[2] = loop1_name;
      axisname[2].append(tmp);
      axisname[2] += loop1_extra;

      // Note: here we need to flip loop 1 endpoints
      // flip is now done correctly in load_mtx_cut, so we no longer need to flip...
      axismin[2] = loop1_range[0];
      axismax[2] = loop1_range[1];
    }

  // 
  // A reasonable default... :)
  dataname = "Current (pA)"; 
  if (delft_settings)
    dataname += settings;
} 

int nextline(FILE *fp, char *buf)
{
  if (fgets(buf, LINESIZE, fp) == NULL) 
    return -1;
  while (buf[0] == '#')
    if (fgets(buf, LINESIZE, fp) == NULL) 
      return -1;
  //info(buf);
  return 0;
}

double nextreading(FILE *fp, int col, int &lnum)
{
  //info("in next reading\n");
  //getchar();
  // char *buf = new char [LINESIZE]; doesn't work under windows? c++ runtime error?
  char *buf = malloc(LINESIZE*sizeof(char));
  double val;
  char *p;

  // Find the next line
  while (1)
    {
      lnum++;
      if (fgets(buf, LINESIZE, fp) == NULL) 
	{
	  val = NAN;
	  break;
	}
      // Strip whitespace
      for(p = buf; *p != 0 && isspace(*p); p++);
      //Ignore comments or empty lines
      if (*p == '#' || *p == '\n' || *p == '\r' || *p == 0)
	continue;
      val =  parse_reading(buf, col);
      break;
    }

  free(buf);
  return val;
}

double parse_reading(char *line, int col)
{
  char sep[] = " \t\n\r";
  double val;
  int i;
  char *p; 
  char *buf = strdup(line);
  
  // Now try to read the data
  p = strtok(buf, sep);
  for (i=0; i < col; i++) 
    if (p != NULL) p = strtok(0,sep);
  if (p == NULL) 
    {
      info("trouble parsing row for column %d:\n%s", col, line);
      val = NAN;
    }
  //info("reading: %s\n", p);
  else if (sscanf(p, "%lf", &val) != 1)  
    {
      info("trouble converting column value _%s_ to a float", p);
      val = NAN;
    }
  free(buf);
  return val; 
} 


int MTX_Data::load_dat_meta(const char *name, int col)
{
  info("read %s col %d\n", name, col);
  char *buf = new char [LINESIZE];

  // First open the metadata file and get the stuff we need.
  string metafile = name;
  metafile = search_replace(metafile, ".dat", ".meta.txt");
  FILE *fp = fopen(metafile.c_str(), "r");
  if (fp == NULL) return -1;

  // First stuff should be the axis info
  for (int i=0; i<3; i++)
    {
      if (nextline(fp, buf) == -1) return -1;
      if (sscanf(buf, "%d", &size[i]) != 1) return -1;
      if (nextline(fp, buf) == -1) return -1;
      if (sscanf(buf, "%lf", &axismin[i]) != 1) return -1;
      if (nextline(fp, buf) == -1) return -1;
      if (sscanf(buf, "%lf", &axismax[i]) != 1) return -1;
      if (nextline(fp, buf) == -1) return -1;
      axisname[i] = buf; strip_newlines(axisname[i]);
    }

  // a recurring theme in spyview: we need to flip the y axis range...

  // 19 July 2011: testing now with a file from Vincent, it seems we
  // don't need to flip the y axis?  

  // Actually, it is handier if we do flip the y axis, otherwise we
  // need to flip the data afterwards for a conventional sweep.

  double tmp = axismin[1];
  axismin[1] = axismax[1];
  axismax[1] = tmp;

  // The next lines in the metadata file should be the (optional)
  // column names. Start with a default label based on the column
  // number, then replace it if we find a specific column label in the
  // file.
  dataname = str_printf("Column %d", col+1); //Note: column number starts at zero in c...
  while (1)
    {
      int c;
      if (nextline(fp, buf) == -1) break;
      if (sscanf(buf, "%d", &c) != 1) break;
      c--;
      if (nextline(fp, buf) == -1) break;
      if (c == col)
	{
	  dataname = buf;
	  strip_newlines(dataname);
	}
    }
  //info("dlab _%s_\n", dataname.c_str());
  fclose(fp);

  // Ok, now reading the .dat file should be pretty easy.

  clear();
  data = new double [size[0]*size[1]*size[2]]; 
  data_loaded = 1;
  fp = fopen(name, "r");
  if (fp == NULL) return -2;

  bool progress = (size[0]*size[1]*size[2]) > 100*100*100*5;
  static char msgbuf[256];
  if (progress && progress_gui)
    {
      open_progress_gui();
      msg->value("Reading file: 0%");
    }

  double val, last_val;
  val = last_val = 0;
  int lnum = 0; // line number
  int npoints = 0;
  bool incomplete = false;

  int i,j,k;
  int i0, j0, k0;

  // In test file from Vincent, the meta.txt code is getting the wrong order?
  // for (k=0; k<size[2]; k++)
  //   {
  //     for (j=0; j<size[1]; j++)
  // 	{
  // 	  for (i=0; i<size[0]; i++)
  // 	    {
  for (i=0; i<size[0]; i++)
    {
      for (j=size[1]-1; j>=0; j--) // flip y data around in matrix...
	{
	  for (k=0; k<size[2]; k++)
	    {
	      if (incomplete) // fill matrix
		getData(i,j,k) = last_val;
	      else // otherwise try to get new data
		{
		  val = nextreading(fp, col, lnum);
		  if (isnan(val)) // failed to read a point
		    {
		      if (npoints == 0) 
			{
			  info("could not read any points\nassuming empty file, filling with zeros\n");
			  last_val = 0;
			  getData(i,j,k) = last_val;
			  incomplete = true;
			}
		      else
			{
			  info("Failed to find point number %d from line number %d, i,j,k = %d %d %d.\n"
			       "Assuming incomplete file\n"
			       "filling matrix with last reading %e\n", npoints, lnum, i, j, k, last_val);
			  incomplete = true;
			  getData(i,j,k) = last_val;
			}
		    }
		  else // add the new data to the matrix
		    {
		      getData(i,j,k) = val;
		      last_val = val;
		      npoints++;
		    }
		}
	    }
	  if (!incomplete && npoints % 1000 == 0)  // update progress gui
	    {
	      if (progress_gui)
		Fl::check();
	      if (progress)
		{
		  snprintf(msgbuf,sizeof(msgbuf), "Reading File: %.0f%%", 1.0*k/(size[2]-1)*100.0);
		  if (progress_gui)
		    msg->value(msgbuf);
		  else 
		    info("%s\r", msgbuf);
		}
	    }
	}
    }
  fclose(fp);
  return 0;
}
