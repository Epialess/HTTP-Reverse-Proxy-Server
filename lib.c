#include "httpproxy.h"

/**
   Creates a socket for listening for connections.
   Closes the program and prints an error message on error.
 */
int create_listen_socket(uint16_t port) {
  struct sockaddr_in addr;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    err(EXIT_FAILURE, "socket error");
  }

  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htons(INADDR_ANY);
  addr.sin_port = htons(port);
  // Configure server socket
  int enable = 1;
  int ret =
      setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

  ret = bind(listenfd, (struct sockaddr *)&addr, sizeof addr);
  if (ret < 0) {
    err(EXIT_FAILURE, "bind error");
  }

  if (listen(listenfd, 500) < 0) {
    err(EXIT_FAILURE, "listen error");
  }

  return listenfd;
}

/**
   Creates a socket for connecting to a server running on the same
   computer, listening on the specified port number.  Returns the
   socket file descriptor on succes.  On failure, returns -1 and sets
   errno appropriately.
 */
int create_client_socket(uint16_t port) {
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd < 0) {
    return -1;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (connect(clientfd, (struct sockaddr *)&addr, sizeof addr)) {
    return -1;
  }
  return clientfd;
}

const char ERROR500[] =
    "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";

void send500(int fd) {
  printf("Sending 500 response...\n");
  char error[100] = "";
  strcat(error, ERROR500);
  send(fd, error, strlen(error), 0);
  close(fd);
}

/**
   Converts a string to an 16 bits unsigned integer.
   Returns 0 if the string is malformed or out of the range.
 */
uint16_t strtouint16(char number[]) {
  char *last;
  long num = strtol(number, &last, 10);
  if (num <= 0 || num > UINT16_MAX || *last != '\0') {
    return 0;
  }
  return num;
}

int32_t strtoint32(char number[]) {
  char *last;
  long num = strtol(number, &last, 10);
  if (num <= -1 || num > INT32_MAX || *last != '\0') {
    return -1;
  }
  return num;
}

void createHttpResponse(httpObj *data, char *type, ssize_t fsize, char *msg) {
  snprintf(data->response, sizeof(data->response),
           "HTTP/1.1 %s\r\nContent-Length: %ld\r\n\r\n%s", type, fsize, msg);
  return;
}

// https://www.thegeekstuff.com/2010/10/linux-error-codes/
void createErrorResponse(httpObj *data, int errn) {
  data->valid = false;

  switch (errn) {
  case 400:
    createHttpResponse(data, "400 Bad Request", sizeof("Bad Request"),
                       "Bad Request\n");
    data->statusCode = 400;
    break;
  case 13:
    createHttpResponse(data, "403 Forbidden", sizeof("Forbidden"),
                       "Forbidden\n");
    data->statusCode = 403;
    break;
  case 21:
  case 2:
    createHttpResponse(data, "404 File Not Found", sizeof("File Not Found"),
                       "File Not Found\n");
    data->statusCode = 404;
    break;
  case 501:

    createHttpResponse(data, "501 Not Implemented", sizeof("Not Implemented"),
                       "Not Implemented\n");
    data->statusCode = 501;
    break;
  default:

    createHttpResponse(data, "500 Internal Server Error",
                       sizeof("Internal Server Error"),
                       "Internal Server Error\n");
    data->statusCode = 500;
  }

  return;
}

int getDCRLFindex(char *buff) {
  char *result = strstr(buff, "\r\n\r\n");
  if (result == NULL)
    return -1;
  int index = result - buff + 4;
  return index;
}

// check filename
bool isValidFileName(char *line) {
  const char validChar[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz._";

  ssize_t leng = strlen(line);
  // printf("leng:%ld\n", leng);

  if (leng == 0 || leng > 19)
    return false;

  for (ssize_t i = 0; i < leng; i++) {
    // character in line[] is not in validChar[]
    if (strchr(validChar, line[i]) == NULL)
      return false;
  }

  return true;
}

void parseRequestLine(httpObj *data, char *line) {
  char *cmd = malloc(10);
  char *file = malloc(21);
  char *version = malloc(10);

  if (sscanf(line, "%s %s %s", cmd, file, version) != 3) {
    // printf("[+] line:%s\n", line);
    data->valid = false;
    return;
  }

  if (strcmp(cmd, "GET") == 0) {
    data->command = GET;
  } else
    data->command = -1;

  strcpy(data->commandStr, cmd);

  if (file[0] != '/') {
    data->valid = false;
    // printf("[+] invalid filename:%s\n", file);
    return;
  } else if (file[0] == '/') {
    // remove '/' from file
    char *fn = file + 1;
    if (isValidFileName(fn))
      strcpy(data->inFile, fn);
    else {
      data->valid = false;
      // printf("[+] invalid filename:%s\n", fn);
      return;
    }
  }

  // check protocol
  if (strcmp(version, "HTTP/1.1") == 0) {
    strcpy(data->version, version);
  } else
    data->valid = false;

  if (data->valid) {
    sprintf(data->requestLine, "%s /%s %s", data->commandStr, data->inFile,
            data->version);
    // printf("[+] requestLine:%s\n", data->requestLine);
  }

  free(cmd);
  free(file);
  free(version);
  return;
}

void validateRequest(httpObj *data) {
  data->contentLength = -1;
  char *saveptr = data->buff;
  char *token = strtok_r(data->buff, "\r\n", &saveptr);
  parseRequestLine(data, token);
  // printf("Token:%s\n", token);
  memcpy(data->first1KB, token, 1001);
  // printf("data->first1KB:%s\n", data->first1KB);

  char val[1024];
  char key[64];
  char domain[32];
  char port[32];
  int cnt = 0;
  token = strtok_r(NULL, "\r\n", &saveptr);
  // printf("Token:%s\n", token);

  while (token != NULL) {
    // %[^:]: "Any character except ':'" :
    if (sscanf(token, "%[^:]: %s", key, val) != 2) {
      // printf("Bad token:%s\n", token);
      warn("Request format error: string:string");
      data->valid = false;
      break;
    }
    if (strstr(key, "Host") != NULL) {
      if (sscanf(val, "%[^:]:%s", domain, port) != 2) {
        warn("Host header format error: string:string");
        data->valid = false;
        break;
      }
      strcpy(data->host, val);
    }
    if (strstr(key, "Content-Length") != NULL) {
      // printf("val:%s\n", val);
      data->contentLength = strtoint32(val);
      // printf("[+] contentLength:%ld\n", data->contentLength);
      cnt++;
    }
    if (cnt > 2) {
      warn("Multiple Host or Content-Length headers");
      data->valid = false;
      break;
    }
    token = strtok_r(NULL, "\r\n", &saveptr);
  }
  return;
}

static const char *months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};
// returns true if date of file in proxy is same age or newer
// than the one in follower server else returns false
bool fileIsNewer(char *proxyDate, char *followerDate) {
  char monthStr[4];
  char monthStr2[4];

  int s_day, p_day;
  int s_year, p_year;
  int s_hour, p_hour;
  int s_min, p_min;
  int s_sec, p_sec;
  int s_month, p_month;

  sscanf(followerDate, "%*[a-zA-Z,] %d %3s %d %d:%d:%d", &s_day, monthStr,
         &s_year, &s_hour, &s_min, &s_sec);

  for (int i = 0; i < 12; i++) {
    if (strncmp(monthStr, months[i], 3) == 0) {
      s_month = ++i;
      break;
    }
  }
  // printf("serverDate\nday:%d\nmonth:%d\nyear:%d\n%d:%d:%d\n", s_day, s_month,
  //        s_year, s_hour, s_min, s_sec);

  sscanf(proxyDate, "%*[a-zA-Z,] %d %3s %d %d:%d:%d", &p_day, monthStr2,
         &p_year, &p_hour, &p_min, &p_sec);

  for (int i = 0; i < 12; i++) {
    if (strncmp(monthStr2, months[i], 3) == 0) {
      p_month = ++i;
      break;
    }
  }
  // printf("cacheDate\nday:%d\nmonth:%d\nyear:%d\n%d:%d:%d\n", p_day, p_month,
  //        p_year, p_hour, p_min, p_sec);

  if (s_year < p_year)
    return true;
  else if (s_year > p_year)
    return false;
  else {
    if (s_month < p_month)
      return true;
    else if (s_month > p_month)
      return false;

    if (s_month == p_month) {
      if (s_day < p_day)
        return true;
      else if (s_day > p_day)
        return false;

      if (s_day == p_day) {
        if (s_hour < p_hour)
          return true;
        else if (s_hour > p_hour)
          return false;

        if (s_hour == p_hour) {
          if (s_min < p_min)
            return true;
          else if (s_min > p_min)
            return false;

          if (s_min == p_min) {
            if (s_sec <= p_sec)
              return true;
            else if (s_sec > p_sec)
              return false;
          }
        }
      }
    }
  }
  return false;
}

// void addFileToProxy(httpObj *data, threadObj *thread) {
//   int followerFd;
//   int port = getFollowerPort();

//   while ((followerFd = create_client_socket(port)) < 0) {
//     proxyPtr->servers[priorityIdx].isAlive = false;
//     port = getFollowerPort();
//     if (port < 0) break;
//   }
//   if (port < 0) {
//     printf("addFileToProxy: Priority server does not exist\n\n");
//     return;
//   }
//   if (send(followerFd, data->headerLines, strlen(data->headerLines), 0) < 0)
//   {
//     printf("addFileToProxy: sending header request failed\n");
//     close(followerFd);
//     return;
//   }

//   // printf("data->contentLength:%ld\n", data->contentLength);

//   if (data->contentLength != -1) {
//     if (data->contentLength <= thread->maxFileSizeCache) {
//       int fd = open(data->inFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
//       // printf("inFile:%s\n", data->inFile);
//       if (flock(fd, LOCK_EX) < 0) {
//         warn("flock put");
//       }
//       memset(data->buff, 0, sizeof(BUFFER_SIZE));

//       // remove header
//       int n;
//       while ((n = readLine(followerFd, data->buff, BUFFER_SIZE)) > 0) {
//         if (strcmp(data->buff, "\r\n") == 0) break;
//       }
//       // read from file in follower server and write file to proxy server.
//       readAndWrite(data, followerFd, fd);
//       printCache(cacheQ);
//       if (isFull(cacheQ)) {
//         char *fileToDelete = dequeueCache(cacheQ);
//         if (remove(fileToDelete) == 0) {
//           printf("'%s' is deleted successfully.\n", fileToDelete);
//         } else {
//           printf("'%s' is not deleted.\n", fileToDelete);
//         }
//       }
//       enqueueCache(cacheQ, data->inFile);
//       if (close(fd) < 0) warn("close");
//       // close(followerFd);
//     }
//   }
//   return;
// }