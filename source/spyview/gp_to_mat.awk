#!/usr/bin/gawk -f
BEGIN {
  x_increasing=0;
  y_increasing=0;
  if(ARGC > 3)
    {
      xmode=1;
      xcol=ARGV[1];
      ycol=ARGV[2];
      zcol=ARGV[3];
      ARGV[1] = "";
      ARGV[2] = "";
      ARGV[3] = "";
      x_increasing=-1;
      y_increasing=-1;
    }
  else if(ARGC > 1)
    {
      zcol=ARGV[1];
      ARGV[1] = "";
      xmode=2;
    }
  else
    {
      zcol=1;
      xmode=2;
    }
}
/^$/ {printf("\n");}
/^[^#]+$/ { 
  if(xmode==1)
    {
      xmode=0;
      xmin=$xcol;
      xmax=$xcol;
      ymin=$ycol;
      ymax=$ycol;
    }
  if(xmode==0)
    {
      if(x_increasing == -1)
	{
	  if($xcol > xmin)
	    x_increasing = 1;
	  else if($xcol < xmin)
	    x_increasing = 0;
	}
      if(y_increasing == -1)
	{
	  if($ycol > ymin)
	    y_increasing = 1;
	  else if($ycol < ymin)
	    y_increasing = 0;
	}
      if($xcol < xmin)
	xmin = $xcol;
      if($xcol > xmax)
	xmax = $xcol;
      if($ycol < ymin)
	ymin = $ycol;
      if($ycol > ymax)
	ymax = $ycol;
    }
  if(zcol > 0)
    printf("%s ", $zcol); 
  else
    printf("%g ", z());
}

END {
  if(xmode == 0)
    {
      printf("\n");
      if(x_increasing > 0)
	{
	  printf("#xmin %f\n", xmin);
	  printf("#xmax %f\n", xmax);
	}
      else
	{
	  printf("#xmin %f\n", xmax);
	  printf("#xmax %f\n", xmin);
	}
      if(y_increasing > 0)
	{
	  printf("#ymin %f\n", ymax);
	  printf("#ymax %f\n", ymin);
	}
      else
	{
	  printf("#ymin %f\n", ymin);
	  printf("#ymax %f\n", ymax);
	}
    }
}
