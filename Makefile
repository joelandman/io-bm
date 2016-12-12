PROGRAM	= io-bm

# adjust this to point to where your MPI lives ...
#MPI	= /apps/lam711/bin
#MPI	= /opt/mpich127p1/pgi/ch_shmem/bin
#MPI	= /opt/mpich/gnu/bin/
MPI	= /opt/openmpi/1.8.1/bin/
###################
CC	= ${MPI}mpicc
CFLAGS  =  -g
LFLAGS  =  -g
#
###################

all:	${PROGRAM}.exe


${PROGRAM}.exe:	${PROGRAM}.o
	$(CC) ${CFLAGS} ${LFLAGS} -o ${PROGRAM}.exe ${PROGRAM}.o -lm  

clean:	
	rm -f ${PROGRAM}.exe ${PROGRAM}.o 


rebuild:
	$(MAKE) -f Makefile.${PROGRAM} clean
	$(MAKE) -f Makefile.${PROGRAM}

.c.o:
	$(CC) -c $(CFLAGS) $<
