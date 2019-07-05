# OptaneBench
Benchmark for Intel Optane SSD, towards an unwritten contract of Intel Optane SSD


# Microworkloads

## Random/Sequential workloads: (Figure 1, 4, 5, 7, 8, 10)
We control workloads with FIO. variables mainly include 0) I/O pattern 2) Queue depth and 3) request size.
rand read template: fio/fio-randread.fio.template
sequential read template: fio/fio-seqread.fio.template


trim\_all.fio before each write workload (to avoid influence from garbage collection)
rand write template: fio/fio-randread.fio.template
sequential write template: fio/fio-seqread.fio.template

To note: overall queue depth = numjobs * iodepth (we use numjobs = 4 when overall QD >= 4, numjobs = 1 when overall QD < 4)
 
Mix:
1. First init 10% of space with fill\_all.fio
2. Use fio-mix.fio.template to issue mixed workloads 

## Detecting Interleaving Degree (Figure 3)
* reference: HPDC 
* inititialize the device:
    * trim all data: fio trim\_all.fio
    * fill the storage space using sequential writes with a request size of 256KB: ./init device\_name device\_size(in sector) 512
* test with different stride:
    * ./multi\_thread\_aio /dev/nvme0n1 8 2 8 jump 16 
    * dev\_name D(offset to jump, in unit of sector) j(num\_parallel\_jobs to submit IO) d(stride/io\_size in sector) read\_type(random/seq/jump) queue\_depth
 
## Parallel Sectors (Figure 6)
* ./chunk\_multi\_thread\_aio /dev/nvme0n1 0 2 1 randomread 4
* chunk\_multi\_thread\_aio dev\_name D(not useful) j(num\_parallel\_jobs to submit IO) d(io\_size in sector) read\_type(random/seq/jump) queue\_depth(how many parallel sector reads to a 4KB chunk)
* blktrace is recommended to track latency of each request

## Identifying Alignment Influence (Figure 9)
* reference: HPDC
* identify\_chunk dev\_name M(Estimated Max chunk size) n(req\_size) k(offset)
* M = 64 during the test. vary n and k to test (both in unit of sector)

## Detecting Mapping Policy (Figure 11)
* init with different written order
    * init: sequential write
    * random write: random\_overwrite dev\_name S(overwritten zone in GB, start from 0) n(req\_size in sector)
* read according to different order:
    * identify\_mapping dev\_name D(Estimated Max interleave degree) j(num\_parallel\_jobs) d(stride in chunk(16 sectors)) write\_order(write order file name)

