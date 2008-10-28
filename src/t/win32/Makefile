WARNINGS=-Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k \
	-Wstrict-prototypes

CFLAGS	=-DWINVER=0x0500  $(WARNINGS)
CPPFLAGS = -I../../libc

# Test cases
TESTS	= waitpid-test-01

# useful progs
PROGS	= prog01

EXES	= $(TESTS:=.exe) $(PROGS:=.exe)

all: $(EXES)

%.exe: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< ../../libc/libc.a -lws2_32

clean:
	rm -f $(TESTS) $(PROGS)

clobber:
	rm -f $(EXES)
