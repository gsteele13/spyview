#include "ImageData.H"
#include <unistd.h>
#include <math.h>

void usage(const char *msg="")
{
  if (msg != NULL)
    info("Error: %s\n\n", msg);
  info("usage: gilles2mtx [options] file.dat\n"
       "\n"
       " -m use meta.txt file\n"
       "\n"
       );
  exit(0);
}

int main(int argc, char **argv)
{
  ImageData id;
  id.datfile_type = DELFT_LEGACY;
  char c;

  while ((c = getopt(argc, argv, "mc:shr:")) != -1)
    {
      switch (c)
 	{
	case 'm':
	  id.datfile_type = DAT_META;
	  break;
	case 'h':
	case '?':
	default:
	  usage();
 	}
    }

  id.mtx.progress_gui = false;

  if (optind > argc)
    usage("Error: need to specify filename!");

  char *filename = strdup(argv[optind]);

  char *outname = (char *)malloc(strlen(filename)+20); //for the file.??.mtx 
  char *p;
  FILE *fp;
  
  id.gp_column = 2;  
  if (id.load_file(filename) == -1)
    usage("error reading column 3 from datafile");
  strcpy(outname, filename);
  p = strstr(outname, ".dat");
  strcpy(p, ".3.mtx");
  info("Opening output filename %s\n", outname);
  fp = fopen(outname, "wb");
  fprintf(fp, 
	  "Units, %s,"
	  "%s, %e, %e,"
	  "%s, %e, %e,"
	  "Nothing, 0, 1\n",
	  id.zname.c_str(),
	  id.xname.c_str(), id.getX(0), id.getX(id.width-1),
	  id.yname.c_str(), id.getY(id.height-1), id.getY(0));
  fprintf(fp, "%d %d 1 8\n", id.width, id.height);

  for (int i=0; i<id.width; i++)
    for (int j=0; j<id.height; j++)
      fwrite(&id.raw(i,j), sizeof(double), 1, fp);
  fclose(fp);

  id.gp_column = 3;  
  if (id.load_file(filename) == -1)
    usage("error reading column 4 from datafile");
  strcpy(outname, filename);
  p = strstr(outname, ".dat");
  strcpy(p, ".4.mtx");
  info("Opening output filename %s\n", outname);
  fp = fopen(outname, "wb");
  fprintf(fp, 
	  "Units, %s,"
	  "%s, %e, %e,"
	  "%s, %e, %e,"
	  "Nothing, 0, 1\n",
	  id.zname.c_str(),
	  id.xname.c_str(), id.getX(0), id.getX(id.width-1),
	  id.yname.c_str(), id.getY(id.height-1), id.getY(0));
  fprintf(fp, "%d %d 1 8\n", id.width, id.height);
  for (int i=0; i<id.width; i++)
    for (int j=0; j<id.height; j++)
      fwrite(&id.raw(i,j), sizeof(double), 1, fp);
  fclose(fp);
}    
