
CC = gcc
FLAGS = -I ./NtyCo/core/ -I ./ -L ./NtyCo/ -lntyco -lpthread -luring -ldl
SRCS = kvstore.c ./server/ntyco.c ./server/proactor.c ./engine/kvs_array.c ./engine/kvs_rbtree.c ./engine/kvs_hash.c ./server/reactor.c ./modules/kvs_log.c
TESTCASE_SRCS = testcase.c
TARGET = kvstore
SUBDIR = ./NtyCo/
TESTCASE = testcase

OBJS = $(SRCS:.c=.o)


all: $(SUBDIR) $(TARGET) $(TESTCASE)

$(SUBDIR): ECHO
	make -C $@

ECHO:
	@echo $(SUBDIR)

$(TARGET): $(OBJS) 
	$(CC) -o $@ $^ $(FLAGS)

$(TESTCASE): $(TESTCASE_SRCS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(FLAGS) -c $^ -o $@

clean: 
	rm -rf $(OBJS) $(TARGET) $(TESTCASE)


