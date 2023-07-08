#!/bin/bash
#cd ../
SUBDIRS="include src "
FILETYPES="*.hpp *.cpp"
ASTYLE="astyle -A8 -c -s4 -xV -xn -xt4 -w -Y -p -U -xe -k3 -W3 -j -xg "
for d in ${SUBDIRS}
do
  for t in ${FILETYPES}
  do
    for file in $d/$t
    do
      if test -f $file
      then
        ${ASTYLE} $file 
        rm -f ${file}.orig
      fi
    done
  done
done
