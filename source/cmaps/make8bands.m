x=[0:255]; 
%b = (1-abs(cos(2*pi/256*x*4)))*255;
b = (cos(2*pi/255*x*8-pi)+1)*255/2;
r = b; 
g = b;
fp = fopen('test.ppm', 'w+');
fprintf(fp, 'P3\n1 256\n255\n');
for m = 1:256
  fprintf(fp, '%d %d %d\n', round(r(m)), round(g(m)), round(b(m)));
end
fclose(fp);
plot(b);