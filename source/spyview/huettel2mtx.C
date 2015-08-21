#include "ImageData.H"
#include <unistd.h>

#define LINEMAX 1024*10
void usage(const char *msg="")
{
  if (msg != NULL)
    info("Error: %s\n\n", msg);
  info("usage: dat2mtx [options] file.dat\n"
       "\n"
       " -r force split into fwd/rev files"
        );
  exit(0);
}

string parse(string ident, string metadata)
{
  size_t pos1, pos2;

  pos1 = metadata.find(ident);
  if (pos1 == string::npos) 
    info("ident %s not found!", ident.c_str());
  pos1 += ident.size();

  pos2 = metadata.find_first_of(" ,\r\n", pos1);
  if (pos2 == string::npos) 
    info("end of token for ident %s not found!", ident.c_str());
  
  return metadata.substr(pos1,pos2-pos1);
}

int main(int argc, char **argv)
{
  
  char *filename;
  string outname;

  double sweep_start;
  double sweep_end;
  string loop_start;
  string loop_end;
  string zname = "Current (pA)";

  ImageData id;

  bool split_fwd_rev = false;
  char c;

  while ((c = getopt(argc, argv, "r")) != -1)
    {
      switch (c)
	{
	case 'r':
	  split_fwd_rev = true;
	  break;
	case 'h':
	case '?':
	default:
	  usage();
	}
    }

  if (optind > argc)
    usage("Error: need to specify filename!");

  filename = strdup(argv[optind]);

  // First load the file into the image data class, using simply the GNUPLOT column format

  id.datfile_type = GNUPLOT;
  id.gpload_type = COLUMNS;
  id.gp_column=2;
  id.mtx.progress_gui = false;
  id.mtx.parse_txt = false;

  id.load_file(filename);
  info("file size: w %d h %d\n", id.width, id.height);

  // Now extract all of the useful metadata from the headers that are
  // spewed everywhere through the file

  vector<string> metadata;
  char linebuf[LINEMAX];
  FILE *fp = fopen(filename, "r");
  
  while (fgets(linebuf, LINEMAX, fp) != NULL)
    {
      if (strstr(linebuf, "B=") != NULL)
	metadata.push_back(linebuf);
    }

  //info("metadata.size = %d\n", metadata.size());
  //info("metadata[0] = \n%s\n", metadata[0].c_str());
  //info("metadata[%d] = \n%s\n", id.height-1, metadata[id.height-1].c_str());

  // Now find the sweep starting point by looking at the sweep value
  // of the first point

  rewind(fp);
  int n=0;
  double col1;
  while (fgets(linebuf, LINEMAX, fp) != NULL)
    {
      n++;
      if (strchr("#\n\r",linebuf[0]) != NULL)
	continue;
      if (sscanf(linebuf, "%lf\t%lf", &col1, &sweep_start) != 2)
	{
	  info("error reading sweep start value at line %d", n);
	  exit(0);
	}
      else
	break;
    }

  if (col1 == 0.5)
    {
      info("fwd/rev sweep file found\n");
      split_fwd_rev = true;
    }

  double tmp;
  // Now find the end of the sweep
  while (fgets(linebuf, LINEMAX, fp) != NULL)
    {
      n++;
      if (sscanf(linebuf, "%*f\t%lf", &tmp) != 1)
	break;
      sweep_end = tmp;
    }
  
  //info("sweep start is %e\n", sweep_start);
  //info("sweep end is   %e\n", sweep_end);

  // Find out which variable we are sweeping

  string sweep_variable;
  sweep_variable = parse("sw=", metadata[0]);
  //info("parsed sweep variable %s\n", sweep_variable.c_str());

  // this is fucking retarded: the names in the "sw=" field do not
  // match the labels used in the rest of the line. somebody is an idiot.
 
  int sweep_var=-1;
  if (sweep_variable == "DAC1") sweep_var=0;
  else if (sweep_variable == "DAC2") sweep_var=1;
  else if (sweep_variable == "DAC3") sweep_var=2;
  else if (sweep_variable == "DAC4") sweep_var=3;
  else if (sweep_variable == "DAC5") sweep_var=4;
  else if (sweep_variable == "DAC6") sweep_var=5;
  else if (sweep_variable == "DAC7") sweep_var=6;
  else if (sweep_variable == "DAC8") sweep_var=7;
  else if (sweep_variable == "RF_FREQ") sweep_var=8;
  else if (sweep_variable == "RF_POWER") sweep_var=9 ;
  else if (sweep_variable == "MAGNET") sweep_var=10;

  //info("sweep_var is %d\n", sweep_var);
  // Now try to parse the metadata to determine the loop variable(s)

  vector<string> to_parse;
  to_parse.push_back("D1=");
  to_parse.push_back("D2=");
  to_parse.push_back("D3=");
  to_parse.push_back("D4=");
  to_parse.push_back("D5=");
  to_parse.push_back("D6=");
  to_parse.push_back("D7=");
  to_parse.push_back("D8=");
  // stupid format: fortunately, this will pick the RF FREQ not the
  // PULSE FREQ, since it comes first
  to_parse.push_back("FREQ="); 
  to_parse.push_back("POWER=");
  to_parse.push_back("B=");

  vector<string> init_val;
  vector<string> final_val;
  vector<int> labels;

  // make a list of variables that are not zero
  for (int i=0; i<to_parse.size(); i++)
    {
      init_val.push_back(parse(to_parse[i], metadata[0]));
      final_val.push_back(parse(to_parse[i], metadata[id.height-1]));
      if (init_val[i] != "0.00" && final_val[i] != "0.00")
	labels.push_back(i);
    }

  string loop_variable;
  string settings;
  // Find out what other things may have changed to try to figure out
  // what the loop variable is
  for (int i=0; i<labels.size(); i++)
    {
      //info("label %d '%s' %s to %s\n", 
      //labels[i], to_parse[labels[i]].c_str(),
      //init_val[labels[i]].c_str(), 
      //final_val[labels[i]].c_str());
      // this one changed, it's probably a loop variable
     if (init_val[labels[i]] != final_val[labels[i]] &&
	  labels[i] != sweep_var)
	{
	  //take the loop axis as the range of the first likely loop variable that we find.
 	  if (loop_variable.size() == 0) 
 	    {
 	      loop_start = init_val[labels[i]];
 	      loop_end = final_val[labels[i]];
 	    }
	  loop_variable += to_parse[labels[i]] + 
	    init_val[labels[i]] + " to " + final_val[labels[i]] + "; ";
	}
      // this one didn't change, let's put it in the zname
     else if (labels[i] != sweep_var)
       settings += to_parse[labels[i]] + " = " + init_val[labels[i]] + "; ";
    }

  // I'm not sure where the best place is to put this...
  loop_variable += settings;

  //info("loop variable: %s\n", loop_variable.c_str());
  //info("zname is: %s\n", zname.c_str());

  // OK, now after 200 lines of dirty, messy code, we finally have the
  // loop, sweep, and other parameter settings

  // Now let's write the mtx file(s)

  char *p = strstr(filename, ".dat");
  *p = 0;

  outname = filename;
  if (!split_fwd_rev)
    outname += ".mtx";
  else 
    outname += ".fwd.mtx";

  info("outputting %s\n", outname.c_str());
  if (split_fwd_rev)
    id.even_odd(true,true);
      
  // Consistent with my previous plots, I will put sweep variable on the Y axis
  id.rotate_ccw();

  fp = fopen(outname.c_str(), "wb");
  fprintf(fp, "Units, "
	  "%s, "
	  "%s, %s, %s,"
	  "%s, %e, %e,"
	  "Nothing, 0, 1\n",
	  zname.c_str(),
	  loop_variable.c_str(), loop_start.c_str(), loop_end.c_str(),
	  sweep_variable.c_str(), sweep_start, sweep_end);  

  fprintf(fp, "%d %d 1 8\n", id.width, id.height);
  
  for (int i=0; i<id.width; i++)
    for (int j=0; j<id.height; j++)
      fwrite(&id.raw(i,j), sizeof(double), 1, fp);

  fclose(fp);

  if (split_fwd_rev)
    {
      outname = filename;
      outname += ".rev.mtx";
      info("outputting %s\n", outname.c_str());

      id.reset();
      id.even_odd(false,true);
      id.xflip();

      // Consistent with my previous plots, I will put sweep variable on the Y axis
      id.rotate_ccw();
      
      fp = fopen(outname.c_str(), "wb");
      fprintf(fp, "Units, "
	      "%s, "
	      "%s, %s, %s,"
	      "%s, %e, %e,"
	      "Nothing, 0, 1\n",
	      zname.c_str(),
	      loop_variable.c_str(), loop_start.c_str(), loop_end.c_str(),
	      sweep_variable.c_str(), sweep_start, sweep_end);  
      
      fprintf(fp, "%d %d 1 8\n", id.width, id.height);

      for (int i=0; i<id.width; i++)
	for (int j=0; j<id.height; j++)
	  fwrite(&id.raw(i,j), sizeof(double), 1, fp);
      
      fclose(fp);
    }

}

