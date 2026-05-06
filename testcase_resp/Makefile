CC = gcc

LDFLAGS = -lhiredis

TESTCASE1.1 = set
TESTCASE1.1_SRCS = ./testcase_sample/set.c

TESTCASE1.2 = get
TESTCASE1.2_SRCS = ./testcase_sample/get.c

TESTCASE1.3 = set_save
TESTCASE1.3_SRCS = ./testcase_sample/set_save.c

TESTCASE1.4 = del
TESTCASE1.4_SRCS = ./testcase_sample/del.c

TESTCASE2.1 = set_blog
TESTCASE2.1_SRCS = ./testcase_sample/set_blog.c

TESTCASE2.2 = get_blog
TESTCASE2.2_SRCS = ./testcase_sample/get_blog.c

TESTCASE3 = multicmd
TESTCASE3_SRCS = ./testcase_sample/multicmd.c

TESTCASE1 = basic_function
TESTCASE1_SRCS = ./testcase_sample/basic_function.c



all: $(TESTCASE1) $(TESTCASE1.1) $(TESTCASE1.2) $(TESTCASE1.3) $(TESTCASE1.4) $(TESTCASE2.1) $(TESTCASE2.2) $(TESTCASE3) \
	



$(TESTCASE1): $(TESTCASE1_SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TESTCASE1.1): $(TESTCASE1.1_SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TESTCASE1.2): $(TESTCASE1.2_SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TESTCASE1.3): $(TESTCASE1.3_SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TESTCASE1.4): $(TESTCASE1.4_SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TESTCASE2.1): $(TESTCASE2.1_SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TESTCASE2.2): $(TESTCASE2.2_SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)

	
$(TESTCASE3): $(TESTCASE3_SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)



clean: 
	rm -rf $(TESTCASE1) $(TESTCASE1.1) $(TESTCASE1.2) $(TESTCASE1.3) $(TESTCASE1.4) $(TESTCASE2.1) $(TESTCASE2.2) $(TESTCASE3) \
	