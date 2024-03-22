#!/bin/bash
SERVERS_PATH=~/cse130f21/asgn3/serverFolder

if [ ! -d ${SERVERS_PATH} ] 
then
    echo "Directory ${SERVERS_PATH} DOES NOT exist." 
    exit 1
fi

rm -rf out*

# create binary files
head -c 1021 < /dev/urandom > ${SERVERS_PATH}/srcSmall.bin
head -c 1020 < /dev/urandom > ${SERVERS_PATH}/srcSmall2.bin
head -c 333 < /dev/urandom > ${SERVERS_PATH}/srcSmall3.bin
head -c 444 < /dev/urandom > ${SERVERS_PATH}/srcSmall4.bin

echo "[+] Testing FIFO ----------------------------"
(
curl -s localhost:8080/srcSmall.bin -o out &
curl -s localhost:8080/srcSmall2.bin -o out2 &
curl -s localhost:8080/srcSmall3.bin -o out3 &
curl -s localhost:8080/srcSmall4.bin -o out4 &
wait)
# srcSmall -> srcSmall2 -> srcSmall3

diff out ${SERVERS_PATH}/srcSmall.bin
diff out2 ${SERVERS_PATH}/srcSmall2.bin 
diff out3 ${SERVERS_PATH}/srcSmall3.bin 
diff out4 ${SERVERS_PATH}/srcSmall4.bin 













# # srcSmall dequeued
# # srcSmall4 enqueued
# # srcSmall2 -> srcSmall3 -> srcSmall4
# curl -s localhost:8080/srcSmall4.bin -o out4
# diff out4 ${SERVERS_PATH}/srcSmall4.bin 

# # srcSmall2 dequeued
# # srcSmall enqueued
# # srcSmall3 -> srcSmall4 -> srcSmall
# curl -s localhost:8080/srcSmall.bin -o out
# diff out ${SERVERS_PATH}/srcSmall.bin 

# # cache should stay 
# # srcSmall3 -> srcSmall4 -> srcSmall
# curl -s localhost:8080/srcSmall3.bin -o out3
# diff out3 ${SERVERS_PATH}/srcSmall3.bin 

# curl -s localhost:8080/srcSmall.bin -o out
# diff out ${SERVERS_PATH}/srcSmall.bin 

# curl -s localhost:8080/srcSmall4.bin -o out4
# diff out4 ${SERVERS_PATH}/srcSmall4.bin 

