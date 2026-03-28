
CC = gcc
FLAGS = -I ./NtyCo/core/ -I ./ -L ./NtyCo/ -lntyco -lpthread -luring -ldl
SRCS = kvstore.c ./kvs-server/ntyco.c ./kvs-server/proactor.c ./kvs-engine/kvs_array.c ./kvs-engine/kvs_rbtree.c ./kvs-engine/kvs_hash.c ./kvs-server/reactor.c ./kvs-module/kvs_log.c ./kvs-module/kvs_save.c

TARGET = kvstore
SUBDIR = ./NtyCo/


TESTCASE1.1 = set
TESTCASE1.1_SRCS = ./testcase/set.c

TESTCASE1.2 = get
TESTCASE1.2_SRCS = ./testcase/get.c

TESTCASE1.3 = set_save
TESTCASE1.3_SRCS = ./testcase/set_save.c

OBJS = $(SRCS:.c=.o)


all: $(SUBDIR) $(TARGET) $(TESTCASE1.1) $(TESTCASE1.2) $(TESTCASE1.3)

$(SUBDIR): ECHO
	make -C $@

ECHO:
	@echo $(SUBDIR)

$(TARGET): $(OBJS) 
	$(CC) -o $@ $^ $(FLAGS)

$(TESTCASE1.1): $(TESTCASE1.1_SRCS)
	$(CC) -o $@ $^

$(TESTCASE1.2): $(TESTCASE1.2_SRCS)
	$(CC) -o $@ $^	

$(TESTCASE1.3): $(TESTCASE1.3_SRCS)
	$(CC) -o $@ $^	

%.o: %.c
	$(CC) $(FLAGS) -c $^ -o $@

clean: 
	rm -rf $(OBJS) $(TARGET) $(TESTCASE) $(TESTCASE1.1) $(TESTCASE1.2) $(TESTCASE1.3)


