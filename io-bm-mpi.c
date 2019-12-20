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

#define _LARGEFILE64_SOURCE 
#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE

#define useMPI

#ifdef useMPI
#include "mpi.h"
#endif

struct timeval ti,tf,caliper[NUMBER_OF_CALIPER_POINTS];
struct timezone tzi,tzf;
void swap_arrays (void *array1, void *array2);

int main(int argc, char **argv)
  {
    int tid,nthreads;
    int i,j,k,milestone,rc,i_len;
    long N, buffers_per_file,bytes_read,bytes_written,N_file_ops;
    long bytes_of_io,offset,buffer_size,buffer_allocation;
    int true,false,ret,flags,target_tid,pagesize;
    int binary,same_file,all,read_flag,write_flag,direct,mmaped;
    double *f,*buffer,*rec_buffer,total,sum,psum,delta_t,tsum,B;
    double *real_buffer,*real_rec_buffer,dt_max,B_sum;
    char *cpu_name,c,*filename,*fn;    
    int file_descriptor,err,mode,sync,mmap_flags,mmap_result,debug;
    int verbose;
    struct stat sb,*buf;
    int status;
    void *mmap_addr,*vbuffer;
     
     
#ifdef useMPI 
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &tid);
    MPI_Comm_size(MPI_COMM_WORLD, &nthreads);    
    MPI_Status stat;   
#endif
#ifndef useMPI
    tid=0;
    nthreads=1;
#endif
    true		= (1==1);
    false		= (1==0);
    filename	= (char *)calloc(1024,sizeof(char ));
    fn			= (char *)calloc(1024,sizeof(char ));
    binary		= true;
    same_file		= false;
    all			= false;
    read_flag		= false;
    write_flag		= false;
    direct		= false;
    mmaped		= false;
    debug		= false;
    verbose		= false;
    sync		= true;
    buffer_size		= 16 * MB; /* 16 MB buffer as default */
    ret		= 0;
    tsum		= 0.0;
    psum		= 0.0;
    sum			= 0.0;
    N_file_ops		= 0;
        
    while ((c = getopt(argc, argv, "n:f:srwb:dmzDv")) != -1)
       {
	switch (c)
	 {
	  case 'n':
        	   N = (long)atol((char *)optarg);        	  
        	   break;	  
	  case 'b':
        	   buffer_size = (long)atol((char *)optarg);
		   if (buffer_size < 1) buffer_size = 16 * MB;
	    	   if (buffer_size > (1024*MB)) buffer_size = 16 * MB;
		   if (buffer_size < 1025) buffer_size *= MB;
        	   break;	  
	  case 'f':
        	   filename = (char *)optarg;
		   i_len=strlen(filename);
        	   break;
	  case 'a':
        	   all		= true;
        	   break;
	  case 'r':
        	   read_flag		= true;
        	   break;
	  case 'w':
        	   write_flag	= true;
        	   break;
	  case 'd':
        	   direct	= true;
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
	  case 'm':
        	   mmaped	= true;
        	   break;
	  case 'z':
        	   sync		= false;
        	   break;
	 }
       }

    /* get the machine name */
    //gethostname(cpu_name,80); 
    struct utsname  uts;
    uname( &uts );
    cpu_name	= uts.nodename;

    if (all == true)
       {
         if ((tid == 0 ) && (verbose)) printf("each thread will output %i gigabytes\n",N);
	 bytes_of_io	= GB*N;
       }
      else
       { 
	 if ((tid == 0 ) && (verbose)) printf("N=%i gigabytes will be written in total\n",N);
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
    if (same_file == false)
       {
         sprintf(fn,"%s.%i",filename,tid);
	 offset=0;	 
       }
      else
       {
         sprintf(fn,"%s",filename);	       
	 offset	= bytes_of_io*tid;
       }

    /* adjust sync flags */
    if (sync == true)
       {
         mmap_flags	= MS_SYNC;
       }    
      else
       {
         mmap_flags	= MS_ASYNC;
       }

    /* allocate buffer memory */

    i_len=strlen(filename);
    buffer_allocation	= (long)buffer_size/sizeof(double);
    buffers_per_file	= (long)bytes_of_io/buffer_size;
    pagesize=getpagesize();
    if ( (tid == 0) && (verbose)) printf("page size                     ... %i bytes \n",pagesize);
    if ( (tid == 0) && (verbose)) printf("number of elements per buffer ... %i  \n",buffer_allocation);
    if ( (tid == 0) && (verbose)) printf("number of buffers per file    ... %i  \n",buffers_per_file);
    if ( (verbose)) printf("[tid=%i] offset = %li  \n",tid,offset);    
     
    if (debug) printf("[tid=%i] Allocating memory ... %i bytes \n",tid,buffer_size);
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
		buffer_allocation,sizeof(double));
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
    
   target_tid=tid+1;
   if (target_tid >= nthreads) {target_tid = 0;}

   if (write_flag == true)
      {
       
       flags			= O_CREAT | O_LARGEFILE |O_WRONLY ;
       if (mmaped) flags	= O_CREAT | O_LARGEFILE |O_RDWR | O_TRUNC ;

       mode	=  S_IRWXU;
       if (direct == true) { flags |= O_DIRECT ; }
       if (same_file == false)  { flags |= O_TRUNC ; }
       if (debug) printf("[tid=%i] opening file=%s for writing\n",tid,fn);
       file_descriptor = open(fn,flags,mode);
       
       if ( verbose ) printf("[tid=%i] file name = %s\n",tid,fn);       
 
       if (file_descriptor == -1)
          {
	    perror("failed to open file: ");
	    exit (1);
	  }
	 else
	  {
	    if (debug) printf("[tid=%i] file descriptor = %i\n",tid,file_descriptor);
	  }
       
       /*status=fstat(file_descriptor,buf);*/
       
       if (mmaped == true)
          { 
	    if (debug) printf("[tid=%i] setting up memory map for write\n",tid);
	    mmap_addr = mmap((caddr_t)0, (size_t)buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, (off_t)0);
	    if (mmap_addr == MAP_FAILED)
	       {
	         printf("FATAL ERROR: unable to memory map file for writing\n");
		 switch (errno)
		    {
		      case EACCES:
		      		  printf("FATAL ERROR: The fildes argument is not open for read, regardless of the protection specified, or  fildes  is  not open for write and PROT_WRITE was specified for a MAP_SHARED type mapping.\n");       	  
        	      break;	  
  		      case EAGAIN:
		      		  printf("FATAL ERROR:  The mapping could not be locked in memory, if required by mlockall(), due to a lack of resources.\n");       	  
        	      break;	  
  		      case EBADF :
		      		  printf("FATAL ERROR:  The fildes argument is not a valid open file descriptor. \n");       	  
        	      break;	  
  		      case EINVAL  :
		      		  printf("FATAL ERROR:   The  addr argument (if MAP_FIXED was specified) or off is not a multiple of the page size as returned  by sysconf(), or is considered invalid by the implementation. \n");       	  
        	      break;
  		       
		    }
		 exit (4);
	       }	     
	    if (debug) printf("[tid=%i] memory map setup complete for write\n",tid);
	  }
       lseek(file_descriptor,offset,SEEK_SET);

      }

   if (read_flag == true)
      {
       flags	= O_RDONLY;
       if (direct == true) { flags |= O_DIRECT ; }
       if (debug) printf("[tid=%i] opening file=%s for reading\n",tid,fn);
       file_descriptor = open(fn,flags)	;
       if (same_file == true)  { lseek(file_descriptor,offset,SEEK_SET); }
       if (mmaped == true)
          { 
	    mmap_addr = mmap(buffer, buffer_size, PROT_READ, MAP_SHARED, file_descriptor, 0);	   
	    if (mmap_addr == MAP_FAILED)
	       {
	         printf("FATAL ERROR: unable to memory map file for reading\n");
		 exit (4);
	       }	     
	  }

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
	   if (mmaped == false)
	      {
	        if (debug) printf("[tid=%i] writing buffer\n",tid);
	        bytes_written = write(file_descriptor,buffer,buffer_size);
	        if (debug) printf("[tid=%i] done writing buffer, %i bytes written\n",tid,bytes_written);
              }
	     else
	      {
	        if (debug) printf("[tid=%i] copying buffer to mmaped file descriptor=%i\n",tid,file_descriptor);
		memcpy( buffer,  mmap_addr,8 );  
		
	        
	        if (debug) printf("[tid=%i] done copying buffer to mmaped file descriptor\n",tid);
	        if (debug) printf("[tid=%i] syncing mmaped file descriptor\n",tid);
		mmap_result = msync(mmap_addr,(size_t)buffer_size,mmap_flags);
	        if (debug) printf("[tid=%i] done syncing mmaped file descriptor\n",tid);
		if (mmap_result == 0)
		   {
		     bytes_written = buffer_size;
		     N_file_ops++;
		     mmap_addr = mmap(buffer, (size_t)buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, (off_t)(N_file_ops*buffer_size) ) ;  
		     if (mmap_addr == MAP_FAILED)
			{
	        	  printf("FATAL ERROR: unable to set next memory map buffer offset for writing\n");
			  exit (5);
			}
		   }
		   else
		   {
		     bytes_written = -1;
		   }
	      }
	   if (bytes_written == -1)
	      { 	        
	        perror("failed to write buffer: ");		
	      }
	   /* roll buffers around a ring  */
#ifdef useMPI
	   if (debug) printf("[tid=%i] rolling buffers\n",tid);
	   if (tid==0)
              {
	       if (debug) printf("[tid=%i] MPI: posting send ... \n",tid);
	       // MPI_Send(buffer,buffer_allocation,MPI_DOUBLE,target_tid,i*(tid+1)+tid,MPI_COMM_WORLD);
	       if (debug) printf("[tid=%i] MPI: send complete, posting receive ... \n",tid);
	       // MPI_Recv(rec_buffer,buffer_allocation,MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,&stat); 	       
	       if (debug) printf("[tid=%i] MPI: recieve  \n",tid);

	      }
	     else
	      {
	       if (debug) printf("[tid=%i] MPI: posting receive ... \n",tid);
	       // MPI_Recv(rec_buffer,buffer_allocation,MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,&stat); 
	       if (debug) printf("[tid=%i] MPI: receive complete, posting send ... \n",tid);
	       // MPI_Send(buffer,buffer_allocation,MPI_DOUBLE,target_tid,i*(tid+1)+tid,MPI_COMM_WORLD);	
	       if (debug) printf("[tid=%i] MPI: send complete \n",tid);
         
	      }
	   if (debug) printf("[tid=%i] done rolling buffers\n",tid);
	   if (debug) printf("[tid=%i] swapping buffers for use\n",tid);
	    swap_arrays((double *)buffer, (double *)rec_buffer);
	   if (debug) printf("[tid=%i] done swapping buffers for use\n",tid);
#endif
	  }

       if (read_flag == true) 
          {        
	   if (mmaped == false)
	      {
	        bytes_read = (long) read(file_descriptor,buffer,buffer_size);
	      }
	     else
	      {
	        memcpy(buffer, mmap_addr,(size_t)buffer_size);		
		N_file_ops++;
		mmap_addr = mmap(0, buffer_size, PROT_READ, MAP_SHARED, file_descriptor, N_file_ops*buffer_size);
		if (mmap_addr == MAP_FAILED)
		   {
		     bytes_read	= 0;
	             printf("FATAL ERROR: unable to set next memory map buffer offset for writing\n");
		     exit (5);
		   }
		   else
		   {
		     bytes_read	= buffer_size;
		   }		
	      }
	   sum=0.0;
	   psum=0.0;
	   for(j=0;j<(bytes_read/sizeof(double)); j++) { sum += buffer[j]; }
#ifdef useMPI
	   MPI_Reduce(&psum,&sum,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
	   tsum+=psum;
#endif
#ifndef useMPI
	   tsum+=sum;
#endif
	   if (tsum > (double)GB) {tsum /= (double)GB;}
	  }	  
       }
   
   
   /* handle the last write/read operation */
   if (mmaped == false) 
      {
   
       if (write_flag == true) { write(file_descriptor,buffer,bytes_of_io-buffers_per_file*buffer_size); }
       if (read_flag  == true) { read(file_descriptor,buffer,bytes_of_io-buffers_per_file*buffer_size);  } 
      }
   
   
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
      printf("Thread=%05i: host=%s time = %-.3f s IO bandwidth = %-.3f MB/s\n",tid, cpu_name, delta_t, B);
    }

  
#ifdef useMPI 
   MPI_Barrier ( MPI_COMM_WORLD );
   MPI_Reduce(&B,&B_sum,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
   MPI_Reduce(&delta_t,&dt_max,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
   
   // get hostnames from all nodes and put into hosts array, indexed by threadid.

   if (tid==0)
      {
        printf("Naive linear bandwidth summation = %-.3f MB/s\n",B_sum);
        printf("More precise calculation of Bandwidth = %-.3f MB/s\n",
		(double)bytes_of_io*(double)nthreads/(dt_max * (double)MB));
      }
   
   
   MPI_Finalize();
#endif
   return 0;    
  
  }

void swap_arrays (void *array1, void *array2)
 {
   void         *temp;
   temp         = array1;
   array1       = array2;
   array2       = temp;
 }
