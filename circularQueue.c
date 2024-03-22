#include "httpproxy.h"

queue *initQueue(int size) {
  queue *q = &g_cb;
  q->size = size;
  q->buff = gCbuff;
  return q;
}

queue *initCache(int size, int maxFileSizeCache) {
  queue *cacheQ = (queue *)malloc(sizeof(queue));
  memset(cacheQ, 0, sizeof(queue));
  cacheQ->size = size;

  cacheQ->file = malloc(size * sizeof(struct fileInfo));
  int sizeName = 19;
  for (int i = 0; i < size; ++i) {
    cacheQ->file[i].filename = (char *)malloc(sizeName * sizeof(char *));
    cacheQ->file[i].maxFileSizeCache = maxFileSizeCache;
    cacheQ->file[i].contentLength = -1;
    cacheQ->file[i].buff =
        (uint8_t *)malloc(((maxFileSizeCache + 100) * sizeof(uint8_t *)));
    cacheQ->file[i].age = (char *)malloc(50 * sizeof(char *));
    cacheQ->file[i].needsUpdate = false;
  }

  return cacheQ;
}

void freeQueue(queue *q) {
  free(q->buff);
  free(q);
  return;
}

bool isEmpty(queue *q) { return q->head == q->tail; }

bool isFull(queue *q) {
  if (q->count && (q->tail % q->size) == q->head) {
    return true;
  } else
    return false;
}

void enqueue(queue *q, int data) {
  int tail = q->tail;

  if (q->count && (tail % q->size) == q->head) {
    // printf("Overflow Queue[%d] %d lost\n", q->head, q->buff[q->head]);
    q->head = (q->head + 1) % q->size;
    q->count--;
  }
  // printf("Added Queue[%d] = %d\n", q->tail, data);
  q->buff[q->tail] = data;
  q->tail = (q->tail + 1) % q->size;
  q->count++;
  return;
}

int dequeue(queue *q) {
  int head = q->head;
  int ret = -1;
  if (q->count <= 0) {
    // printf("Buffer is empty\n");
    return ret;
  }
  if (q->count || (head % q->size) != q->tail) {
    // printf("Removed Queue[%d] = %d\n", q->head, q->buff[q->head]);
    ret = q->buff[q->head];
    q->head = (q->head + 1) % q->size;
    q->count--;
  } else {
    // printf("Buffer is empty\n");
  }
  return ret;
}

void printQueue(queue *q) {
  int head = q->head;
  int tail = q->tail;
  int count = 0;
  for (int i = head; count < q->count; i = (i + 1) % q->size) {
    // printf("Queue[%d] = %d\n", i, q->buff[i]);
    count++;
    if (i == (tail - 1)) {
      break;
    }
  }
  return;
}

void printCache(queue *q) {
  int head = q->head;
  int tail = q->tail;
  int count = 0;
  for (int i = head; count < q->count; i = (i + 1) % q->size) {
    printf("Cache[%d] = %s\n", i, q->file[i].filename);
    count++;
    if (i == (tail - 1)) {
      break;
    }
  }
  return;
}

bool isInCache(queue *q, char *filename) {
  int head = q->head;
  int tail = q->tail;
  bool found = false;
  int count = 0;
  for (int i = head; count < q->count; i = (i + 1) % q->size) {
    // printf("Cache[%d] = %s\n", i, q->file[i].filename);
    if (strcmp(q->file[i].filename, filename) == 0) {
      found = true;
    }
    count++;
    if (i == (tail - 1)) {
      break;
    }
  }
  return found;
}

int getIndex(queue *q, char *filename) {
  int head = q->head;
  int tail = q->tail;
  int count = 0;
  int idx = -1;

  for (int i = head; count < q->count; i = (i + 1) % q->size) {
    // printf("Cache[%d] = %s\n", i, q->file[i].filename);
    if (strcmp(q->file[i].filename, filename) == 0) {
      idx = i;
      break;
    }
    count++;
    if (i == (tail - 1)) {
      idx = i;
      break;
    }
  }
  return idx;
}

void enqueueCache(queue *q, char *filename) {
  int tail = q->tail;

  if (q->count && (tail % q->size) == q->head) {
    printf("Overflow Cache[%d] %s lost\n", q->head, q->file[q->head].filename);
    clearFileEntry(q, q->head);
    q->head = (q->head + 1) % q->size;
    q->count--;
  }
  printf("Added Cache[%d] = %s\n", q->tail, filename);
  q->file[q->tail].filename =
      (char *)malloc((sizeof filename + 1) * sizeof(char));
  strcpy(q->file[q->tail].filename, filename);

  q->tail = (q->tail + 1) % q->size;
  q->count++;
  printCache(q);
  return;
}

char *dequeueCache(queue *q) {
  int head = q->head;
  char *out = q->file[q->head].filename;
  if (q->count <= 0) {
    // printf("Buffer is empty\n");
  }
  if (q->count || (head % q->size) != q->tail) {
    printf("Removed Cache[%d] = %s\n", q->head, q->file[q->head].filename);
    clearFileEntry(q, q->head);
    q->head = (q->head + 1) % q->size;
    q->count--;
  } else {
    // printf("Buffer is empty\n");
  }
  return out;
}

void clearFileEntry(queue *q, int index) {
  printf("Clearing Cache[%d] = %s\n", index, q->file[index].filename);
  memset(q->file[index].buff, 0, sizeof(*q->file[index].buff));
  memset(q->file[index].age, 0, sizeof(*q->file[index].age));
  q->file[index].contentLength = -1;
  q->file[index].sizeBuff = 0;
}

void addToFileBuffer(queue *q, int index, uint8_t *recvline, int n) {
  memcpy(q->file[index].buff + q->file[index].sizeBuff, recvline, n);
  q->file[index].sizeBuff += n;
}