file=$1
cat <<EOF
set st d lp
plot "< tcut -s 1 $file.ppm" u 0:1, "" u 0:2, "" u 0:3
replot "$file.control_points" i 0, "" i 1, "" i 2
EOF
