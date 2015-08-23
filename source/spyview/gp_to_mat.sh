#!/bin/bash
if [ $1 == "-h" ]
then
echo "gp_to_mat.sh column_number < file.gp > file.mat"
exit
fi

gawk '/^$/ {printf("\n");} $0 !~ /\#/ { printf("%s ", $'$1'); }' 
