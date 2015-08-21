#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

void _error(int line, const char *file, const char *func, const char *fmt, ...)   __attribute__ (( format (printf,4,5) ));

#define ERROR(x, arg...) _error(__LINE__,__FILE__,__FUNCTION__,x, ##arg )

void _error(int line, const char *file, const char *func, const char *fmt, ...) 
{
  fprintf(stderr,"%s(%s:%d): ",file,func,line);
  va_list va;
  va_start(va,fmt);
  vfprintf(stderr,fmt,va);
  va_end(va);
  exit(-1);
}

void usage()
{
  if (errno != 0)
    perror("main");
  fprintf(stderr, "stminfo -f file [query_string (all)]\n"
	  "  all = verbose information about all parameters\n"
	  "  scanrange = scan range, format XxY, units in microns\n"
	  "  rotation = scan rotation\n"
	  "  zoffset = z offset, in angstroms\n"
	  "  pc = plane compensation values, X,Y\n"
	  "  chname = channel name string\n"
	  "  time = total time for the scan, in minutes\n"
	  "  size = size of image, in pixels, XxY\n"
	  "  tpp = time per pixel, in miliseconds\n");
  exit(0);
}

int main(int argc, char **argv)
{
  FILE *fp = NULL;
  char *fn;
  char c;

  while ((c = getopt(argc, argv, "f:h")) != -1)
    {
      switch (c)
	{
	case 'f':
	  fn = strdup(optarg);
	  fp = fopen(fn, "r");
	  if (fp == NULL)
	    usage();
	  break;
	case 'h':
	  usage();
	  break;
	}
    }

  if (fp == NULL)
    usage();

  char *query;

  if (argc - optind < 1)
    query = strdup("all");
  else
    query = argv[optind];
  
  int hdrlen=0x1000;
  char header[0x1000];

  int w, h;
  double xvrange, yvrange, xcal, ycal, zcal, zoffset, xpc, ypc;    
  double linetime;
  double rotation;
  int chnum;
  char buf1[256], buf2[256];
  char ch_name[256];
  char ch_units[256];
  char user[256];
  char scandate[256];

  //Read in the header

  if (fread(header, 1, hdrlen, fp) < hdrlen)
    ERROR("Invalid STM file: %s\n", fn);

  // Parse the header

  if (sscanf(strstr(header, "Pix"), "Pix %d", &w) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "Lin"), "Lin %d", &h) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "SR0"), "SR0 %lf", &xvrange) != 1)  
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "SR1"), "SR1 %lf", &yvrange) != 1)
    ERROR("Error Parsing Header\n");
  
  if (sscanf(strstr(header, "S00"), "S00 %lf", &xcal) != 1) 
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "S10"), "S10 %lf", &ycal) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "S20"), "S20 %lf", &zcal) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "OS2"), "OS2 %lf", &zoffset) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "CM0"), "CM0 %lf", &xpc) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "CM1"), "CM1 %lf", &ypc) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "SAn"), "SAn %lf", &rotation) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "LTm"), "LTm %lf", &linetime) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "ImC"), "ImC %d", &chnum) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "STm"), "STm %256[^\n]", scandate) != 1)
    ERROR("Error Parsing Header\n");

  if (sscanf(strstr(header, "Usr"), "Usr %256[^\n]", user) != 1)
    ERROR("Error Parsing Header\n");

  sprintf(buf1, "A%dN", chnum);
  sprintf(buf2, "A%dN %%256\[^\n]", chnum);
  if (sscanf(strstr(header, buf1), buf2, ch_name) != 1)
    ERROR("Error Parsing Header\n");

  if (strcmp("all", query) == 0)
    {
      fprintf(stdout, 
	      "Filename:   %s\n"
	      "Scan Range: %.3f x %.3f um\n"
	      "Size:       %d x %d \n"
	      "Rotation:   %.2f\n"
	      "Ch Name:    %s\n"
	      "TPP:        %.2f ms\n"
	      "Scan Time:  %.2f min\n"
	      "Plane Comp: %.2f,%.2f\n" 
	      "Zoffset:    %.2f ang\n"
	      "Date:       %s\n"
	      "User:       %s\n",
	      fn,
	      xvrange*xcal/10000,
	      yvrange*ycal/10000,
	      w, h, 
	      rotation, 
	      ch_name, 
	      linetime/w,
	      linetime*h*2/60/1000,
	      xpc*100, ypc*100,
	      zoffset*zcal,
	      scandate,
	      user);
    }
  else if (strcmp("scanrange", query) == 0)
    fprintf(stdout, "%.3fx%.3f\n", xvrange*xcal/10000, yvrange*ycal/1000);
  else if (strcmp("rotation", query) == 0)
    fprintf(stdout, "%.2f\n", rotation);
  else if (strcmp("zoffset", query) == 0)
    fprintf(stdout, "%.2f\n", zoffset*zcal);
  else if (strcmp("pc", query) == 0)
    fprintf(stdout, "%.2f,%.2f\n", xpc*100, ypc*100);
  else if (strcmp("chname", query) == 0)
    fprintf(stdout, "%s\n", ch_name);
  else if (strcmp("time", query) == 0)
    fprintf(stdout, "%.2f min\n", linetime*h*2/60/1000);
  else if (strcmp("tpp", query) == 0)
    fprintf(stdout, "%.2f ms\n", linetime/w);
  else
    usage();
}
