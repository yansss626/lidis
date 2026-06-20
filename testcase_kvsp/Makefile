CC = gcc

CFLAGS = -I../ -I../kvs-module/

SRC = ../kvs-protocol/kvs_protocol.c

TARGETS = set get set_save del set_blog get_blog multicmd

all: $(TARGETS)

set: ./testcase_sample/set.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

get: ./testcase_sample/get.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

set_save: ./testcase_sample/set_save.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

del: ./testcase_sample/del.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

set_blog: ./testcase_sample/set_blog.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

get_blog: ./testcase_sample/get_blog.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

multicmd: ./testcase_sample/multicmd.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)

