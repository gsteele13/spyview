#include <stdio.h>
#include <stdlib.h>

void usage()
{
  fprintf(stderr, "mtxextract dim index < file.mtx > file.pgm\n");
  exit(0);
}

int main(int argc, char **argv)
{
  int dim, index;
  char buf[256];
  int size[3];
  int bytes;

  if (argc < 2)
    usage();
  if (sscanf(argv[1], "%d", &dim) != 1)
    usage();
  if (sscanf(argv[2], "%d", &index) != 1)
    usage();
  
  fgets(buf, 256, stdin);
  if (sscanf(buf, "%d %d %d", &size[0], &size[1], &size[2]) != 3)
    { fprintf(stderr, "Malformed mtx header\n"); exit(-1); }
  if (sscanf(buf, "%*d %*d %*d %d", &bytes) != 1)
    { bytes = 8 ; fprintf(stderr, "Legacy mtx file found: assuming double data (bytes = %d)\n", bytes);}

  if (dim<0 || dim > 2)
    { fprintf(stderr, "invalid dimension %d\n", dim); exit(-1);}
  if (index > size[dim]-1)
    { fprintf(stderr, "invalid index %d for dimension %d (max %d)\n", index, dim, size[dim]-1); exit(-1);}
  
  double tmp;
  double datamax = -1e100;
  double datamin = 1e100;

  // Let's try and be cleaver

  int d1 = (dim+1)%3;
  int d2 = (dim+2)%3;
  double *data = new double[size[d1]*size[d2]];
  int n[3];

  int npts = 0;
  for (n[0]=0; n[0]<size[0]; n[0]++)
    for (n[1]=0; n[1]<size[1]; n[1]++)
      for (n[2]=0; n[2]<size[2]; n[2]++)
	{
	  if (bytes == 4)
	    {
	      float tmp;
	      if (fread(&tmp, sizeof(tmp), 1, stdin) != 1) 
		{ fprintf(stderr, "Short read on mtx file\n"); exit(-1); }
	      if (n[dim] == index)
		{
		  data[n[d1]*size[d2] + n[d2]] = tmp;
		  if (tmp < datamin) datamin = tmp;
		  if (tmp > datamax) datamax = tmp;
		  npts++;
		}
	    }
	  else if (bytes == 8)
	    {
	      float tmp;
	      if (fread(&tmp, sizeof(tmp), 1, stdin) != 1) 
		{ fprintf(stderr, "Short read on mtx file\n"); exit(-1); }
	      if (n[dim] == index)
		{
		  data[n[d1]*size[d2] + n[d2]] = tmp;
		  if (tmp < datamin) datamin = tmp;
		  if (tmp > datamax) datamax = tmp;
		  npts++;
		}
	      else	    
		{ fprintf(stderr, "Unsupported number of bytes %d", bytes); exit(0);}
	    }
	}
  fprintf(stderr, "Read %d points\n", npts);
 
  // Rescale data min and max to remap to the requested dynamic range 
  double center = (datamax+datamin)/2;
  double half_width  = (datamax-datamin)/2;

  datamin = center - half_width * 100 / 50;
  datamax = center + half_width * 100 / 50;
  
  fprintf(stdout, "P5\n%d %d\n#zmin %e\n#zmax %e\n65535\n",size[d1], size[d2], datamin, datamax);

  unsigned char ctmp;

  for (int i=0; i<size[d1]; i++)
    for (int j=0; j<size[d2]; j++)
      {
	tmp = (data[i*size[d2]+j] - datamin)/(datamax - datamin) * 65535.0;
	//fprintf(stdout, "%.0f ", datatmp);
	if (tmp < 0.0 || tmp > 65535.0 + 1e-6)
	  {
	    fprintf(stderr, "\n%d %d %f ", i,j, tmp);
	  }
	ctmp = (unsigned char) ((int)tmp / 256);
	fwrite(&ctmp, 1, 1, stdout);
	ctmp = (unsigned char) ((int)tmp % 256);
	fwrite(&ctmp, 1, 1, stdout);
      }
}
