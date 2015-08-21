#include "ImageData.H"
#include <unistd.h>

#define LINEMAX 1024*10

#define strip_newline(A)  {char *p = strrchr(A, '\n'); if (p) *p = '\0';}

void usage(const char *msg="")
{
  if (msg != NULL)
    info("Error: %s\n\n", msg);
  info("usage: toeno2mtx file.dat\n"
       "\n"
       " Creates file.mtx with headers using data from fileHeader.txt"
        );
  exit(0);
}

void parse(FILE *fp, string &var)
{
  char linebuf[LINEMAX];
  fgets(linebuf, LINEMAX, fp);
  strip_newline(linebuf);
  
  if (var.size() > 0)
    var = var + " (" + linebuf + " )";
  else 
    var = linebuf;
  //info("parsed _%s_\n", linebuf);
}

int main(int argc, char **argv)
{
  char *filename;
  string headername;
  string outname;
  string xname,  yname, zname;
  string xstart, ystart;
  string xend, yend;

  if (argc < 0) usage("must provide filename");

  ImageData id;

  // First load the file into the image data class, using simply the GNUPLOT column format

  id.datfile_type = MATRIX;
  id.mtx.progress_gui = false;
  id.mtx.parse_txt = false;

  filename = strdup(argv[1]);
  if (id.load_file(filename) == -1)
    usage("error opening file");

  info("file size: w %d h %d\n", id.width, id.height);

  char *p;
  p = strstr(filename, ".dat");
  *p = 0;
  
  headername = filename;
  headername += "Header.txt";
  
  FILE *fp = fopen(headername.c_str(), "r");
  char linebuf[LINEMAX];
  
  while (true)
    {
      if (fgets(linebuf, LINEMAX, fp) == NULL)
	break;
      if (strstr(linebuf, "Xlabel") != NULL)
	parse(fp, xname);
      if (strstr(linebuf, "Xunit") != NULL)
	parse(fp, xname);
      if (strstr(linebuf, "Ylabel") != NULL)
	parse(fp, yname);
      if (strstr(linebuf, "Yunit") != NULL)
	parse(fp, yname);
      if (strstr(linebuf, "Zlabel") != NULL)
	parse(fp, zname);
      if (strstr(linebuf, "Xstart") != NULL)
	parse(fp, xstart);
      if (strstr(linebuf, "Xend") != NULL)
	parse(fp, xend);
      if (strstr(linebuf, "Ystart") != NULL)
	parse(fp, ystart);
      if (strstr(linebuf, "Yend") != NULL)
	parse(fp, yend);
    }

  outname = filename;
  outname += ".mtx";

  info("outputting %s\n", outname.c_str());

  fp = fopen(outname.c_str(), "wb");
  fprintf(fp, "Units, "
	  "%s, "
	  "%s, %s, %s,"
	  "%s, %s, %s,"
	  "Nothing, 0, 1\n",
	  zname.c_str(),
	  xname.c_str(), xstart.c_str(), xend.c_str(),
	  yname.c_str(), ystart.c_str(), yend.c_str());

  fprintf(fp, "%d %d 1 8\n", id.width, id.height);
  
  for (int i=0; i<id.width; i++)
    for (int j=0; j<id.height; j++)
      fwrite(&id.raw(i,j), sizeof(double), 1, fp);

  fclose(fp);
}

