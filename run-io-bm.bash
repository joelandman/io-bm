#!/bin/bash

cd /root/build/io-bm
MPI=/opt/openmpi/1.8.1/
NP=72
BUF=128
GB=2048
$MPI/bin/mpirun --allow-run-as-root -np $NP -hostfile hosts /root/build/io-bm/io-bm.exe -n $GB -w -f /mnt/fhgfs/test/out -b $BUF 

