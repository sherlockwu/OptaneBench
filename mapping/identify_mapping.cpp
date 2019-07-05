#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <atomic>
#include <fstream>
#include <vector>
#include <iostream>

std::atomic<long> pointer(0);
std::vector<long> write_order;

// Note: all pos in sector unless before passed in read/write
int SECTOR_SIZE = 512;
int STRIDE_SIZE = SECTOR_SIZE * 16;      // chunk size actually
int num_ios = 10000;

int jobs = 0;
int j = 0;
int d = 0;
int fd = 0;
void *eachThread(void *vargp) 
{ 
    int id = *(int*)vargp;
    long pos;    // in unit of sector
    printf("Thread %d ready to run \n", id);
    char * read_buf = (char *) malloc(sizeof(char) * (STRIDE_SIZE + SECTOR_SIZE));
    int ret = posix_memalign((void **)&read_buf, SECTOR_SIZE, STRIDE_SIZE + SECTOR_SIZE);
    
    int sz;
    while (jobs <= 0) ;

    // do IO 
    
    for ( ; ; ) {
        pos = pointer.fetch_add(1);
        if (pos > num_ios)
            break;
        //printf("Position: %d \n", pos * d);
        //sz = pread(fd, read_buf, 8 * SECTOR_SIZE, (pos + i * big_stride)* SECTOR_SIZE);
        //sz = pread(fd, read_buf, 8 * SECTOR_SIZE, pointer * d * SECTOR_SIZE);
        //assert(sz == 8*SECTOR_SIZE);
        //sz = pread(fd, read_buf, STRIDE_SIZE, pos * d * STRIDE_SIZE);
        //sz = pread(fd, read_buf, STRIDE_SIZE, (pos/j) * 16 * STRIDE_SIZE + (pos%j) * d * STRIDE_SIZE);
        //sz = pread(fd, read_buf, STRIDE_SIZE, (pos%j) * d * STRIDE_SIZE);
        //sz = pread(fd, read_buf, STRIDE_SIZE, (pos%2) * d * STRIDE_SIZE);
        //sz = pread(fd, read_buf, STRIDE_SIZE, (pos%2) * d * STRIDE_SIZE);
        // according to write order
        //std::cout << pos << ":" << write_order[(pos%2) * d] << std::endl;
        //sz = pread(fd, read_buf, STRIDE_SIZE, write_order[(pos%2) * d]);
        sz = pread(fd, read_buf, STRIDE_SIZE, write_order[pos * (d+1)]);
        //printf("Done %ld %d\n", pointer*d*STRIDE_SIZE, sz);
        assert(sz == STRIDE_SIZE);
    }
    free(read_buf);
    return NULL; 
} 



int main(int argc, char* * argv) {
     printf("===== Identify Mapping of Specified Device(read according to writes order) =====\n");
     // identify chunk size
     if (argc < 6) {
         printf("Wrong parameters: identify_mapping dev_name D(Estimated Max interleave degree) j(num_parallel_jobs) d(stride in chunk(16 sectors)) write_order(write order file name)\n");
         return 1;
     }
     int D = atoi(argv[2]);
     j = atoi(argv[3]);
     d = atoi(argv[4]);

     printf("To run with:\n    Max_Degree: %d, num_jobs: %d, stride: %d, device_name: %s, write_order: %s\n", D, j, d, argv[1], argv[5]);

     // open raw block device
     fd = open(argv[1], O_RDONLY | O_DIRECT);      // O_DIRECT
     if (fd < 0) {
         printf("Raw Device Open failed\n");
         return 1;
     } 

     //parallel reads
     
     // start the number of threads
     pthread_t * thread_pool = (pthread_t *)malloc(sizeof(pthread_t) * j);

     for (int i = 0; i < j; i++) {
         int *arg = (int *)malloc(sizeof(*arg));
         *arg = i;
         pthread_create(&thread_pool[i], NULL, eachThread, (void *)arg);
     }

     // prepare the read trace (according to pointer)
     std::ifstream write_trace(argv[5]);
     long cur_write;
     while(write_trace >> cur_write) {
          write_order.push_back(cur_write);
     } 

     sleep(2);
     
     struct timeval start, end;
     gettimeofday(&start, NULL);
     jobs = j;  // hint to start doing IOs
     for (int i = 0; i < j; i++) {
         pthread_join(thread_pool[i], NULL);
     }
     gettimeofday(&end, NULL);
     long time_us = ((end.tv_sec * 1000000 + end.tv_usec)
                  - (start.tv_sec * 1000000 + start.tv_usec));
     //printf("Time taken: %ld us, Bandwidth: %f MB/s \n", time_us, (float)(8*512*100000)/1024/1024*1000000/time_us);
     printf("Time taken: %ld us, Bandwidth: %f MB/s \n", time_us, (float)(STRIDE_SIZE*num_ios)/1024/1024*1000000/time_us);
     
     close(fd);
     return 0;
}
