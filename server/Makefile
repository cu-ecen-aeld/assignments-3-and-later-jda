CFLAGS += -Wall -Werror

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ -pthread -ggdb -O0 $(LDFLAGS)

clean:
	rm -f aesdsocket
