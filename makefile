# Makefile
# source object target
SRC	:=  raft.h epoll_server.h epoll_server.cpp raft.cpp main.cpp -lpthread
TARGET	:= a
# compile and lib parameter
CC		:= g++
LDFLAGS := -L.
INCLUDE := -I.
all:
	$(CC) -o $(TARGET) $(SRC)
# clean
clean:
	rm -fr *.o
	rm -fr $(TARGET)
