# Objective
This project aims to create an HTTP reverse proxy that will support GET
requests, persistent connections, load balancing, and caching. Concurrency is needed for
servicing multiple client requests at the same time. The proxy server will be the manager server which distributes the client’s file descriptors to one of the prioritized follower servers for handling. The proxy acts as a client for the follower servers and acts as a server for incoming request connections. Load balancing requires probing the “healthcheck” of the followers to prioritize the follower server that has the least amount of requests. For caching, if the GET request resource is in cache the server would send a HEAD request to the follower server holding the resource and parse through the Last_Modified header line to determine if the stored resource is newer or the same age. 
This is Assignment 3 for CSE 130: Principles of Computer System Design, Fall 2021, see asgn3.pdf for details

## Instruction for building:
make

## Instruction for running:
First port is always the proxy port and the others are always httpserver ports

```
./httpproxy proxyport httpserverports [-N numberOfThreads] [-R requestshealthcheck] [-c cacheCapacity] [-m maximumFileSizeInCache]
```
