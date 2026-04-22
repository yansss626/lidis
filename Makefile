
CC = gcc
FLAGS = -I ./NtyCo/core/ -I ./ -L ./NtyCo/ -lntyco -lpthread -luring -ldl -libverbs -lrdmacm -I ./kvs-module/
SRCS = kvstore.c ./kvs-server/ntyco.c ./kvs-server/proactor.c ./kvs-server/reactor.c \
	./kvs-engine/kvs_array.c ./kvs-engine/kvs_rbtree.c ./kvs-engine/kvs_hash.c ./kvs-engine/kvs_skiptable.c \
	./kvs-module/kvs_log.c ./kvs-module/kvs_save.c ./kvs-module/kvs_sync.c ./kvs-module/rdma_com.c
TARGET = kvstore
SUBDIR = ./NtyCo/


OBJ_DIR = objs
OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRCS))

all: $(SUBDIR) $(TARGET) 

$(SUBDIR): ECHO
	make -C $@

ECHO:
	@echo $(SUBDIR)

$(TARGET): $(OBJS) 
	$(CC) -o $@ $^ $(FLAGS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(FLAGS) -c $< -o $@

clean: 
	rm -rf $(OBJ_DIR) $(TARGET) 


