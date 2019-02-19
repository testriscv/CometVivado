#!/bin/sh

SUBFOLDERS=$(ls -d */)
COMETSIM=../../build/bin/comet.sim

for TEST in $SUBFOLDERS
do
  echo "started test : " $TEST
  cd $TEST
  $COMETSIM -f *.riscv* > testOutput
  DIFF=$(diff testOutput expectedOutput)
  if [ "$DIFF" != "" ]
  then
    #This is ugly
    diff testOutput expectedOutput
    rm testOutput
    exit 1
  fi
  rm testOutput
  cd ..
done
