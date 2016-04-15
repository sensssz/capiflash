#! /bin/bash
truncate -s 0 capi_read_async
time=1
for ((nblk = 1; nblk < 1048576; nblk = ${nblk} + 128))
do
    ./blockio -d /dev/sg15 -r 100 -q 500 -n ${nblk} -s ${time} -p >> capi_read_async
    time=$((time+1))
done

truncate -s 0 capi_write_async
time=1
for ((nblk = 1; nblk < 1048576; nblk = ${nblk} + 128))
do
    ./blockio -d /dev/sg15 -r 0 -q 500 -n ${nblk} -s ${time} -p >> capi_write_async
    time=$((time+1))
done
