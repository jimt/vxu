#
#	configurable file locations
#

PREFIX=/usr/local
INSTALLDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man/man1

CC = gcc
CFLAGS += -Wall

# user configuration section end

all : vxur vxuw

clean : 
	rm -f *.o vxur vxuw
distclean :
	make clean
	rm -f *~ *.swp *.tgz tags

install: vxur vxuw
	@echo
	@echo INSTALLING vxur vxuw in $(INSTALLDIR)
	strip vxur vxuw
	cp vxur vxuw $(INSTALLDIR)
	@echo
	@echo INSTALLING man files
	cp vxur.1 vxuw.1 $(MANDIR)

vxur:	vxur.c vxu.h
	$(CC) $(CFLAGS) -o vxur vxur.c

vxuw: 	vxuw.c vxu.h
	$(CC) $(CFLAGS) -o vxuw vxuw.c

