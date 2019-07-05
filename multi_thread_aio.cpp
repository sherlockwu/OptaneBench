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

std::condition_variable cond;
std::mutex mutex;
int queue_depth = 0;
int max_qd = 16;
ConsumerProducerQueue<long> job_queue;
int pattern = 7;

struct io_event events[MAX_COUNT];
struct timespec timeout;
char * read_buf;

#define handle_error_en(en, msg) \
           do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

void *eachThread(void *vargp) 
{
    // consumer	
    int id = *(int*)vargp;
     
     // pin main thread to somewhere
     int s, j;
     cpu_set_t cpuset;
     pthread_t thread;

     thread = pthread_self();

     /* Set affinity mask to include CPUs 0 to 4 */
     CPU_ZERO(&cpuset);
     CPU_SET((id+1)%4, &cpuset);

     s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
     if (s != 0)
       handle_error_en(s, "pthread_setaffinity_np");

     /* Check the actual affinity mask assigned to the thread */
     s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
     if (s != 0)
         handle_error_en(s, "pthread_getaffinity_np");

     printf("Set returned by pthread_getaffinity_np() contained:\n");
     for (j = 0; j < CPU_SETSIZE; j++)
         if (CPU_ISSET(j, &cpuset))
             printf("    CPU %d\n", j);
    

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
     if (type == 'r') {
	 std::cout << "To read randomly" << std::endl;
         srand(time(0));
	 for (int i = 0; i < num_ios; i++)
             read_order.push_back((long)((rand() %(80*1024*1024*2/d)))*STRIDE_SIZE);
     } else if (type == 's') {
	 std::cout << "To read sequentially with offset " << D << std::endl;
         for (int i = 0; i < num_ios; i++) {
             // who is channel 1?
	     // i*7 is in: (0+D*i)%7
             //std::cout << i*7 << " in " << (D*i)%7 << " first channel: " << i*7 + (7-((D*i)%7-0)) %7 << std::endl;
             //read_order.push_back((long)(i*7 + (7-((D*i)%7-0)) %7)*STRIDE_SIZE);
             read_order.push_back((long)(i*STRIDE_SIZE));
	 }
         //exit(1);
         //for (int i = 0; i < num_ios; i++)
         //    read_order.push_back((long)(i)*STRIDE_SIZE);
     } else if (type == 'j') {
	 std::cout << "To read stridely with stride: " << D << std::endl;
         for (int i = 0; i < num_ios; i++)
             read_order.push_back(((long)(i+0)*(STRIDE_SIZE + D*SECTOR_SIZE))%((long)800*1024*1024*1024));
     } else if (type == 'z') {
	 std::cout << "To read zigzag" << std::endl;
         for (int i = 0; i < num_ios; i++)
             read_order.push_back((long)(i*7 + (i%2)*6)*STRIDE_SIZE);
     } else if (type == 'p') {   // shear pair
	 std::cout << "To read according to shear pair(randomly choose pattern, then, issue two parallel reads within each pattern at the same offsets)" << std::endl;
         srand(time(0));
	 std::cout << "Pattern size: " << pattern << std::endl;
	 for (int i = 0; i < num_ios-1; i += 2) {
             // segment number
	     long segment = rand() % (800*1024*1024*2/(d*pattern));
	     read_order.push_back(segment*(pattern*STRIDE_SIZE));
             read_order.push_back(segment*(pattern*STRIDE_SIZE) + (D+d)*SECTOR_SIZE);
	 }
     } else if (type == 'e') {  // split sequential 8 sectors into 2*4 sectors etc
	 std::cout << "To read stridely with stride: " << D << " and split 8 sectors "<< std::endl;
         int small = 8/ (STRIDE_SIZE/SECTOR_SIZE);
         num_ios *= small;
	 for (int i = 0; i < num_ios/small; i++) {
	     for (int j = 0; j < small; j++)
	         read_order.push_back((long)(i)*(8*SECTOR_SIZE + D*SECTOR_SIZE) + j*STRIDE_SIZE);
	 }
     } else if (type == '1') {  // split 8 sectors into 1*QD sectors
	 std::cout << "To read stridely with stride: " << D << " and choose " << max_qd << " out of each 8 sectors "<< std::endl;
         int small = max_qd;
         num_ios *= small;
	 for (int i = 0; i < num_ios/small; i++) {
	     for (int j = 0; j < small; j++)
	         read_order.push_back((long)(i)*(8*SECTOR_SIZE + D*SECTOR_SIZE) + j*STRIDE_SIZE);
	 }
     } else if (type == '4') {  // split 8 sectors into 1*QD sectors
         int small = 4;
         num_ios *= small;
	 for (int i = 0; i < num_ios/small; i++) {
	     for (int j = 0; j < small; j++)
	         read_order.push_back((long)(i)*(8*SECTOR_SIZE + D*SECTOR_SIZE) + j*STRIDE_SIZE);
	 }
     } else {
        std::cout << "Wrong choice of workloads, should be seq/random" << std::endl;
	exit(1);
     }

     return;
}

int main(int argc, char* * argv) {
     // pin main thread to somewhere
     int s, j;
     cpu_set_t cpuset;
     pthread_t thread;

     thread = pthread_self();

     /* Set affinity mask to include CPUs 0 to 3 */
     CPU_ZERO(&cpuset);
     CPU_SET(0, &cpuset);

     s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
     if (s != 0)
       handle_error_en(s, "pthread_setaffinity_np");

     /* Check the actual affinity mask assigned to the thread */
     s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
     if (s != 0)
         handle_error_en(s, "pthread_getaffinity_np");

     printf("Set returned by pthread_getaffinity_np() contained:\n");
     for (j = 0; j < CPU_SETSIZE; j++)
         if (CPU_ISSET(j, &cpuset))
             printf("    CPU %d\n", j);
    

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
     for (int i = 0; i < num_ios;) {
	// just add
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
            //    free((struct iocb *)events[j].obj);
            //}
        }

        //if (onflight_io >= max_qd)
        //continue;
	
	//std::cout << "Add " << i << ": " << read_order[i] << std::endl;
	//if (argv[5][0] == 'j') {
                if (onflight_io >= max_qd)
                //if (onflight_io/2 >= max_qd)
                  continue;
	
		while (onflight_io < max_qd && i < num_ios) {
                    job_queue.add(read_order[i]);
	            onflight_io++;
	            i++;
                } 
		//job_queue.add(read_order[i]);
		//job_queue.add(read_order[i] + SECTOR_SIZE);
	        //onflight_io+=2;
                //onflight_io++;
        /*} else {
                if (onflight_io >= max_qd)
                  continue;
	        job_queue.add(read_order[i]);
	        onflight_io++;
                i++;
	}*/
        //i++;
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
     
     
     // Signal to exit 
     for (int i = 0; i < j; i++) {
         job_queue.add(-1);
     }
     for (int i = 0; i < j; i++) {
         pthread_join(thread_pool[i], NULL);
     }
     std::cout << "All IO finished " << STRIDE_SIZE << std::endl;
     printf("Time taken: %ld us, Bandwidth: %f MB/s \n", time_us, (float)((long)STRIDE_SIZE*num_ios)/1024/1024*1000000/time_us);

     free(read_buf);
     close(fd);
     return 0;
}
