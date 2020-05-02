# telnet.mk : makefile options for Telnet library
#
# Jon Mayo - March 20, 2007
#

TELNET_EXEC:=telnet
TELNET_SRCS:=telnet.c
TELNET_OBJS:=$(TELNET_SRCS:.c=.o)

ALL_SRCS+=$(TELNET_SRCS)
ALL_OBJS+=$(TELNET_OBJS)
ALL_EXEC+=$(TELNET_EXEC)

all : $(TELNET_EXEC)

$(TELNET_EXEC) : $(TELNET_OBJS)

$(TELNET_OBJS) : CPPFLAGS:=-DUNIT_TEST

depend : telnet.depend

telnet.depend : $(TELNET_SRCS)
	$(CC) $(CPPFLAGS) -MM $^ >$@
