#!/bin/bash

#NP=72
#BUF=128
#GB=2048
NP=4
BUF=4
GB=1
mpirun -np $NP  io-bmi-mpi -n $GB -w -f big -b $BUF 

