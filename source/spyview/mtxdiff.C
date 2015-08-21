#include "ImageData.H"
#include <unistd.h>
#include <math.h>

void usage(const char *msg = "")
{
  if (msg != NULL)
    info("Error: %s\n\n", msg);
  info("usage: mtxdiff file.mtx\n"
       "\n"
       " take derivative of mtx file along the sweep direction\n"
       );
  exit(0);
}

int main(int argc, char **argv)
{

  char *filename;
  char *outname;

  ImageData id;

  if (argc<2) usage();
  
  filename = strdup(argv[1]);
  outname = (char *)malloc(strlen(filename)+20); //for the file.??.mtx 
  strcpy(outname, filename);
  char *p = strstr(outname, ".mtx");
  if(p == NULL)
    {
      fprintf(stderr,".mtx not found in file name; using end\n");
      p = outname + strlen(outname);
    }
  strcpy(p, ".diff.mtx");

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

  int size[3];
  size[0] = id.mtx.size[0]-1;
  size[1] = id.mtx.size[1];
  size[2] = id.mtx.size[2];

  //double *data = new double[s];
     
  FILE *fp = fopen(outname, "wb");
  fprintf(fp, 
	  "Units, %s,"
	  "%s, %e, %e,"
	  "%s, %e, %e,"
	  "%s, %e, %e\n",
	  id.mtx.dataname.c_str(),
	  id.mtx.axisname[0].c_str(), id.mtx.axismin[0], id.mtx.axismax[0],
	  id.mtx.axisname[1].c_str(), id.mtx.axismin[1], id.mtx.axismax[1], 
	  id.mtx.axisname[2].c_str(), id.mtx.axismin[2], id.mtx.axismax[2]);
  fprintf(fp, "%d %d %d 8\n", size[0], size[1], size[2]);

  double val;

  for (int i=0; i<size[0]; i++)
    for (int j=0; j<size[1]; j++)	
      for (int k=0; k<size[2]; k++)
	{
	  val = id.mtx.getData(i+1,j,k) - id.mtx.getData(i,j,k);
	  fwrite(&val, sizeof(double), 1, fp);
	}
  
  //fwrite(data, sizeof(double), s, fp);
  fclose(fp);
  
//   for (int k=0; k<size[2]; k++)
//     for (int j=0; j<size[1]; j++)
//       for (int i=0; i<size[0]; i++)
// 	fwrite(&data[k*size[1]*size[0]+j*size[0]+i], sizeof(double), 1, fp);
//   fclose(fp);


}    
