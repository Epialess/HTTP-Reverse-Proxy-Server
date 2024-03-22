#!/bin/bash
SERVERS_PATH=~/cse130f21/asgn3/serverFolder
DOMAIN=localhost
PORT=8080 

# https://www.cyberciti.biz/faq/howto-check-if-a-directory-exists-in-a-bash-shellscript/
if [ ! -d ${SERVERS_PATH} ] 
then
    echo "Directory ${SERVERS_PATH} DOES NOT exist." 
    exit 1
fi

rm -rf out*

head -c 100 < /dev/urandom > ${SERVERS_PATH}/srcSmall
head -c 222 < /dev/urandom > ${SERVERS_PATH}/srcSmall2
head -c 333 < /dev/urandom > ${SERVERS_PATH}/srcSmall3
head -c 10000 < /dev/urandom > ${SERVERS_PATH}/srcMedium
head -c 22222 < /dev/urandom > ${SERVERS_PATH}/srcMedium2
head -c 100000 < /dev/urandom > ${SERVERS_PATH}/srcLarge
head -c 222222 < /dev/urandom > ${SERVERS_PATH}/srcLarge2
base64 /dev/urandom | head -c 1022 > ${SERVERS_PATH}/srcSmall

create binary files
head -c 1021 < /dev/urandom > ${SERVERS_PATH}/srcSmall.bin
head -c 1020 < /dev/urandom > ${SERVERS_PATH}/srcSmall2.bin
head -c 333 < /dev/urandom > ${SERVERS_PATH}/srcSmall3.bin
head -c 444 < /dev/urandom > ${SERVERS_PATH}/srcSmall4.bin

head -c 10000 < /dev/urandom > ${SERVERS_PATH}/srcMedium.bin
head -c 1000000 < /dev/urandom > ${SERVERS_PATH}/srcLarge.bin
base64 /dev/urandom | head -c 1000000 > ${SERVERS_PATH}/srcLarge

curl -s localhost:8080/srcLarge -o out
diff out ${SERVERS_PATH}/srcLarge

head -c 1000000 < /dev/urandom > ~/cse130f21/asgn3/serverFolder/srcLarge.bin
head -c 4499 < /dev/urandom > ~/cse130f21/asgn3/serverFolder/srcLarge.bin


curl -sv localhost:8080/srcLarge.bin -o out2
diff out2 ${SERVERS_PATH}/srcLarge.bin

head -c 1021 < /dev/urandom > ${SERVERS_PATH}/srcSmall.bin
curl -s localhost:8080/srcSmall.bin -o out
diff out ${SERVERS_PATH}/srcSmall.bin 




echo "[+] Testing Bad Request Error---------------------------------------"
curl -H "Host: 3409832" localhost:8080/httpproxy.c
curl -H "Host: " localhost:8080/httpproxy.c
curl --http1.0 localhost:8080/httpproxy.c
curl localhost:8080/-=-=-=-=
curl localhost:8080/sldiufjhgsluidfghseiurgfhoseiurghuihewr

echo "[+] Testing File Not Found Error---------------------------------------"
curl -sv localhost:8080/fileNotInServer

echo "[+] Testing Not Implemented Error--------------------------------------"
curl -sI localhost:8080/srcSmall.bin
curl -si -X POST localhost:8080/srcSmall.bin

echo "[+] Testing GET small --------------------------------------"
curl -s localhost:8080/srcSmall.bin -o out
diff out ${SERVERS_PATH}/srcSmall.bin 

curl -s localhost:8080/srcSmall2.bin -o out2
curl -s localhost:8080/srcSmall3.bin -o out3
curl -s localhost:8080/srcSmall.bin -o out

diff out ${SERVERS_PATH}/srcSmall.bin 
diff out2 ${SERVERS_PATH}/srcSmall2.bin 
diff out3 ${SERVERS_PATH}/srcSmall3.bin 

for i in $(seq 10)
do
curl -s localhost:8080/srcSmall.bin -o out
curl -s localhost:8080/srcMedium.bin -o out2
curl -s localhost:8080/srcLarge.bin -o out3
diff out ${SERVERS_PATH}/srcSmall.bin 
diff out2 ${SERVERS_PATH}/srcMedium.bin
diff out3 ${SERVERS_PATH}/srcLarge.bin
done

# for i in $(seq 10)
# do
# (
# curl -s localhost:8080/srcSmall.bin -o out 
# curl -s localhost:8080/srcMedium.bin -o out2 
# curl -s localhost:8080/srcLarge.bin -o out3 
# wait)
# done

echo "[+] Testing cache small --------------------------------------"
curl -sv localhost:8080/srcSmall.bin -o out
if diff out ${SERVERS_PATH}/srcSmall.bin; then
    echo "passed"
fi

sleep 1
echo -n h >> ${SERVERS_PATH}/srcSmall.bin

curl -s localhost:8080/srcSmall.bin -o out2
diff out2 ${SERVERS_PATH}/srcSmall.bin

curl -s localhost:8080/srcSmall.bin -o out2
diff out2 ${SERVERS_PATH}/srcSmall.bin

echo "[+] Testing cache small binary----------------------------"

curl -sv localhost:8080/srcSmall -o out
if diff out ${SERVERS_PATH}/srcSmall; then
    echo "passed"
fi

sleep 1
echo -n h >> ${SERVERS_PATH}/srcSmall

curl -sv localhost:8080/srcSmall -o out2
diff out2 ${SERVERS_PATH}/srcSmall

curl -sv localhost:8080/srcSmall -o out2
diff out2 ${SERVERS_PATH}/srcSmall

curl -sv localhost:8080/srcSmall2.bin -o out3
if diff out3 ${SERVERS_PATH}/srcSmall2.bin; then
    echo "passed"
fi

sleep 1
echo -n h >> ${SERVERS_PATH}/srcSmall2.bin

curl -sv localhost:8080/srcSmall2.bin -o out4
diff out4 ${SERVERS_PATH}/srcSmall2.bin

echo "[+] Testing FIFO ----------------------------"
curl -s localhost:8080/srcSmall.bin -o out
curl -s localhost:8080/srcSmall2.bin -o out2
curl -s localhost:8080/srcSmall3.bin -o out3

# srcSmall -> srcSmall2 -> srcSmall3

diff out ${SERVERS_PATH}/srcSmall.bin 
diff out2 ${SERVERS_PATH}/srcSmall2.bin 
diff out3 ${SERVERS_PATH}/srcSmall3.bin 

# srcSmall dequeued
# srcSmall4 enqueued
# srcSmall2 -> srcSmall3 -> srcSmall4
curl -s localhost:8080/srcSmall4.bin -o out4
diff out4 ${SERVERS_PATH}/srcSmall4.bin 

# srcSmall2 dequeued
# srcSmall enqueued
# srcSmall3 -> srcSmall4 -> srcSmall
curl -s localhost:8080/srcSmall.bin -o out
diff out ${SERVERS_PATH}/srcSmall.bin 

# cache should stay 
# srcSmall3 -> srcSmall4 -> srcSmall
curl -s localhost:8080/srcSmall3.bin -o out3
diff out3 ${SERVERS_PATH}/srcSmall3.bin 

curl -s localhost:8080/srcSmall.bin -o out
diff out ${SERVERS_PATH}/srcSmall.bin 

curl -s localhost:8080/srcSmall4.bin -o out4
diff out4 ${SERVERS_PATH}/srcSmall4.bin 

echo "[+] Testing FIFO ----------------------------"



(
curl -s localhost:8080/srcLarge.bin -o out & \
curl -s localhost:8080/srcLarge.bin -o out2 & \
curl -s localhost:8080/srcLarge.bin -o out3 & \
curl -s localhost:8080/srcLarge.bin -o out4 & \
curl -s localhost:8080/srcLarge.bin -o out5 & \
curl -s localhost:8080/srcLarge.bin -o out6 & \
wait)

diff out ${SERVER_PATH}/srcLarge.bin
diff out2 ${SERVER_PATH}/srcLarge.bin
diff out3 ${SERVER_PATH}/srcLarge.bin
diff out4 ${SERVER_PATH}/srcLarge.bin
diff out5 ${SERVER_PATH}/srcLarge.bin
diff out6 ${SERVER_PATH}/srcLarge.bin


curl -s http://${DOMAIN}:${PORT}/srcSmall -o out.small
if diff out.small ${SERVER_PATH}/srcSmall; then
echo "test3 passed"
else
echo "test3 failed"
fi

curl -s http://${DOMAIN}:${PORT}/srcSmall2 -o out.small2
if diff out.small2 ${SERVER_PATH}/srcSmall2; then
echo "test4 passed"
else
echo "test4 failed"
fi

# medium text files
curl -s http://${DOMAIN}:${PORT}/srcMedium -o out.medium
if diff out.medium ${SERVER_PATH}/srcMedium; then
echo "test5 passed"
else
echo "test5 failed"
fi

curl -s http://${DOMAIN}:${PORT}/srcMedium2 -o out.medium2
if diff out.medium2 ${SERVER_PATH}/srcMedium2; then
echo "test6 passed"
else
echo "test6 failed"
fi

# large text files
curl -s http://${DOMAIN}:${PORT}/srcLarge -o out.large
if diff out.large ${SERVER_PATH}/srcLarge; then
echo "test7 passed"
else
echo "test7 failed"
fi

curl -s http://${DOMAIN}:${PORT}/srcLarge2 -o out.large2
if diff out.large2 ${SERVER_PATH}/srcLarge2; then
echo "test8 passed"
else
echo "test8 failed"
fi

# small binary file 
curl -s http://${DOMAIN}:${PORT}/srcSmall.bin -o out.smallbin
if diff out.smallbin ${SERVER_PATH}/srcSmall.bin; then
echo "test9 passed"
else
echo "test9 failed"
fi

# medium binary file 
curl -s http://${DOMAIN}:${PORT}/srcMedium.bin -o out.mediumbin
if diff out.mediumbin ${SERVER_PATH}/srcMedium.bin; then
echo "test10 passed"
else
echo "test10 failed"
fi

# large binary file 
curl -s http://${DOMAIN}:${PORT}/srcLarge.bin -o out.largebin
if diff out.largebin ${SERVER_PATH}/srcLarge.bin; then
echo "test11 passed"
else
echo "test11 failed"
fi