CFLAGS := -Wall -W -Os -g -MMD
E := test_telnet
S := test_telnet.c
O := $(S:.c=.o)
all :: $E
$(eval clean :: ; $$(RM) $E $O)
$E : $O
##
E := example
S := example.c
O := $(S:.c=.o)
all :: $E
$(eval clean :: ; $$(RM) $E $O)
$E : $O
##
DEPS := $(wildcard *.d)
clean-all : clean ; $(RM) $(DEPS)
include $(DEPS)
