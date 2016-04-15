#!/bin/bash

for i in  `seq 0 3`;
do
    echo "Benchmark: $i"
	command= "/root/staging/capi/capiflash/capiflash/src/block/test/capi_hardware_cache" "-d" "/dev/sg15" "-s" "100" "-t" "$i"
	exec $command
done

