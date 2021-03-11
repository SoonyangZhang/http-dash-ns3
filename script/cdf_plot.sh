#!/bin/bash
algo1=ppo
algo2=festive
algo3=panda
algo4=tobasco
algo5=osmp
algo6=raahs
algo7=fdash
algo8=sftm
algo9=svaa
output=reward1
format=pdf
gnuplot<<!
set grid
set key right bottom
set xlabel "r" 
set ylabel "CDF"
set xrange [-1:4]
set yrange [0:1.0]
set term "${format}"
set output "${output}-cdf.${format}"
plot "${algo1}.txt" u 2:3 title "${algo1}" with lines lw 2 lc 0,\
"${algo2}.txt" u 2:3 title "${algo2}" with lines lw 2 lc 1,\
"${algo3}.txt" u 2:3 title "${algo3}" with linespoints pt 6 lw 2 lc 2,\
"${algo4}.txt" u 2:3 title "${algo4}" with lines lw 2 lc 3,\
"${algo5}.txt" u 2:3 title "${algo5}" with lines lw 2 lc 4,\
"${algo6}.txt" u 2:3 title "${algo6}" with lines lw 2 lc 5,\
"${algo7}.txt" u 2:3 title "${algo7}" with lines lw 2 lc 6,\
"${algo8}.txt" u 2:3 title "${algo8}" with lines lw 2 lc 7,\
"${algo9}.txt" u 2:3 title "${algo9}" with lines lw 2 lc 10
set output
exit
!
output=reward2
gnuplot<<!
set grid
set key right bottom
set xlabel "r" 
set ylabel "CDF"
set xrange [-1:4]
set yrange [0:1.0]
set term "${format}"
set output "${output}-cdf.${format}"
plot "${algo1}.txt" u 2:3 title "${algo1}" with lines lw 2 lc 0,\
"${algo9}.txt" u 2:3 title "${algo9}" with lines lw 2 lc 10
set output
exit
!
