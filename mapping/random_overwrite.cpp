#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <algorithm>
// Note: all pos in sector unless before passed in read/write
int SECTOR_SIZE = 512;
int S = 0;
int n = 0;


int main(int argc, char* * argv) {
     // identify chunk size
     if (argc < 4) {
         printf("Wrong parameters: random_overwrite dev_name S(overwritten zone in GB, start from 0) n(req_size in sector) \n");
         return 1;
     }
     S = atoi(argv[2]);
     n = atoi(argv[3]);
     
     char * write_buf = (char *) malloc(sizeof(char) * (n * SECTOR_SIZE));
     int ret = posix_memalign((void **)&write_buf, SECTOR_SIZE, n*SECTOR_SIZE);

     //printf("To run with:\n    zone_size: %d GB, request_size: %d, device_name: %s\n", S, n, argv[1]);

     // open raw block device
     int fd = open(argv[1], O_WRONLY | O_DIRECT);
     if (fd < 0) {
         printf("Raw Device Open failed\n");
         return 1;
     } 

     //generate_random_overwrite_order();
     std::vector<long> pos_vector;
     for (long i = 0; i < (long)(S)*1024*1024*1024; i+=n*SECTOR_SIZE) pos_vector.push_back(i);
     std::random_shuffle (pos_vector.begin(), pos_vector.end() );
     std::cout << pos_vector.size() << std::endl;
     for (auto it = pos_vector.begin(); it != pos_vector.end(); it++) {
         std::cout << *it << std::endl;
         int sz = pwrite(fd, write_buf, n * SECTOR_SIZE, *it);
         assert(sz == n*SECTOR_SIZE);
     }


     //for (int i = 0; i < S; i+=n) {
     //    long pos = rand_pos(M);
     //    int sz = pwrite(fd, write_buf, n * SECTOR_SIZE);
     //    assert(sz == n*SECTOR_SIZE);
     //}

     free(write_buf);
     close(fd);
     return 0;
}
