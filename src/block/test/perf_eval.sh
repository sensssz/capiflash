#! /bin/zsh
SEQ=seq
mkdir -p $SEQ
#echo 'CAPI async write'
#./perf_eval capi ${SEQ} write async > ${SEQ}/capi_write_async&
#pid=$!
#sleep 2 && ps -p ${pid} -o %cpu
#wait $pid
#
#echo 'CAPI async read'
#./perf_eval capi ${SEQ} read async > ${SEQ}/capi_read_async&
#pid=$!
#sleep 2 && ps -p ${pid} -o %cpu
#wait $pid
#
#echo 'CAPI sync write'
#./perf_eval capi ${SEQ} write sync > ${SEQ}/capi_write_sync&
#pid=$!
#sleep 2 && ps -p ${pid} -o %cpu
#wait $pid
#
#echo 'CAPI sync read'
#./perf_eval capi ${SEQ} read sync > ${SEQ}/capi_read_sync&
#pid=$!
#sleep 2 && ps -p ${pid} -o %cpu
#wait $pid
#
#echo 'CAPI sync aread'
#rm capi_read_async
#touch capi_read_async
#for((i=1;i<=4096;i+=128))
#do
#    ./blockio -d /dev/sg15 -q 1 -p -n $i -r 100 >> ${SEQ}/capi_read_async
#done
#
#echo 'CAPI async write'
#rm capi_write_async
#touch capi_write_async
#for((i=1;i<=4096;i+=128))
#do
#    ./blockio -d /dev/sg15 -q 1 -p -n $i -r 0 >> ${SEQ}/capi_write_async
#done
#
echo 'File write sync'
./perf_eval file ${SEQ} write sync > ${SEQ}/file_write_sync&
pid=$!
sleep 2 && ps -p ${pid} -o %cpu
wait $pid

echo 'File read sync'
./perf_eval file ${SEQ} read sync > ${SEQ}/file_read_sync&
pid=$!
sleep 2 && ps -p ${pid} -o %cpu
wait $pid

echo 'File write async'
./perf_eval file ${SEQ} write async > ${SEQ}/file_write_async&
pid=$!
sleep 2 && ps -p ${pid} -o %cpu
wait $pid

echo 'File read async'
./perf_eval file ${SEQ} read async > ${SEQ}/file_read_async&
pid=$!
sleep 2 && ps -p ${pid} -o %cpu
wait $pid
