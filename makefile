CFLAGS := -Wall -W -Os -MMD
E := test_telnet
S := test_telnet.c
O := $(S:.c=.o)
all :: $E
clean :: ; $(RM) $E $O
$E : $O
##
DEPS := $(wildcard *.d)
clean-all : clean ; $(RM) $(DEPS)
include $(DEPS)
