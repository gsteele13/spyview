#include "ImageData.H"
#include <unistd.h>
#include <math.h>

void usage(const char *msg="")
{
  if (msg != NULL)
    info("Error: %s\n\n", msg);
  info("usage: dat2mtx [options] file.dat\n"
       "\n"
       " -c n1,n2,n3 column numbers to extract (NO DEFAULT, MUST SPECIFY!)\n"
       " -x [0,1,2] cross section to extract (split on: 0=x (yz cut), 1=y (xz cut), 2=z (xy cut)\n"
       " -s split into 2d MTX based on 3rd dimesion (loop2)\n" 
       " -h help\n" 
       " -r use \"real\" units from delft file\n"
       " -S load extra settings in title from delft txt file\n"
       " -m use meta.txt file\n" 
       );
  exit(0);
}

int main(int argc, char **argv)
{
  char *filename;
  char *basename;
  vector<int> columns;
  int xsection_type = 2;
  vector<string> xs_ext;
  xs_ext.push_back("yz");
  xs_ext.push_back("xz");
  xs_ext.push_back("xy");

  ImageData id;
  id.datfile_type = GNUPLOT;
  id.gpload_type = INDEX;
  //id.gp_column = 1;
  id.mtx.progress_gui = false;
  id.mtx.parse_txt = true;
  id.mtx.delft_raw_units = true;

  bool splitfile = false;
  char c;
  char *cols;
  char *p;
  int col;

  info("Build stamp: %s\n", BUILDSTAMP);

  while ((c = getopt(argc, argv, "mc:shrx:S")) != -1)
    {
      switch (c)
 	{
 	case 'c':
	  cols = strdup(optarg);
	  p = strtok(cols, ",");
	  while (p != NULL)
	    {
	      if (sscanf(p, "%d", &col) != 1)
		usage("Invalid column number");
	      info("p %s column %d\n", p, col);
	      col--;
	      columns.push_back(col);
	      p = strtok(0, ",");
	    }
	  break;
	case 's':
	  splitfile = true;
	  break;
	case 'S':
	  id.mtx.delft_settings = true;
	  break;
	case 'r':
	  id.mtx.delft_raw_units = false;
	  break;
	case 'm':
	  id.datfile_type = DAT_META;
	  break;
	case 'x':
	  if (sscanf(optarg, "%d", &xsection_type) != 1)
	    usage("invalid xsection type");
	  if (xsection_type < 0 || xsection_type > 2)
	    usage("invalid xsection type");
	  break;
	case 'h':
	case '?':
	default:
	  usage();
 	}
    }

  if (columns.size() < 1)
    usage("Specify columns using -c");
  
  if (optind > argc)
    usage("need to specify filename!");

  filename = strdup(argv[optind]);
  basename = strdup(filename);
  p = strstr(basename, ".dat");
  *p = 0;
  string outname;
  char buf[256];

  for (int n=0; n<columns.size(); n++)
    {
      id.gp_column = columns[n];
      info("loading file, col %d\n", columns[n]+1);
      id.load_file(filename);
      info("mtx size %d %d %d\n", 
	   id.mtx.size[0], 
	   id.mtx.size[1],
	   id.mtx.size[2]);
      info("x (sweep): %s %.1f %.1f %d\n",
	   id.mtx.axisname[0].c_str(), id.mtx.axismin[0], id.mtx.axismax[0], id.mtx.size[0]);
      info("y (loop1): %s %.1f %.1f %d\n",
	   id.mtx.axisname[1].c_str(), id.mtx.axismin[1], id.mtx.axismax[1], id.mtx.size[1]);
      info("z (loop2): %s %.1f %.1f %d\n",
	   id.mtx.axisname[2].c_str(), id.mtx.axismin[2], id.mtx.axismax[2], id.mtx.size[2]);
      
      outname = basename;
      sprintf(buf, ".c%d",  columns[n]+1);
      outname += buf;
      
      if (!splitfile)
	{
	  outname += ".mtx";
	  info("writing file:\n %s\n", outname.c_str());
	  FILE *fp = fopen(outname.c_str(), "wb");
	  fprintf(fp, 
		  "Units, %s,"
		  "%s, %e, %e,"
		  "%s, %e, %e,"
		  "%s, %e, %e\n",
		  id.mtx.dataname.c_str(),
		  id.mtx.axisname[0].c_str(), id.mtx.axismin[0], id.mtx.axismax[0],
		  id.mtx.axisname[1].c_str(), id.mtx.axismin[1], id.mtx.axismax[1], 
		  id.mtx.axisname[2].c_str(), id.mtx.axismin[2], id.mtx.axismax[2]);
	  fprintf(fp, "%d %d %d 8\n", id.mtx.size[0], id.mtx.size[1], id.mtx.size[2]);
      
	  for (int i=0; i<id.mtx.size[0]; i++)
	    for (int j=0; j<id.mtx.size[1]; j++)
	      for (int k=0; k<id.mtx.size[2]; k++)
		fwrite(&id.mtx.getData(i,j,k), sizeof(double), 1, fp);
	  
	  fclose(fp);
	}
      else
	{
	  info("writing files:\n");
	  outname += ".";
	  outname += xs_ext[xsection_type];
	  id.do_mtx_cut_title = true;
	  
	  // type: 0 = split on x, ...
	  id.mtx_cut_type = (mtxcut_t)xsection_type;

	  char fmt[256];
	  int digits = sprintf(fmt, "%d", id.mtx.size[xsection_type]);
	  snprintf(fmt, sizeof(fmt), ".%%0%dd", digits);

	  for (int m=0; m<id.mtx.size[xsection_type]; m++)
	    {
	      id.mtx_index = m;
	      snprintf(buf, sizeof(buf), fmt, m);
	      string out2 = outname;
	      out2 += buf; 
	      out2 += ".mtx";
	      info(" %s\n", out2.c_str());
	      id.load_mtx_cut();
	      id.saveMTX(out2.c_str());
	    }
	}
    }    
}
