CROSS_COMPILE=
CFLAGS= -g -Wall -Werror

default: aesdsocket

all: aesdsocket

aesdsocket:
	$(CROSS_COMPILE)$(CC) $(CFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm -f *.o
	rm -f /var/tmp/aesdsocketdata
	rm -f aesdsocket