CC ?= cc
CFLAGS ?= -O2 -pipe
LDFLAGS ?= -Wl,-s
DESTDIR ?=
BIN_DIR ?= /bin
MAN_DIR ?= /usr/share/man
DOC_DIR ?= /usr/share/doc

CFLAGS += -pedantic
LIBS = -lm

INSTALL = install -v

imagemove: *.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS)
clean:
	rm -f imagemove 
