#ifndef PROXY_H
#define PROXY_H

#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define GET 1
#define BUFFER_SIZE 4096
#define CIRC_SIZE 16

struct followerServerInfo {
  int port;
  int numErrors;
  int totalRequests;
  bool isAlive;
};

typedef struct httpProxy {
  int numFollowerServers;
  struct followerServerInfo *servers;
} proxyObj;

typedef struct thread {
  int id;
  int numFollowerServers;
  int *requestsHealthcheck;
  int *countConnectionsHC;
  int capacityCache;
  int maxFileSizeCache;
  bool hasCache;
} threadObj;

typedef struct httpServerInfo {
  char buff[BUFFER_SIZE];
  char inFile[21];
  char commandStr[10];
  int command;
  char version[10];
  char requestLine[128];
  char response[256];
  char host[64];
  ssize_t contentLength;
  bool valid;

  char first1KB[1024];
  int statusCode;
  int responseValue;
  char headerLines[2048];
} httpObj;

typedef struct circQueue {
  int *buff;
  int size;
  int head;
  int tail;
  int count;
  struct fileInfo *file;
} queue;

struct fileInfo {
  char *filename;
  bool needsUpdate;
  int maxFileSizeCache;
  ssize_t contentLength;
  int headerSize;
  uint8_t *buff;
  char *age;
  int sizeBuff;
};

// circularQueue.c
queue g_cb;
int gCbuff[CIRC_SIZE];

queue *initQueue(int size);
void freeQueue(queue *q);
bool isEmpty(queue *q);
bool isFull(queue *q);
void enqueue(queue *q, int data);
int dequeue(queue *q);
void printQueue(queue *q);

queue *initCache(int size, int maxFileSizeCache);
bool isInCache(queue *q, char *data);
void printCache(queue *q);
void enqueueCache(queue *q, char *filename);
char *dequeueCache(queue *q);
int getIndex(queue *q, char *filename);
void clearFileEntry(queue *q, int index);
void addToFileBuffer(queue *q, int index, uint8_t *recvline, int n);
// lib.c
int create_listen_socket(uint16_t port);
int create_client_socket(uint16_t port);
void send500(int fd);
int bridge_connections(int fromfd, int tofd, httpObj *data, threadObj *thread);
void bridge_loop(int sockfd1, int sockfd2, httpObj *data, threadObj *thread);
uint16_t strtouint16(char number[]);
int32_t strtoint32(char number[]);
void createHttpResponse(httpObj *data, char *type, ssize_t fsize, char *msg);
void createErrorResponse(httpObj *data, int errn);
int getDCRLFindex(char *buff);
bool isValidFileName(char *line);
void parseRequestLine(httpObj *data, char *line);
void validateRequest(httpObj *data);
bool fileIsNewer(char *proxyDate, char *followerDate);

// httpproxy.c
void *handleConnection(void *arg);
void addRequest(int clientfd, queue *q);
void *healthcheck();
int getFollowerPort();
void probeFollowerServers();
void addFileToProxy(httpObj *data, threadObj *thread);
void readRequest(httpObj *data, int clientfd);
int processRequest(httpObj *data, int clientfd, threadObj *thread);
void addToCache(queue *q, char *filename);
#endif