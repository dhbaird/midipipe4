CFLAGS += -Irtmidi-1.0.7 -D__LINUX_ALSASEQ__
CFLAGS += -Iportmidi-82-20080614/portmidi-tmp/porttime
CFLAGS += -Iportmidi-82-20080614/portmidi-tmp/pm_common
CFLAGS += -O2
CXXFLAGS = $(CFLAGS)
LDFLAGS += -lasound
CC = g++

all: midipipe4

clean:
	-rm *.o

PMLIBS = 
PMLIBS += portmidi-82-20080614/portmidi-tmp/pm_linux/libportmidi.a
PMLIBS += portmidi-82-20080614/portmidi-tmp/porttime/libporttime.a

midipipe4.o: midipipe4.cpp
midipipe4: midipipe4.o $(PMLIBS)

