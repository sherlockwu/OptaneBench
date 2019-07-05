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
#include <libaio.h>
#include <mutex>
#include <condition_variable>
#include "ConsumerProducerQueue.h"

#define MAX_COUNT 65536

std::atomic<long> pointer(0);
std::vector<long> read_order;

// Note: all pos in sector unless before passed in read/write
int SECTOR_SIZE = 512;
int STRIDE_SIZE = SECTOR_SIZE * 1;      // chunk size actually
int num_ios = 100000;
int completed_ios = 0;

int D = 0;
int j = 0;
int d = 0;
int fd = 0;
io_context_t ctx_;
int TRIES = 5;    // for shear
long overall_us = 0;

std::condition_variable cond;
std::mutex mutex;
int queue_depth = 0;
int max_qd = 16;
ConsumerProducerQueue<long> job_queue;
int pattern = 7;

struct io_event events[MAX_COUNT];
struct timespec timeout;
char * read_buf;

void *eachThread(void *vargp) 
{
    // consumer	
    int id = *(int*)vargp;
    long pos;    // in unit of sector
    printf("Thread %d ready to run \n", id);
    
    int sz;

    for ( ; ; ) {
	// Consumer wait the queue depth control
	job_queue.consume(pos);
	if (pos == -1) {
	    break;
	}
	//std::cout << "to issue IO " << pos/4096 << std::endl;
        struct iocb * p = (struct iocb *)malloc(sizeof(struct iocb));
            //io_prep_pread(p, fd_[item.fd], item.buffer, item.length, item.offset);
        io_prep_pread(p, fd, read_buf, STRIDE_SIZE, pos);
        //p->data = (void *) item.io_status; 
        p->data = (void *) read_buf;

        if (io_submit(ctx_, 1, &p) != 1) {
            io_destroy(ctx_);
            std::cout << "io submit error" << std::endl;
            exit(1);
        }
    }
    std::cout << "Thread " << id << " issued all IOs" << std::endl;
    return NULL; 
} 


void generate_read_trace(char type) {
     if (type == 's') {
	 std::cout << "running shear: qd: " << max_qd << " segment size(in sector): " << D*d << std::endl;
         srand(time(0));
	 for (int i = 0; i < num_ios; i++)        // generate random segments
             read_order.push_back((long)((rand() %(800*1024*1024*2/(D*d))))*D*STRIDE_SIZE);
         return;
     } else if (type == 'r') {
	 std::cout << "To read randomly" << std::endl;
         srand(time(0));
	 for (int i = 0; i < num_ios; i++)
             read_order.push_back((long)((rand() %(800*1024*1024/4)))*4096);
     } else {
        std::cout << "Wrong choice of workloads, should be seq/random" << std::endl;
	exit(1);
     }

     return;
}

int main(int argc, char* * argv) {
     printf("===== Multi-thread libaio to Specified Device =====\n");
     // identify chunk size
     if (argc < 7) {
         printf("Wrong parameters: multi_thread_aio dev_name D(offset to jump, in unit of sector) j(num_parallel_jobs to submit IO) d(stride/io_size in sector) read_type(random/seq/jump) queue_depth\n");
         return 1;
     }
     D = atoi(argv[2]);    //offset between two reads, in unit of sector
     j = atoi(argv[3]);
     d = atoi(argv[4]);        // request size
     STRIDE_SIZE = SECTOR_SIZE * d;
     max_qd = atoi(argv[6]);
     int FIRST = atoi(argv[7]);
     //max_qd = 16;
     pattern = atoi(argv[6])/16;
     job_queue.set_max(max_qd);

     printf("To run with:\n    jump: %d, num_jobs: %d, stride: %d, device_name: %s, write_order: %s, max_qd: %d\n", D, j, d, argv[1], argv[5], max_qd);

     // open raw block device
     fd = open(argv[1], O_RDONLY | O_DIRECT);      // O_DIRECT
     if (fd < 0) {
         printf("Raw Device Open failed\n");
         return 1;
     } 
    // context to do async IO 
    memset(&ctx_, 0, sizeof(ctx_));
    if (io_setup(MAX_COUNT, &ctx_) != 0) {
        std::cout << "io_context_t set failed" << std::endl;
        exit(1);
    }
    // generate read trace
    //
     generate_read_trace(argv[5][0]);

     //parallel reads
     
     // start the number of threads
     pthread_t * thread_pool = (pthread_t *)malloc(sizeof(pthread_t) * j);

     
     for (int i = 0; i < j; i++) {
         int *arg = (int *)malloc(sizeof(*arg));
         *arg = i;
         pthread_create(&thread_pool[i], NULL, eachThread, (void *)arg);
     }


     read_buf = (char *) malloc(sizeof(char) * (STRIDE_SIZE + SECTOR_SIZE));
     int ret = posix_memalign((void **)&read_buf, SECTOR_SIZE, STRIDE_SIZE + SECTOR_SIZE);
     sleep(1);
    
     //as a producer
     struct timeval start, end;
     gettimeofday(&start, NULL);
     int onflight_io = 0;

     timeout.tv_sec = 0;
     timeout.tv_nsec = 100;
     for (int i = 0; i < num_ios; i++) {
	// for each request
	onflight_io = 0;
	if (argv[5][0] == 'r') {
	    // parallel reads
	    for (int k = 0; k < max_qd; k++) {
                job_queue.add(read_order[i] + k*SECTOR_SIZE);
	    }
	    onflight_io = max_qd;
	} else if (argv[5][0] == 'p') {
            job_queue.add(FIRST*STRIDE_SIZE);
            job_queue.add((FIRST+1)*STRIDE_SIZE + D*SECTOR_SIZE);
	    onflight_io = 2;
	}
	// wait until all finished
	while (onflight_io > 0) {
            int ret = io_getevents(ctx_, 0, MAX_COUNT, events, &timeout);
            if (ret < 0) {
                std::cout << "Getevents Error" << std::endl; 
                exit(1);
            } 
	    if (ret > 0) {
	        onflight_io -= ret;
	    }
	}
     }

     //wait until all IO finished
     while (onflight_io) {
        int ret = io_getevents(ctx_, 0, MAX_COUNT, events, &timeout);
        if (ret < 0) {
            std::cout << "Getevents Error" << std::endl; 
            exit(1);
        } 
	if (ret > 0) {
	    onflight_io -= ret;
            //std::cout << "=== Get Events("<< ret << "): " << ret << std::endl;
            //for (int j = 0; j < ret; j++) {
            //    std::cout << "=== Complete " << "::" << events[j].obj->aio_fildes << std::endl;
            //}
        } 
     }



     gettimeofday(&end, NULL);
     long time_us = ((end.tv_sec * 1000000 + end.tv_usec)
                  - (start.tv_sec * 1000000 + start.tv_usec));
     
     overall_us += time_us; 
     // Signal to exit 
     for (int i = 0; i < j; i++) {
         job_queue.add(-1);
     }
     for (int i = 0; i < j; i++) {
         pthread_join(thread_pool[i], NULL);
     }
     
     std::cout << "All IO finished" << std::endl;
     printf("Time taken: %ld us, Bandwidth: %f MB/s \n", time_us, (float)((long)STRIDE_SIZE*num_ios)/1024/1024*1000000/time_us);
     //printf("Time taken: %ld us, Bandwidth: %f MB/s \n", overall_us/TRIES, (float)((long)STRIDE_SIZE*num_ios)/1024/1024*1000000/(overall_us/TRIES));
    
     free(read_buf);
     close(fd);
     return 0;
}
