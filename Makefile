CC = gcc
CFLAGS = -g -Wall -Wextra -Wpedantic -Wshadow -pthread
TARGET = httpproxy

SOURCES	 = httpproxy.c	lib.c circularQueue.c
OBJECTS	 = httpproxy.o	lib.o circularQueue.o
HEADERS	 = httpproxy.h

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(HEADERS)
	$(CC) -o $(TARGET) $(OBJECTS) $(CFLAGS)

$(OBJECTS) : $(SOURCES) $(HEADERS)
	$(CC) -c $(SOURCES) $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJECTS)

check:
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)