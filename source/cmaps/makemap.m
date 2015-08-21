function makemap(x,name)
x = x*255;
r = x(:,1);
g = x(:,2);
b = x(:,3);
fp = fopen(name, 'w+');
fprintf(fp, 'P3\n1 256\n255\n');
for m = 1:256
  fprintf(fp, '%d %d %d\n', round(r(m)), round(g(m)), round(b(m)));
end
fclose(fp);