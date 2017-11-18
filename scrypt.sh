#!/bin/bash
unzip ./1rcvClient/1.2.txt -d ./2rcvServer/

destdir=./extracted_files.txt
files=$( ls ./2rcvServer/* )
counter=0
for i in $files ; do

    echo $i >> "$destdir"

done


