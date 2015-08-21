x=[0:255]; 
g = (x<128) .* (2*x)  + (x >= 128) .* (2*255 - 2*x); 
b = (x<128) .* (255-2*x) ;
r = (x>=128) .* (2*x - 255);
b = (x<128) .* (255-2*x) + (x>=128) .* (sqrt(255^2 - r.^2 - g.^2));
r = (x<128) .* (sqrt(255^2-g.^2-b.^2)) + (x>=128) .* (2*x - 255);
b = (x<128) .* (255-2*x) + (x>=128) .* (sqrt(255^2 - r.^2 - g.^2));
fp = fopen('aaron.ppm', 'w+');
fprintf(fp, 'P3\n1 256\n255\n');
for m = 1:256
  fprintf(fp, '%d %d %d\n', round(r(m)), round(g(m)), round(b(m)));
end
fclose(fp);
