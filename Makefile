PROG=	mcc
SRCS=	mcc.c write_32.c write_64.c

CFLAGS=		-O2 -fstack-protector -D_FORTIFY_SOURCE=2 -pie -fPIE
LDFLAGS=	-Wl,-z,now -Wl,-z,relro

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PROG).out $(SRCS)

debug: $(SRCS)
	$(CC) -g -Wall -Wextra -Wconversion -Wundef -o $(PROG).out $(SRCS)

clean:
	rm -f $(PROG).out
