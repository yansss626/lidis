
CC = gcc
FLAGS = -I ./NtyCo/core/ -I ./ -L ./NtyCo/ -lntyco -lpthread -luring -ldl
SRCS = kvstore.c ./server/ntyco.c ./server/proactor.c ./engine/kvs_array.c ./engine/kvs_rbtree.c ./engine/kvs_hash.c ./server/reactor.c ./modules/kvs_log.c ./modules/kvs_save.c
TESTCASE_SRCS = testcase.c
TARGET = kvstore
SUBDIR = ./NtyCo/
TESTCASE = testcase

TESTCASE1.1 = set
TESTCASE1.1_SRCS = ./module_testcase/set.c

TESTCASE1.2 = get
TESTCASE1.2_SRCS = ./module_testcase/get.c

TESTCASE1.3 = set_save
TESTCASE1.3_SRCS = ./module_testcase/set_save.c

OBJS = $(SRCS:.c=.o)


all: $(SUBDIR) $(TARGET) $(TESTCASE) $(TESTCASE1.1) $(TESTCASE1.2) $(TESTCASE1.3)

$(SUBDIR): ECHO
	make -C $@

ECHO:
	@echo $(SUBDIR)

$(TARGET): $(OBJS) 
	$(CC) -o $@ $^ $(FLAGS)

$(TESTCASE): $(TESTCASE_SRCS)
	$(CC) -o $@ $^

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


