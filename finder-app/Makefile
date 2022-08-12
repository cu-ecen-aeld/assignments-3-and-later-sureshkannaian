INCLUDE_DIRS = 
LIB_DIRS = 
CROSS_COMPILE=

CDEFS=
CFLAGS= -g -Wall $(INCLUDE_DIRS) $(CDEFS) 
LIBS= -lrt


SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	writer

clean:
	-rm -f *.o
	-rm -f writer


writer: writer.o
	$(CROSS_COMPILE)gcc $(LDFLAGS) $(CFLAGS) -o $@ $@.o  -lpthread -lrt

depend:
	
.c.o:
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $<
