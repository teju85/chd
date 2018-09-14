#!/bin/bash
keysFile=$1
for l in `seq 1 10`; do
    ./chd -l $l -a 0.5 $keysFile
done
