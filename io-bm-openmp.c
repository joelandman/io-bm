#include <stdio.h>
#include <math.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/fcntl.h>

#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <string.h>
#include <malloc.h>
#include <error.h>
#include <errno.h>
#include <sys/mman.h>

#define NUMBER_OF_CALIPER_POINTS 10
#define NUMBER_OF_SLICES 4

#define KB (1024)
#define MB (1024*1024)
#define GB (1024*1024*1024)
#define TB (1024*1024*1024*1024)
#define PB (1024*1024*1024*1024*1024)


#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE


#include <omp.h>

struct timeval ti,tf,caliper[NUMBER_OF_CALIPER_POINTS],
               ttot[NUMBER_OF_CALIPER_POINTS];
struct timezone tzi,tzf;
void swap_arrays (void *array1, void *array2);




int main(int argc, char **argv)
  {
    int tid,nthreads;
    int i,j,k,milestone,rc,i_len,p,h;
    long N, buffers_per_file,bytes_read,bytes_written,N_file_ops;
    long bytes_of_io,offset,buffer_size,buffer_allocation;
    int true,false,ret,flags,target_tid,pagesize;
    int binary,same_file,all,read_flag,write_flag,direct,mmaped,THR;
    double *f,*buffer,*rec_buffer,total,sum,psum,delta_t,tsum,B;
    double *real_buffer,*real_rec_buffer,dt_max,B_sum;
    char *cpu_name,c,*filename,*fn;
    int file_descriptor,err,mode,sync,mmap_flags,mmap_result,debug;
    int verbose;
    struct stat sb,*buf;
    int status;
    void *mmap_addr,*vbuffer;

// hardwired for now, will do something elegant later
    const char top[] = "/data";
    printf("starting parallel region\n");



    filename	= (char *)calloc(1024,sizeof(char ));

    tsum		= 0.0;
    psum		= 0.0;
    sum			= 0.0;


    true         = (1==1);
    false		     = (1==0);
    binary       = true;
    same_file    = false;
    all          = false;
    read_flag    = false;
    write_flag   = false;
    direct		   = false;
    mmaped		   = false;
    debug		     = false;
    verbose		   = false;
    sync		     = true;
    buffer_size	 = 16 * MB; /* 16 MB buffer as default */
    ret		       = 0;
    N_file_ops	 = 0;
    THR          = -1;


   if (debug) printf("parsing args\n");

    while ((c = getopt(argc, argv, "n:f:srwb:dmzDvp:h:t:")) != -1)
       {
      	switch (c)
      	 {
      	  case 'n':
              	   N = (long)atol((char *)optarg);
                   if (debug) printf(" N=%iGB of IO\n",(int)N);
                   // size in GB for IO
              	   break;
          case 't':
              	   THR = (long)atol((char *)optarg);
                   if (debug) printf(" Threads=%i\n",(int)THR);
                   // size in GB for IO
              	   break;
          case 'p':
                   p = (int)atoi((char *)optarg);
                   // which disk index to start with for FILENAME/INDEX
                   if (debug) printf(" p=%i disk index start\n",(int)p);
              	   break;
          case 'h':
                   h = (int)atoi((char *)optarg);
                   // highest disk index for FILENAME/INDEX , after which we have to wrap
                   if (debug) printf(" h=%i disk index max\n",(int)h);
              	   break;

      	  case 'b':
              	   buffer_size = (long)atol((char *)optarg);
      		         if (buffer_size < 1) buffer_size = 16 * MB;
      	    	     if (buffer_size > (1024*MB)) buffer_size = 16 * MB;
      		         if (buffer_size < 1025) buffer_size *= MB;
                   if (debug) printf(" buffer_size=%i MB\n",(int)buffer_size);
              	   break;
      	  case 'f':
              	   filename = (char *)optarg;
      		         i_len=strlen(filename);
                   printf(" filename=%s\n",filename);
              	   break;
      	  case 'a':
              	   all		= true;
              	   break;
      	  case 'r':
              	   read_flag		= true;
                   printf(" read_flag set\n");
              	   break;
      	  case 'w':
              	   write_flag	= true;
                   printf(" write_flag set\n");
              	   break;
      	  case 'd':
              	   direct	= true;
                   printf(" direct IO set\n");
              	   break;
      	  case 'D':
              	   debug	= true;
      		         verbose	= true;
              	   break;
      	  case 'v':
      		         verbose	= true;
      		         break;
      	  case 's':
              	   same_file	= true;
              	   break;
      	  case 'z':
              	   sync		= false;
              	   break;
      	 }
       }

gettimeofday(&ttot[0],&tzf);

if (THR==-1) THR=1;
omp_set_num_threads(THR);

#pragma omp parallel default(shared) private(fn,buffer,tid,tsum,psum,sum,buffer_allocation,buffers_per_file,pagesize,vbuffer,rec_buffer,caliper,nthreads,file_descriptor,milestone,bytes_of_io,delta_t,B)
  {

     tid     = omp_get_thread_num();
    /* get the machine name */
    //gethostname(cpu_name,80);

      //struct utsname  uts;
      //uname( &uts );
      //cpu_name	= uts.nodename;
      nthreads = omp_get_num_threads();
      if ((tid==0) & debug) printf("N(threads)=%i\n",nthreads);
      //if (verbose) printf("thread %i of %i\n",tid,nthreads);

    if (all == true)
       {
         if ((tid == 0 ) && (verbose))
              printf("each thread will output %i gigabytes\n",(int)N);
	       bytes_of_io	= GB*N;
       }
      else
       {
      	 if ((tid == 0 ) && (verbose)) printf("N=%i gigabytes will be written in total\n",(int)N);
      	 bytes_of_io    = (long) (GB*(double)N/(double)nthreads);
         if ((tid == 0 ) && (verbose)) printf("each thread will output %-.3f gigabytes\n",((double)N/(double)nthreads));
       }
    if (direct == true)
       {
         if (debug) printf("[tid=%i] using direct IO\n",tid);
       }
      else
       {
         if (debug) printf("[tid=%i] using buffered IO\n",tid);
       }

    if ((read_flag == true) && (write_flag == true))
       {
        printf("FATAL ERROR: unable to read and write together ...\n");
        exit (2);
       }
    if ((read_flag == false) && (write_flag == false))
       {
        printf("FATAL ERROR: must use read (-r) OR write (-w) for test ...\n");
        exit (2);
       }

    /* set the file name, and amount to read/write for each thread */
    fn			= (char *)calloc(1024,sizeof(char ));
    if (same_file == false)
       {
         sprintf(fn,"%s/%i/%s.%i",filename,tid,filename,tid);
	       offset=0;
       }
      else
       {
         sprintf(fn,"%s",filename);
	       offset	= bytes_of_io*tid;
       }

    /* allocate buffer memory */
#pragma omp barrier

    i_len=strlen(filename);
    buffer_allocation	= (long)buffer_size/sizeof(double);
    buffers_per_file	= (long)bytes_of_io/buffer_size;

    pagesize=getpagesize();
    if ( (tid == 0) && (verbose)) printf("[tid=%i] page size                     ... %i bytes \n",tid,pagesize);
    if ( (tid == 0) && (verbose)) printf("[tid=%i] number of elements per buffer ... %i  \n",tid,(int)buffer_allocation);
    if ( (tid == 0) && (verbose)) printf("[tid=%i] number of buffers per file    ... %i  \n",tid,(int)buffers_per_file);
    //if ( (verbose)) printf("[tid=%i] offset = %li  \n",tid,offset);

    if (debug) printf("[tid=%i] Allocating memory ... %i bytes \n",tid,(int)buffer_size);
    buffer	= memalign(pagesize,buffer_size);
    vbuffer	= buffer;
    rec_buffer	= memalign(pagesize,buffer_size);
    if (buffer==NULL)
       {
        perror("buffer allocation error: ");
        exit(5);
       }
    if (rec_buffer==NULL)
       {
        perror("buffer allocation error: ");
        exit(5);
       }

    if (debug) printf("[tid=%i] Done allocating memory: size=%li bytes \n",tid,buffer_size);

    if (buffer == NULL)
      {
        printf("FATAL ERROR: unable to allocate %i units of %i bytes per unit for the buffer\n",
		          (int)buffer_allocation,(int)sizeof(double));
        exit (3);
	/* really should send a signal back to the tid=0 process and let it
	   shut things down gracefully  ... this may leak memory in
	   MPI stacks that don't do a good job of releasing resources
	   after a crash (MPICH) */
      }

    /* store random numbers in each buffer */
    if ( (tid == 0) && (debug)) printf("[tid=%i] storing random numbers ...\n",tid);
    for( i=0; i<buffer_allocation; i++) { buffer[i]=(double)rand()/(double)RAND_MAX; }
    if ( (tid == 0) && (debug)) printf("[tid=%i] Done storing random numbers \n",tid);

   if (write_flag == true)
      {
       flags  = O_CREAT | O_LARGEFILE |O_WRONLY ;
       mode	  =  S_IRWXU;
       if (direct == true) { flags |= O_DIRECT ; }
       if (same_file == false)  { flags |= O_TRUNC ; }
       if (debug) printf("[tid=%i] opening file=%s for writing\n",tid,fn);
       file_descriptor = open(fn,flags,mode);

       //if ( verbose ) printf("[tid=%i] file name = %s\n",tid,fn);

       if (file_descriptor == -1)
          {
           char *msg = (char *)calloc(1024,sizeof(char ));
           sprintf(msg,"[tid=%i] failed to open file=%s\n",tid,fn);
	         perror(msg);
	         exit (1);
      	  }
    	 else
    	  {
    	    if (debug) printf("[tid=%i] file descriptor = %i\n",tid,file_descriptor);
    	  }

       /*status=fstat(file_descriptor,buf);*/
       lseek(file_descriptor,offset,SEEK_SET);
      }

   if (read_flag == true)
      {
       flags	= O_RDONLY;
       if (direct == true) { flags |= O_DIRECT ; }
       if (debug) printf("[tid=%i] opening file=%s for reading\n",tid,fn);
       file_descriptor = open(fn,flags)	;
       if (same_file == true)  { lseek(file_descriptor,offset,SEEK_SET); }
      }

   if (debug) printf("[tid=%i] file open for file=%s is complete\n",tid,fn);


   milestone	= 0;
   if (debug) printf("[tid=%i] setting milestone=0 time\n",tid);
   gettimeofday(&caliper[milestone],&tzf);
   if (debug) printf("[tid=%i] milestone=0 time set\n",tid);

   for(i=0;i<buffers_per_file;i++)
      {
       if (debug) printf("[tid=%i]writing buffer=%i\n",tid,i);
       if (write_flag == true)
          {
        	  if (debug) printf("[tid=%i] writing buffer\n",tid);
        	  bytes_written = write(file_descriptor,buffer,buffer_size);
        	  if (debug) printf("[tid=%i] done writing buffer, %i bytes written\n",tid,(int)bytes_written);

        	  if (bytes_written == -1)
        	    {
        	      perror("failed to write buffer: ");
        	    }
        	   /* roll buffers around a ring  */

           // you can put real computation here
	         //swap_arrays((double *)buffer, (double *)rec_buffer);
	       }

       if (read_flag == true)
          {
	          bytes_read = (long) read(file_descriptor,buffer,buffer_size);

	          sum=0.0;
	          psum=0.0;

            // do some computing specifically to prevent optimizer from
            // making this go away
	          for(j=0;j<(bytes_read/sizeof(double)); j++) { sum += buffer[j]; }
            tsum+=sum;
	          if (tsum > (double)GB) {tsum /= (double)GB;}
	        }
       }


   /* handle the last write/read operation */

   if (write_flag == true) { write(file_descriptor,buffer,bytes_of_io-buffers_per_file*buffer_size); }
   if (read_flag  == true) { read(file_descriptor,buffer,bytes_of_io-buffers_per_file*buffer_size);  }

   milestone++;
   gettimeofday(&caliper[milestone],&tzf);
   close(file_descriptor);


   /* now report the milestone time differences */
   for (i=0;i<=(milestone-1);i++)
    {

      delta_t = (double)(caliper[i+1].tv_sec-caliper[i].tv_sec);
      delta_t += (double)(caliper[i+1].tv_usec-caliper[i].tv_usec)/1000000.0;
      B	= (double) bytes_of_io/delta_t;
      B /= (double) MB;
      printf("Thread=%03i: total IO = %li MB , time = %-.3f s IO bandwidth = %-.3f MB/s\n",
        tid, bytes_of_io/MB, delta_t, B);
    }
    //free(fn);
    //free(buffer);
    //free(vbuffer);
    //free(rec_buffer);
 }

gettimeofday(&ttot[1],&tzf);
delta_t  = (double)(ttot[1].tv_sec-ttot[0].tv_sec);
delta_t += (double)(ttot[1].tv_usec-ttot[0].tv_usec)/1000000.0;
B	= (double) N*GB/delta_t;
B /= (double) MB;

printf("Total time = %-.3f s, B = %-.3f MB/s\n",delta_t,B);
return 0;

}

void swap_arrays (void *array1, void *array2)
 {
   void         *temp;
   temp         = array1;
   array1       = array2;
   array2       = temp;
 }
