#include "httpproxy.h"

pthread_mutex_t queueMut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t healthMut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t healthCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t cacheMut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cacheCond = PTHREAD_COND_INITIALIZER;

proxyObj *proxyPtr = NULL;
queue *cacheQ = NULL;
int priorityIdx = -1;
// consumer thread
void *handleConnection(void *arg) {
  threadObj *thread = (threadObj *)arg;
  while (1) {
    // critical section:
    pthread_mutex_lock(&queueMut);

    while (isEmpty(&g_cb)) {
      // printf("Thread\t[%d] waiting\n", thread->id);
      pthread_cond_wait(&queueCond, &queueMut);
    }

    int clientfd = dequeue(&g_cb);

    pthread_mutex_unlock(&queueMut);
    // end of critical section

    if (clientfd > 0) {
      // printf("Thread\t[%d] handling clientfd[%d]\n", thread->id, clientfd);

      // check if priority server exist
      int followerFd;
      int port = getFollowerPort();

      httpObj *data = malloc(sizeof(httpObj));
      readRequest(data, clientfd);
      // pthread_mutex_lock(&Mut);
      if (processRequest(data, clientfd, thread) < 0) {
        continue;
      }

      // connect to the priority server's port
      // printf("port:%d\n", port);
      while ((followerFd = create_client_socket(port)) < 0) {
        proxyPtr->servers[priorityIdx].isAlive = false;
        port = getFollowerPort();
        if (port < 0)
          break;
      }

      if (port < 0) {
        printf("\thandleConnection: Priority server does not exist\n\n");
        send500(clientfd);
        continue;
      }
      // printf("followerFd:%d\n", followerFd);
      if (followerFd < 0) {
        printf("Thread\t[%d] followerFd invalid\n", thread->id);
        send500(clientfd);
        close(followerFd);
        continue;
      }

      // printf("headerLines:\n%s", data->headerLines);

      // send header request to follower server
      if (send(followerFd, data->headerLines, strlen(data->headerLines), 0) <
          0) {
        printf("Thread\t[%d] sending header request failed\n", thread->id);
        send500(clientfd);
        close(followerFd);
        continue;
      }
      // forward connection and add to cache
      bridge_loop(clientfd, followerFd, data, thread);

      if (thread->hasCache &&
          (data->contentLength > thread->maxFileSizeCache)) {
        printf("FILE[%s] size:%ld exceeds max file size of cache!\n",
               data->inFile, data->contentLength);
      }
      // else
      // printf("file content in cache:\n\n%s\n",
      //        cacheQ->file[getIndex(cacheQ, data->inFile)].buff);

      // if (cacheQ->file[getIndex(cacheQ, data->inFile)].needsUpdate &&
      //     isInCache(cacheQ, data->inFile)) {
      // int idx = getIndex(cacheQ, data->inFile);
      // printf("[+] updated cache[%d] info:\n%s\n", idx,
      //        cacheQ->file[idx].buff);
      // printf("cacheQ->file[%d].contentLength:%ld\n", idx,
      //        cacheQ->file[idx].contentLength);
      // }
      printf("\n-----------CACHE----------\n");
      printCache(cacheQ);
      printf("--------------------------\n");

      // close the connections
      close(clientfd);
      close(followerFd);
      printf("Thread\t[%d] finished processing clientfd[%d]\n", thread->id,
             clientfd);
      free(data);
    } else {
      warn("clientfd == -1");
    }
  }
}

// https://git.ucsc.edu/cse130/spring20-palvaro/achoi15/-/blob/master/asgn3/loadbalancer.c
// Copied from TA Clark's section in spring 2020
/*
 * bridge_connections send up to BUFFER_SIZE bytes from fromfd to tofd
 * fromfd, tofd: valid sockets
 * returns: number of bytes sent, 0 if connection closed, -1 on error
 */

int bridge_connections(int fromfd, int tofd, httpObj *data, threadObj *thread) {
  uint8_t recvline[BUFFER_SIZE + 1];
  memset(recvline, 0, BUFFER_SIZE + 1);
  int n = recv(fromfd, recvline, BUFFER_SIZE, 0);

  if (n < 0) {
    printf("connection error receiving\n");
    return -1;
  } else if (n == 0) {
    printf("receiving connection ended\n");
    return 0;
  }

  recvline[n] = '\0';
  // printf("bridge_connections:");

  if (thread->hasCache) {
    // when a new file is first requested
    // parse GET header attributes
    // and then if size of file is <= max file cache size,
    // enqueue file to cache and set file attributes.
    // will only get executed one time once contentLength is set
    if (data->contentLength == -1) {
      char temp[BUFFER_SIZE + 1];
      strcpy(temp, (char *)recvline);
      // printf("temp:%s\n", temp);
      // set content length to add file to proxy
      char *saveptr = temp;
      char key[64];
      char val[64];
      char *token = strtok_r(temp, "\r\n", &saveptr);
      // printf("token1:%s\n", token);

      // check if file is in follower servers
      if (strstr(token, "HTTP/1.1 200 OK")) {
        data->valid = true;
      } else
        data->valid = false;

      token = strtok_r(NULL, "\r\n", &saveptr);
      // printf("token2:%s\n", token);
      sscanf(token, "%[^:]: %s", key, val);
      data->contentLength = strtoint32(val);
      token = strtok_r(NULL, "\r\n", &saveptr);
      // printf("token3:%s\n", token);
      sscanf(token, "%[^:]: %[^\n]", key, val);

      // printf("data->contentLength:%ld\n", data->contentLength);

      if (!isInCache(cacheQ, data->inFile) &&
          (data->contentLength <= thread->maxFileSizeCache) && data->valid) {
        // printCache(cacheQ);

        if (isFull(cacheQ)) {
          pthread_mutex_lock(&cacheMut);
          // while (isEmpty(cacheQ)) {
          //   // printf("Thread\t[%d] waiting\n", thread->id);
          //   pthread_cond_wait(&cacheCond, &cacheMut);
          // }
          dequeueCache(cacheQ);

          pthread_mutex_unlock(&cacheMut);
        }
        addToCache(cacheQ, data->inFile);
        // enqueueCache(cacheQ, data->inFile);

        int index = getIndex(cacheQ, data->inFile);
        cacheQ->file[index].contentLength = data->contentLength;
        strcpy(cacheQ->file[index].age, val);
        // printf("[+] file age:%s\n",
        //        cacheQ->file[getIndex(cacheQ, data->inFile)].age);

        char *content = strstr((char *)recvline, "\r\n\r\n");
        if (content != NULL) {
          content += 4;
          // printf("header bytes:%ld", content - (char *)recvline);
          cacheQ->file[index].headerSize = content - (char *)recvline;
        }
      }
    }
    // if file in server is newer than the one in cache, update
    // cached file entry's content length, age, and header size.
    if (isInCache(cacheQ, data->inFile) &&
        cacheQ->file[getIndex(cacheQ, data->inFile)].needsUpdate &&
        cacheQ->file[getIndex(cacheQ, data->inFile)].contentLength == -1 &&
        data->valid) {
      char temp[BUFFER_SIZE + 1];
      strcpy(temp, (char *)recvline);
      printf("\tupdateCache\n");

      // set content length to add file to proxy
      char *saveptr = temp;
      char key[64];
      char val[64];
      char *token = strtok_r(temp, "\r\n", &saveptr);
      // printf("token1:%s\n", token);

      if (strstr(token, "HTTP/1.1 200 OK")) {
        data->valid = true;
      } else
        data->valid = false;

      token = strtok_r(NULL, "\r\n", &saveptr);
      // printf("token2:%s\n", token);
      sscanf(token, "%[^:]: %s", key, val);
      data->contentLength = strtoint32(val);

      token = strtok_r(NULL, "\r\n", &saveptr);
      // printf("token3:%s\n", token);
      sscanf(token, "%[^:]: %[^\n]", key, val);
      int idx = getIndex(cacheQ, data->inFile);
      // clearFileEntry(cacheQ, idx);

      cacheQ->file[idx].contentLength = data->contentLength;
      strcpy(cacheQ->file[idx].age, val);

      char *content = strstr((char *)recvline, "\r\n\r\n");
      if (content != NULL) {
        content += 4;
        // printf("new header bytes:%ld", content - (char *)recvline);
        cacheQ->file[idx].headerSize = content - (char *)recvline;
      }
      // printf("[+] updated cache info:%s\n", cacheQ->file[idx].buff);
    }
    // else if ((data->contentLength <= thread->maxFileSizeCache) &&
    //            cacheQ->file[getIndex(cacheQ, data->inFile)].needsUpdate &&
    //            data->valid) {
    //   printf("greater than maxFileSizeCache!\n\tskipping update\n");
    //   printCache(cacheQ);
    // }

    // if file is in cache and meets requirements
    // add recvline to file buffer
    // printf("recvline:\n%s\n", recvline);
    if (isInCache(cacheQ, data->inFile) &&
        (data->contentLength <= thread->maxFileSizeCache) && data->valid) {
      // printf("addToFileBuffer\n");
      int i = getIndex(cacheQ, data->inFile);
      addToFileBuffer(cacheQ, i, recvline, n);
    }
  }

  n = send(tofd, recvline, n, 0);
  if (n < 0) {
    printf("connection error sending\n");
    return -1;
  } else if (n == 0) {
    printf("sending connection ended\n");
    // close(fromfd);
    return 0;
  }
  return n;
}

// Copied from TA Clark's section in spring 2020
/*
 * bridge_loop forwards all messages between both sockets until the connection
 * is interrupted. It also prints a message if both channels are idle.
 * sockfd1, sockfd2: valid sockets
 */
void bridge_loop(int sockfd1, int sockfd2, httpObj *data, threadObj *thread) {
  fd_set set;
  struct timeval timeout;

  int fromfd, tofd;
  bool clientSent = false;
  bool serverReplied = false;
  while (1) {
    // set for select usage must be initialized before each select call
    // set manages which file descriptors are being watched
    FD_ZERO(&set);
    FD_SET(sockfd1, &set);
    FD_SET(sockfd2, &set);
    // printf("sockfd1:%d \nsockfd2:%d \n",sockfd1,sockfd2);

    // same for timeout
    // max time waiting, 5 seconds, 0 microseconds
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    // select return the number of file descriptors ready for reading in set
    switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
    case -1:
      printf("error during select, exiting\n");
      perror("select");
      return;
    case 0:
      printf("both channels are idle, waiting again\n");
      continue;
    default:
      if (FD_ISSET(sockfd1, &set)) {
        fromfd = sockfd1;
        tofd = sockfd2;
      } else if (FD_ISSET(sockfd2, &set)) {
        fromfd = sockfd2;
        tofd = sockfd1;
      } else {
        printf("this should be unreachable\n");
        return;
      }
    }
    if (bridge_connections(fromfd, tofd, data, thread) <= 0) {
      if (!serverReplied && clientSent) {
        printf("Server did not send reply to client.\n");
        send500(sockfd1);
        close(sockfd2);
      }

      return;
    } else {
      if (fromfd == sockfd1) {
        clientSent = true;
      } else if (fromfd == sockfd2) {
        serverReplied = true;
      }
    }
  }
}
void addToCache(queue *q, char *filename) {
  // critical section:
  pthread_mutex_lock(&cacheMut);
  enqueueCache(q, filename);
  // signal condition in bridge_condition
  // pthread_cond_signal(&cacheCond);
  pthread_mutex_unlock(&cacheMut);
  // end of critical section
  return;
}

void addRequest(int clientfd, queue *q) {
  // critical section:
  pthread_mutex_lock(&queueMut);
  enqueue(q, clientfd);
  // signal condition in handleConnection
  pthread_cond_signal(&queueCond);
  pthread_mutex_unlock(&queueMut);
  // end of critical section
  return;
}

int processRequest(httpObj *data, int clientfd, threadObj *thread) {
  if (getDCRLFindex(data->buff) < 0) {
    perror("processRequest: double carriage not found");
    data->valid = false;
  } else
    validateRequest(data);

  if (!data->valid) {
    createErrorResponse(data, 400);
    send(clientfd, data->response, strlen(data->response), 0);
    // printf("[+] err response:\n%s\n", data->response);
    close(clientfd);
    return -1;
  } else if (data->command < 0) {
    createErrorResponse(data, 501);
    send(clientfd, data->response, strlen(data->response), 0);
    // printf("[+] err response:\n%s\n", data->response);
    close(clientfd);
    return -1;
  }

  if (thread->hasCache) {
    // if file is in cache, send head request to priority server
    if (isInCache(cacheQ, data->inFile)) {
      printf("FILE[%s] IS IN cache. Sending HEAD request...\n", data->inFile);

      int port = getFollowerPort();
      int followerFd;
      ssize_t contentLength = -1;
      char temp[5] = "";
      char headRequest[100];
      char date[50];

      sprintf(headRequest, "HEAD /%s HTTP/1.1\r\nHost: localhost:%d\r\n\r\n",
              data->inFile, port);
      // printf("headRequest:%s\n", headRequest);

      while ((followerFd = create_client_socket(port)) < 0) {
        proxyPtr->servers[priorityIdx].isAlive = false;
        port = getFollowerPort();
        if (port < 0)
          break;
      }
      if (port < 0) {
        printf("\tprocessRequest: Priority server does not exist\n\n");
        send500(clientfd);
        if (close(followerFd) < 0)
          warn("close");
        return -1;
      }
      if (send(followerFd, headRequest, strlen(headRequest), 0) < 0) {
        proxyPtr->servers[priorityIdx].isAlive = false;
        send500(clientfd);
        if (close(followerFd) < 0)
          warn("close");
      }
      uint8_t buff[BUFFER_SIZE + 1];
      // recieve response from server
      int bytes = recv(followerFd, buff, BUFFER_SIZE, 0);

      if (bytes < 0) {
        perror("processRequest: recv");
      }
      buff[bytes] = '\0';
      char response[BUFFER_SIZE + 1];
      strcpy(response, (char *)buff);
      // printf("HEAD response:\n%s\n\n", response);
      // process HEAD response
      if (strstr(response, "HTTP/1.1 200 OK") == NULL) {
        proxyPtr->servers[priorityIdx].isAlive = false;
        close(followerFd);
      }
      int idx = getDCRLFindex(response);
      if (idx < 0) {
        proxyPtr->servers[priorityIdx].isAlive = false;
        close(followerFd);
      }
      if (sscanf(response,
                 "HTTP/1.1 200 OK\r\nContent-Length:%ld \r\n\r\n%[^:]: %[^\n]",
                 &contentLength, temp, date) == 3) {
        // printf("\tValid response: Content-Length: %ld date: %s\n",
        //        contentLength, date);
        data->contentLength = contentLength;
        // printf("data->contentLength:%ld\n", data->contentLength);
      } else {
        printf("\tInvalid response!\n\n");
      }
      printf("File date in follower server:%s\n", date);

      // if date of file in cache is same age or newer than the one in
      // server send the file in cache. Else return 0 to forward
      // request.
      // int index = getIndex(cacheQ, data->inFile);
      if (fileIsNewer(cacheQ->file[getIndex(cacheQ, data->inFile)].age, date)) {
        printf("FILE [%s] age in cache is newer/same than follower server! "
               "Sending "
               "file...\n ",
               data->inFile);
        int index = getIndex(cacheQ, data->inFile);

        cacheQ->file[index].needsUpdate = false;
        cacheQ->file[index].contentLength = data->contentLength;
        strcpy(cacheQ->file[index].age, date);
        cacheQ->file[index].headerSize = strlen(response);

        // printf("cacheQ->file[index].contentLength:%ld\n",
        //        cacheQ->file[index].contentLength);
        // printf("cacheQ->file[index].headerSize:%d\n",
        //        cacheQ->file[index].headerSize);

        // printf("SENT in cache:\n\n%s\n", cacheQ->file[index].buff);
        int n = 0;
        if ((n = send(clientfd, cacheQ->file[index].buff,
                      cacheQ->file[index].contentLength +
                          cacheQ->file[index].headerSize,
                      0) < 0)) {
          send500(clientfd);
        }

        if (close(clientfd) < 0)
          warn("close");
        if (close(followerFd) < 0)
          warn("close");
        // printf("\n-----------CACHE----------\n");
        // printCache(cacheQ);
        // printf("--------------------------\n");
        // printf("\t[SUCCESS]\n");
        return -1;
      } else {
        printf("FILE[%s] IN cache is OLDER than in follower server! "
               "Forwarding...\n",
               data->inFile);
        int i = getIndex(cacheQ, data->inFile);
        cacheQ->file[i].needsUpdate = true;
        clearFileEntry(cacheQ, i);

        if (close(followerFd) < 0)
          warn("close");
        return 0;
      }
    } else {
      printf("FILE[%s] NOT IN cache. Forwarding...\n", data->inFile);

      printCache(cacheQ);
    }
  }
  return 0;
}

// read request from client and initialize threads
void readRequest(httpObj *data, int clientfd) {
  data->valid = true;
  // memset(data->buff, 0, BUFFER_SIZE);
  ssize_t bytes = recv(clientfd, data->buff, BUFFER_SIZE, 0);
  if (bytes < 0) {
    perror("recv");
  }

  data->buff[bytes] = '\0';
  strcpy(data->headerLines, data->buff);

  // printf("headerLines:\n%s", headerLines);
  // printf("[+] received %ld bytes from client:\n%s\n", bytes, data->buff);
  return;
}

void *healthcheck() {
  // initial probe
  // pthread_mutex_unlock(&healthMut);
  probeFollowerServers(proxyPtr);

  while (1) {
    // block on healthCond in handleConnection
    pthread_cond_wait(&healthCond, &healthMut);
    probeFollowerServers(proxyPtr);
  }
}

// select the server with the minimum amount of total requests
int getFollowerPort() {
  int idx = -1;

  for (int i = 0; i < proxyPtr->numFollowerServers; i++) {
    if (proxyPtr->servers[i].isAlive) {
      idx = i;
      break;
    } else if (i == proxyPtr->numFollowerServers - 1)
      return -1;
  }
  // printf("priority port1:\n%d", proxyPtr->servers[idx].port);

  for (int i = 0; i < proxyPtr->numFollowerServers; i++) {
    // find min idx of total requests
    if (proxyPtr->servers[i].totalRequests <
        proxyPtr->servers[idx].totalRequests) {
      if (proxyPtr->servers[i].isAlive) {
        idx = i;
      } else if (proxyPtr->servers[i].totalRequests ==
                 proxyPtr->servers[idx]
                     .totalRequests) { // tie breaker or if both have same
        // num of errors choose idx with least errors
        if (proxyPtr->servers[i].numErrors <=
            proxyPtr->servers[idx].numErrors) {
          if (proxyPtr->servers[i].isAlive) {
            idx = i;
          }
        }
      }
    }
  }

  if (idx != -1)
    proxyPtr->servers[idx].totalRequests++;
  else
    return -1;

  // printf("\tPRIORITY PORT:%d\n\n", proxyPtr->servers[idx].port);
  priorityIdx = idx;
  return proxyPtr->servers[idx].port;
}

void probeFollowerServers() {
  printf("==============probeFollowerServers==============\n");
  int followerFd;
  ssize_t contentLength = -1;
  int total = 0;
  int err = 0;
  char temp[5] = "";
  char *cLength = temp;
  char health[100] = "";
  for (int i = 0; i < proxyPtr->numFollowerServers; i++) {
    // connect to server
    printf("Connecting to server port: %d\n", proxyPtr->servers[i].port);
    if ((followerFd = create_client_socket(proxyPtr->servers[i].port)) < 0) {
      proxyPtr->servers[i].isAlive = false;
      continue;
    }

    sprintf(health, "GET /healthcheck HTTP/1.1\r\nHost: localhost:%d\r\n\r\n",
            proxyPtr->servers[i].port);
    // printf("health:%s\n\n", health);
    // send healthcheck
    if (send(followerFd, health, strlen(health), 0) < 0) {
      proxyPtr->servers[i].isAlive = false;
      close(followerFd);
      continue;
    }

    uint8_t buff[BUFFER_SIZE + 1];
    int bytesRecv = 0;
    do {
      // recieve response from server
      int bytes = recv(followerFd, buff + bytesRecv, BUFFER_SIZE, 0);

      if (bytes < 0) {
        perror("probeFollowerServers: recv");
      } else {
        bytesRecv += bytes;
      }

      buff[bytesRecv] = '\0';
      char response[BUFFER_SIZE + 1];
      strcpy(response, (char *)buff);
      // printf("response:\n%s\n\n", response);

      // process healthcheck response
      if (strstr(response, "HTTP/1.1 200 OK") == NULL) {
        proxyPtr->servers[i].isAlive = false;
        close(followerFd);
        continue;
      }

      int idx = getDCRLFindex(response);
      if (idx < 0) {
        proxyPtr->servers[i].isAlive = false;
        close(followerFd);
        continue;
      }
      // https://stackoverflow.com/questions/2799612/how-to-skip-a-line-when-fscanning-a-text-file
      // sscanf ignores "Last-Modified" line
      if (sscanf(response,
                 "HTTP/1.1 200 OK\r\nContent-Length: "
                 "%ld\r\n\r\n%*[^\n]\n%d\n%d\n",
                 &contentLength, &err, &total) != EOF) {
        // printf(
        //     "\tValid healthcheck: Content-Length: %ld err: %d total: %d
        //     \n\n", contentLength, err, total);
        cLength = strstr(response, "\r\n\r\n") + 4;
      } else {
        printf("\tInvalid healthcheck!\n\n");
      }
    } while (contentLength != (ssize_t)strlen(cLength));

    if (followerFd < 0) {
      proxyPtr->servers[i].isAlive = false;
      printf("\tfollowerFd < 0 after parsing\n\n");
    } else {
      proxyPtr->servers[i].numErrors = err;
      proxyPtr->servers[i].totalRequests = total;
      proxyPtr->servers[i].isAlive = true;
      printf("numErrors:%d totalRequests:%d\n\n",
             proxyPtr->servers[i].numErrors,
             proxyPtr->servers[i].totalRequests);
    }
    close(followerFd);
  }
}

int main(int argc, char *argv[]) {
  // set up default values for our environment
  int requestsHealthcheck = 5;
  int numConnections = 5;
  int capacityCache = 3;
  int maxFileSizeCache = 1024;
  int countConnectionsHC = 0;
  bool hasCache = true;
  int opt;
  while ((opt = getopt(argc, argv, "N:R:s:m:")) != -1)
    switch (opt) {
    case 'N': // must be positive
      numConnections = strtouint16(optarg);
      if (numConnections == 0) {
        fprintf(stderr, "Invalid -N value: %s\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    case 'R': // must be positive
      requestsHealthcheck = strtouint16(optarg);
      if (requestsHealthcheck == 0) {
        fprintf(stderr, "Invalid -R value: %s\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    case 's':
      // Any non-negative integer is a valid value for parameters s and m
      // a value of zero (0) on either of them indicates that there will be
      // no caching.
      capacityCache = strtoint32(optarg);
      if (capacityCache == 0) {
        // TODO: set to no caching
        hasCache = false;
      } else if (capacityCache < 0) {
        fprintf(stderr, "Invalid -s value: %s\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    case 'm':
      maxFileSizeCache = strtoint32(optarg);
      if (maxFileSizeCache == 0) {
        // TODO: set to no caching
        hasCache = false;
      } else if (maxFileSizeCache < 0) {
        fprintf(stderr, "Invalid -m value: %s\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    case '?':
      if (optopt == 'N' || optopt == 'R' || optopt == 's' || optopt == 'm')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        fprintf(stderr, "Port is negative or unknown option: `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);

      return EXIT_FAILURE;
    default:
      return EXIT_FAILURE;
    }
  // initialize follower servers
  proxyPtr = malloc(sizeof(proxyObj));
  proxyPtr->numFollowerServers = argc - optind - 1;
  proxyPtr->servers =
      malloc(proxyPtr->numFollowerServers * sizeof(struct followerServerInfo));

  int proxyPort = strtouint16(argv[optind]);
  if (proxyPort == 0) {
    fprintf(stderr, "Invalid port: %s\n", argv[optind]);
    return EXIT_FAILURE;
  }
  printf("proxyPort:%d\n", proxyPort);

  for (int i = optind; i < argc; i++) {
    int index = i - optind - 1;
    if (index == -1)
      continue;
    // printf("index:%d\n", index);
    proxyPtr->servers[index].port = strtouint16(argv[i]);
    if (proxyPtr->servers[index].port == 0) {
      fprintf(stderr, "Invalid port: %s\n", argv[i]);
      return EXIT_FAILURE;
    }
    proxyPtr->servers[index].numErrors = 0;
    proxyPtr->servers[index].totalRequests = 0;
    proxyPtr->servers[index].isAlive = true;
  }

  if (proxyPtr->numFollowerServers == 0) {
    fprintf(stderr, "There must be at least one other port number argument.\n");
    return EXIT_FAILURE;
  }

  printf("N:%d R:%d s:%d m:%d proxyPort:%d numFollowerServers:%d\n",
         numConnections, requestsHealthcheck, capacityCache, maxFileSizeCache,
         proxyPort, proxyPtr->numFollowerServers);

  for (int i = 0; i < proxyPtr->numFollowerServers; i++) {
    printf("server[%d]:%d\n", i, proxyPtr->servers[i].port);
  }

  // initialize healthcheck monitor thread
  pthread_t healthThread;
  pthread_create(&healthThread, NULL, healthcheck, NULL);

  // initialize handleConnection threads
  pthread_t thread[numConnections];
  threadObj *tPtr = NULL;

  for (int idx = 0; idx < numConnections; idx++) {
    tPtr = malloc(sizeof(threadObj));
    tPtr->id = idx;
    tPtr->numFollowerServers = proxyPtr->numFollowerServers;
    tPtr->requestsHealthcheck = &requestsHealthcheck;
    tPtr->countConnectionsHC = &countConnectionsHC;
    tPtr->capacityCache = capacityCache;
    tPtr->maxFileSizeCache = maxFileSizeCache;
    tPtr->hasCache = hasCache;
    int rc =
        pthread_create(&(thread[idx]), NULL, handleConnection, (void *)tPtr);
    if (rc) {
      errx(EXIT_FAILURE, "main: pthread_create() is %d\n", rc);
    }
  }

  int listenfd = create_listen_socket(proxyPort);

  queue *q = initQueue(CIRC_SIZE);
  if (hasCache) {
    cacheQ = initCache(capacityCache, maxFileSizeCache);
    // printf("cacheQ->file[0]:%d\n", cacheQ->file[0].maxFileSizeCache);
    // printf("cacheQ->file[0].filename:%s\n", cacheQ->file[0].filename);
    // strcpy(cacheQ->file[0].filename, "file1");
    // uint8_t bufff[5] = "hi";
    // printf("cacheQ->file[0].filename:%s\n", cacheQ->file[0].filename);
    // enqueueCache(cacheQ, "file1", bufff);
    // enqueueCache(cacheQ, "file2", bufff);
    // enqueueCache(cacheQ, "file3", bufff);
    // // printCache(cacheQ);
    // // enqueueCache(cacheQ, "file4", bufff);

    // if (isFull(cacheQ)) {
    //   char *file = dequeueCache(cacheQ);
    //   // printf("file:%s\n", file);
    //   printCache(cacheQ);
    // }
    // char *file = dequeueCache(cacheQ);
    // printf("isInCache:%d\n", isInCache(cacheQ, "70"));
    // printf("file:%s\n", file);
  }

  // char *old = "Sat, 01 Nov 2021 21:02:32 GMT";
  // char *new = "Wed, 02 Nov 2020 21:02:32 GMT";

  // if (fileIsNewer(new, old)) {
  //   printf("\ntrue\n");
  // } else
  //   printf("false\n");

  while (1) {
    int clientfd = accept(listenfd, NULL, NULL);
    if (clientfd < 0) {
      warn("accept error");
      continue;
    }
    addRequest(clientfd, q);
    countConnectionsHC++;
    printf("countConnections:%d\n", *(tPtr->countConnectionsHC));
    // check if a healthcheck is due
    if (*(tPtr->countConnectionsHC) % *(tPtr->requestsHealthcheck) == 0) {
      printf("**************Healthcheck is due**************\n");
      // signal the health monitoring thread
      pthread_cond_signal(&healthCond);
    }
  }
  freeQueue(q);
  return EXIT_SUCCESS;
}
