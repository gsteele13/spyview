#!/usr/bin/zsh

file1=$1
file2=$2
out=`basename $1 .ps`.`basename $2 .ps`.ps

cat $file1 | awk '
BEGIN {do_print = 1}
/showpage/ {do_print = 0}
// {if (do_print) print $0}' > $out

cat $file2 | awk '
BEGIN {do_print = 0}
/imgstr/ {do_print = 1}
// {if (do_print) print $0}' >> $out 



