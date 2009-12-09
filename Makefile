BINDIR=/usr/bin
LOCALESDIR=/usr/share/locale
MANDIR=/usr/share/man/man8
WARNFLAGS=-Wall  -W -Wshadow
CFLAGS?=-O1 -g ${WARNFLAGS}
CC?=gcc


OBJS = device.o lsusb.o


lsusb: $(OBJS) Makefile usb.h list.h
	$(CC) ${CFLAGS} $(LDFLAGS) $(OBJS) -ludev -o lsusb


clean:
	rm -f *~ lsusb *.o

