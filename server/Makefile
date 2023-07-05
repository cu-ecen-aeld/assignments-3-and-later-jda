CFLAGS += -Wall -Werror

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.o timestamp.o helpers.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ -pthread -ggdb -O0 $(LDFLAGS)

timestamp.o: timestamp.c 
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -Wall -pthread -ggdb -O0 $(LDFLAGS)

helpers.o: helpers.c 
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -Wall -pthread -ggdb -O0 $(LDFLAGS)

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -pthread -ggdb -O0 $(LDFLAGS)


clean:
	rm -f aesdsocket
	rm *.o
