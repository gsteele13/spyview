x=[0:255]; 

x1=0; y1=0; x2=162; y2=127; a=(y2-y1)/(x2-x1); b=(y2-a*x2);
l1 = a*x+b;

x1=0; y1=0; x2=162; y2=194; a=(y2-y1)/(x2-x1); b=(y2-a*x2);
l2 = a*x+b;

x1=162; y1=127; x2=223; y2=230; a=(y2-y1)/(x2-x1); b=(y2-a*x2);
l3 = a*x+b; 

x1=162; y1=194; x2=255; y2=255; a=(y2-y1)/(x2-x1); b=(y2-a*x2);
l4 = a*x+b;

x1=223; y1=178; x2=255; y2=255; a=(y2-y1)/(x2-x1); b=(y2-a*x2);
l5 = a*x+b;


r = (x<162) .* l2;
g = (x<162) .* l1;
b = (x<222) .* l1;

g += (x>=162).*(x<226) .* l3;

r += (x>=162) .* l4;
g += (x>=226) .* l4;
b += (x>=222) .* l5;

fp = fopen('matlabpink.ppm', 'w+');
fprintf(fp, 'P3\n1 256\n255\n');
for m = 1:256
  fprintf(fp, '%d %d %d\n', round(r(m)), round(g(m)), round(b(m)));
end
fclose(fp);
