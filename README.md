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


Write need to trim all?

Mix....

## Detecting Interleaving Degree (Figure 3)
* inititialize the device:
    * trim all data: fio trim\_all.fio
    * fill the storage space using sequential writes with a request size of 256KB: ./init device\_name device\_size(in sector) 512
  
## Parallel Sectors (Figure 6)
* ???? 

## Identifying Alignment Influence (Figure 9)

## Detecting Mapping Policy (Figure 11)

