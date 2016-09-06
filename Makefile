CFLAGS?=-O2 -g -Wall -W -I/usr/include/ -I/usr/include/libusb-1.0 
LDLIBS+= -lusb-1.0  -lmirsdrapi-rsp 
CC?=gcc
PROGNAME=sdrplayserver

all: sdrplayserver

%.o: %.c
	$(CC) $(CFLAGS) -c $<

sdrplayserver: sdrplayserver.o 
	$(CC) -g -o sdrplayserver sdrplayserver.o  $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o sdrplayserver
