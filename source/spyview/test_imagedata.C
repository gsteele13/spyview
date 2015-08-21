#include "ImageData.H"

int main(int argc, char **argv)
{
  ImageData id;

  // Input file support:
  // pgm: works
  // dat: works
  // mtx: works
  // gp, columns: works
  // gp, index: 

  //id.load_file(argv[1]);

  id.load_Delft(argv[1]);
  //id.load_mtx_cut(5,YZ);

  //fprintf(stderr, "loaded %d %d %d %d\n", id.mtx.data_loaded, 
  //id.mtx.size[0], id.mtx.size[1], id.mtx.size[2]);
  
  //  id.mtx_cut_type = YZ;
  //id.mtx_index = 0;
  //id.load_mtx_cut();

  //fprintf(stderr, "w h %d %d\n", id.width, id.height);

  // Debugged functions
  
  //id.offset(1.5);
  //id.scale(1e5);
  //id.log10();
  //id.gamma(1./5);
  //id.lbl(10,10,1);                   
  //id.cbc(0,0);
  //id.sub_linecut(0,50);
  //id.norm_lbl();
  //id.norm_cbc();
  //id.fitplane();
  //id.plane(1,0);
  //id.xflip();
  //id.yflip();
  //id.rotate_cw();
  //id.rotate_ccw();
  //id.yderv();
  //id.yderv();
  //id.lowpass(3,3);
  //id.pixel_average(3,3);

//   id.quantize();


   for (int j=0; j<id.height; j++)
     {
       for (int i=0; i<id.width; i++)
 	fprintf(stdout, "%e ", id.raw(i,j));
       //fprintf(stdout, "%d ", id.quant(i,j));
       fprintf(stdout, "\n");
     }
}
