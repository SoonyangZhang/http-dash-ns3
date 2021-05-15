#! /bin/sh
time=1797078517
file=${time}_reward_and_entropy.txt
output=reward-entropy
gnuplot<<!
set xlabel "epoch" 
set ylabel "reward"
set y2label "entropy"
set xrange [0:50000]
set yrange [0:2]
set y2range [0:2]
set y2tics
set ytics nomirror 
set format x "%.0f"
set term "png"
set output "${output}.png"
plot "${file}" u 1:2 title "average reward" with lines lw 2 axis x1y1,\
"${file}" u 1:3 title "average entropy" with lines lw 2  axis x1y2
set output
exit
!
