#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Handy byte swapping macro (http://astronomy.swin.edu.au/~pbourke/dataformats/endian/)

#define SWAP_2(x) ( (((x) & 0xff) << 8) | ((unsigned short)(x) >> 8) )

void usage()
{
  fprintf(stderr, "ns2pgm input_files ... (outputs to input.0.pgm, input.1.pgm, input.2.pgm, etc...)\n");
#ifdef WIN32
  getchar();
#endif
  exit(0);
}

int main(int argc, char **argv)
{
  FILE *input_fp, *output_fp;

  int nimages = 0; // number of images in nanoscope file
  int npixels; // number of horizontal pixels in the image (hard coded maximum of 100 images per input file)
  int nlines; // number of lines 

  // Nanoscope images seem to always contain 2 bytes per pixel
  int data_offset[100]; // byte offset from start of file
  int data_length[100]; // number of bytes to read
  int image_type[100]; // for now, 0 for topography, 1 for amplitude


  int max_length = 0;
  double image_size[100]; // nanoscope images are always square?

  double ZSens_V_to_nm;
  double CurrentSens_V_to_nA;
  double ZDAtoVolt;  // usually 0.006713867V/LSB , 440V (+16bit) for nanoscope controllers
  double Z_scale_V[100]; // data scale in volts: use the to get the correted scaling factor
  double Z_limit_V;

  char line[256];

  if (argc < 2) usage();
  if (strncmp(argv[1],"-h",2) == 0) usage();

  // Information about nanoscope file format obtained from
  // nanoimport.C, which is part of the GxSM project

  char *fn;
  int fnum;

  for (fnum = 1; fnum < argc; fnum++)
    {
      fn = argv[fnum];

      nimages = 0;
      max_length = 0;

      // First, parse the header
      input_fp = fopen(fn, "rb");
      if (input_fp == NULL)
	{
	  fprintf(stderr, "Could not read file %s: %s\n", fn, strerror(errno));
#ifdef WIN32
	  getchar();
#endif
	}
      
      fprintf(stderr, "%s: ", fn);
      while (1)
	{
	  fgets(line, 256, input_fp);
      
	  // Global paramters, for all images in file

	  // Info for pixel size of image: all images in the file are the same size?
	  if(strncmp(line, "\\Valid data len X: ",18) == 0)
	    { npixels = atoi(&line[18]); continue;}
	  if(strncmp(line, "\\Valid data len Y: ",18) == 0)
	    { nlines = atoi(&line[18]); continue;}
        
	  // Info for Z scale in the image (this is the same for all files)
	  if(strncmp(line, "\\@Sens. Zscan: V ",16) == 0)
	    { 
	      ZSens_V_to_nm = atof(&line[17]); 
	      continue;
	    } 
      
	  if(strncmp(line, "\\@1:Z limit: V [Sens. Zscan] (",30) == 0)
	    {
	      ZDAtoVolt = atof(&line[30]);
	      Z_limit_V = atof(&line[49]);
	      continue;
	    }
      
	  //if(strncmp(line, "\\@1:Z limit: V [Sens. Zscan] (",30) == 0) 
	  //{ ZDAtoVolt = atof(&line[30]); continue;} //Settings on D/A converter: V per LSB (usually 0.006731... = 440/65535)
      
	  if (strncmp(line, "\\@2:Z scale: V [Sens.",10) == 0)
	    { 
	      char *p = line;
	      p = strchr(p, ')');
	      p += 2;
	      Z_scale_V[nimages] = atof(p); 
	      if (strncmp(&line[22], "Zscan", 5) == 0) 
		image_type[nimages] = 0;
	      else
		image_type[nimages] = 1;
	      fprintf(stderr, "%d=%s ", nimages-1, image_type[nimages] == 0 ? "topo" : "other");
	      continue;
	    }

	  // Where to find the binary data
	  if(strncmp(line, "\\Data offset: ",13) == 0)
	    { data_offset[nimages] = atoi(&line[13]); continue;}
	  if(strncmp(line, "\\Data length: ",13) == 0)
	    { 
	      data_length[nimages] = atoi(&line[13]); 
	      if ( data_length[nimages] > max_length) 
		max_length = data_length[nimages];
	      continue;
	  
	    }
      
	  // Image specific parameters

	  // Indicates start of new set of image parameters: use to count the number of images
	  if(strncmp(line, "\\*Ciao image list",16) == 0)
	    { nimages++; continue;}

	  // Info for X/Y size of the image
	  if(strncmp(line, "\\Scan size: ",11) == 0) // may be nm or microns, but should be obvious to the user
	    { image_size[nimages] = atof(&line[11]); continue;}

	  // This indicates we have reached the end of the header. The
	  // next line is the start of the binary data.
      
	  if(strncmp(line, "\\*File list end ",15) == 0) break;
	}
  
      int n; 
      char outname[256];
      char ibuf[2];
      char obuf[2];
      int m, i, j;
      int d1, d2;

      // 1024*1024*8*2 gives a segfault and no compiler warnings (probably
      // too big for the stack!), took me days to figure this out
      unsigned short *data;
      data = malloc(max_length);

      unsigned short tmp;

      // This is something that I don't understand: the scaling on the
      // nanoscope data just doesn't work like it should. The solution is
      // that for any given scanner, there is a fixed scaling factor that
      // gives us the correct height calibration, but this seems to be
      // completely unrelated to any of the information in the file
      // header.
  
      // This number was obtained by exporting both a "raw" and "nm" ascii
      // file from Nanoscope 5.12b and then performing a linear fit

      double scale_bits_to_nm;

      for (n=1; n<=nimages; n++)
	{
	  //fprintf(stderr, "ff %g\n", fudge_factor[n]);

	  fseek(input_fp, data_offset[n], SEEK_SET);
	  m = fread(data, 1, data_length[nimages], input_fp);
	  if (m != nlines*npixels*2)
	    fprintf(stderr, "Warning image %d: m %d, pix %d lin %d pix*lin %d \n", n, m, npixels, nlines, nlines*npixels*2);
      
	  for (i=0; i<nlines*npixels; i++)
	    data[i] = SWAP_2(data[i]-32767);
	  
	  // Swap the Y axis

	  for (i=0; i<npixels; i++)
	    for (j=0; j<nlines/2; j++)
	      {
		tmp = data[(nlines-j-1)*npixels+i];
		data[(nlines-j-1)*npixels+i] = data[j*npixels+i];
		data[j*npixels+i] = tmp;
	      }

	  snprintf(outname, 256, "%s.%d.pgm", fn, n-1);
	  if ((output_fp = fopen(outname, "wb")) == NULL)
	    {
	      fprintf(stderr, "Could not write file %s: %s\n", fn, strerror(errno));
	      usage();
	    }

	  fprintf(output_fp, "P5\n%d %d\n", npixels, nlines);

	  // The scaling should work like this:
	  // http://www.physics.arizona.edu/~smanne/DI/software/v43header.html
	  // However, it does not.

	  // I found the problem!!!! The problem is that DI is not
	  // updating the hard scale properly for each image after it does
	  // the post-acquisition scaling!

	  // However, we can figure out what the real hard scale is by
	  // looking at the Z_scale:

	  // real V/LSB = ZDAtoVolt * Z_scale_V / Z_limit_V

	  scale_bits_to_nm = ZSens_V_to_nm * ZDAtoVolt * Z_scale_V[n] / Z_limit_V;
	  if (image_type[n] == 0)
	    fprintf(output_fp, "#zmin %e\n#zmax %e\n#zunit nm\n", -32768*scale_bits_to_nm, 32767*scale_bits_to_nm);
	  else // just output in raw voltage value (20V range for amplitude)
	    fprintf(output_fp, "#zmin -10\n#zmax 10\n#zunit Voltage (V)\n");

	  fprintf(output_fp, "#xmin 0\n#xmax %e\n#ymin 0\n#ymax %e\n", image_size[n], image_size[n]);
	  fprintf(output_fp, "#xunit Mircons\n#yunit Microns\n");
	  fprintf(output_fp, "65535\n");
	  fwrite(data, data_length[nimages], 1,output_fp);
	  fclose(output_fp);
	}
      fprintf(stderr, "done.\n");
      fclose(input_fp);
    }
}

