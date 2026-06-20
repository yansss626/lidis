CC = gcc

CFLAGS = -I../../ -I../../kvs-module/

SRC = ../../kvs-protocol/kvs_protocol.c

TARGETS = test_set_qps test_get_qps test_save_qps

all: $(TARGETS)

test_set_qps: ./test_set_qps.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_get_qps: ./test_get_qps.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_save_qps: ./test_save_qps.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)

