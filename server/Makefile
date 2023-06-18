CFLAGS += -Wall -Werror

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS) 

clean:
	rm -f aesdsocket
