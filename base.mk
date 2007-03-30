CPPFLAGS:=\
	-I$(BUILD_BASE) \
	-Wall \
	-std=gnu89 \
	-pedantic \
	-D"VERSION=\"1.0p0\"" \
	-D"BUILD_DATE=\"`date -u`\""
CFLAGS:=-ggdb -pg -Wmissing-prototypes
LDFLAGS:=
LDLIBS:=

## SPECIAL RULES

all : depend

.PHONY : all clean clean-all clean-backup clean-depend depend install

clean : clean-depend
	$(RM) $(ALL_OBJS)

clean-all : clean
	$(RM) $(ALL_EXEC)

clean-backup :
	$(RM) *~

clean-depend :
	$(RM) *.depend

depend :

install :

## RULES
%.so : %.o
	$(CC) $(CFLAGS) $(CPPFLAGS) -shared $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
