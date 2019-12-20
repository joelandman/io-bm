PROGRAM	= io-bm

# adjust this to point to where your MPI lives ...
###################
MPICC	= ${MPI}mpicc
CC 	= gcc
CFLAGS  = -g -fopenmp
LFLAGS  = -g -fopenmp
#
###################

all:	${PROGRAM}-mpi ${PROGRAM}-openmp


${PROGRAM}-openmp:	${PROGRAM}-openmp.c
	$(CC) ${CFLAGS} ${LFLAGS} -o ${PROGRAM}-openmp ${PROGRAM}-openmp.c -lm

${PROGRAM}-mpi:      ${PROGRAM}-mpi.c
	$(MPICC) ${CFLAGS} ${LFLAGS} -o ${PROGRAM}-mpi ${PROGRAM}-mpi.c -lm

clean:
	rm -f ${PROGRAM}-openmp  ${PROGRAM}-mpi


rebuild:
	$(MAKE) -f Makefile.${PROGRAM} clean
	$(MAKE) -f Makefile.${PROGRAM}

.c.o:
	$(CC) -c $(CFLAGS) $<
