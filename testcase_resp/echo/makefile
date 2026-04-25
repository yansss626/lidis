CC = gcc


ECHO_CLIENT_SRC = echo_client.c
ECHO_CLIENT = echo_client

all: $(ECHO_CLIENT)



$(ECHO_CLIENT): $(ECHO_CLIENT_SRC)
	$(CC) -o $@ $^

clean:
	rm -rf $(ECHO_CLIENT)