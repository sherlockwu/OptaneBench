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

int main(int argc, char* * argv) {
     printf("===== Initialize Specified Device =====\n");
     // identify chunk size
     if (argc < 4) {
         printf("Wrong parameters: init dev_name S(device_size in sector) n(req_size in sector) \n");
         return 1;
     }
     int S = atoi(argv[2]);
     int n = atoi(argv[3]);
     
     char * write_buf = (char *) malloc(sizeof(char) * ((n+1) * SECTOR_SIZE));
     int ret = posix_memalign((void **)&write_buf, SECTOR_SIZE, (n+1)*SECTOR_SIZE);

     printf("To run with:\n    device_size: %d, request_size: %d, device_name: %s\n", S, n, argv[1]);

     // open raw block device
     int fd = open(argv[1], O_WRONLY | O_DIRECT);
     if (fd < 0) {
         printf("Raw Device Open failed\n");
         return 1;
     } 


     for (int i = 0; i < S; i+=n) {
         if (i%1000000 == 0)
             printf("Done %d\n", i);
         int sz = write(fd, write_buf, n * SECTOR_SIZE);
         assert(sz == n*SECTOR_SIZE);
     }

     free(write_buf);
     close(fd);
     return 0;
}
