#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

// Note: all pos in sector unless before passed in read/write
int SECTOR_SIZE = 512;

long rand_pos(int Align) {
    //return Align * rand(8*1024*1024*2/Align);      // randomly span 8GB, while aligned to Align
    return Align * (rand() %(100*1024*1024*2/Align));      // randomly span 8/200GB, while aligned to Align
}

int main(int argc, char* * argv) {
     printf("===== Identify Chunk Size of Specified Device =====\n");
     // identify chunk size
     if (argc < 5) {
         printf("Wrong parameters: identify_chunk dev_name M(Estimated Max chunk size) n(req_size) k(offset)\n");
         return 1;
     }
     int M = atoi(argv[2]);
     int n = atoi(argv[3]);
     int k = atoi(argv[4]);
     char * read_buf = (char *) malloc(sizeof(char) * (n * SECTOR_SIZE + SECTOR_SIZE));
     int ret = posix_memalign((void **)&read_buf, SECTOR_SIZE, n*SECTOR_SIZE);

     printf("To run with:\n    aligned_chunk: %d, request_size: %d, offset: %d, device_name: %s\n", M, n, k, argv[1]);

     // open raw block device
     int fd = open(argv[1], O_RDONLY | O_DIRECT);      // O_DIRECT?????
     if (fd < 0) {
         printf("Raw Device Open failed\n");
         return 1;
     } 
     srand(time(0));

     for (int i = 0; i < 500000; i++) {
         long pos = rand_pos(M) + k;
         //printf("Random Position: %ld \n", pos * SECTOR_SIZE);
         int sz = pread(fd, read_buf, n * SECTOR_SIZE, pos * SECTOR_SIZE);
         //int sz = read(fd, read_buf, n * SECTOR_SIZE);
         assert(sz == n*SECTOR_SIZE);
         //printf("Read in %d bytes\n", sz);
     }

     printf("Experiment done\n");
     close(fd);
     return 0;
}
