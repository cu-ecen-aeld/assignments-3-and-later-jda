CFLAGS += -Wall -Werror
LDFLAGS += -pthread

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.o timestamp.o helpers.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

timestamp.o: timestamp.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ $(LDFLAGS)

helpers.o: helpers.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ $(LDFLAGS)

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ $(LDFLAGS)


clean:
	rm -f aesdsocket
	rm -f *.o
