PLATFORM ?= mac
PLATFORM ?= linux

PM_DIR = portmidi-82-20080614/portmidi

CFLAGS += -I$(PM_DIR)/porttime
CFLAGS += -I$(PM_DIR)/pm_common
CFLAGS += -O2
CFLAGS += $(REALTIME)
CXXFLAGS = $(CFLAGS)

ifeq ($(PLATFORM),linux)
  LDFLAGS += -lasound
  REALTIME = -DUSE_LINUX_SCHED_FIFO
endif
ifeq ($(PLATFORM),mac)
  LDFLAGS += -framework CoreMIDI
  LDFLAGS += -framework CoreFoundation
  LDFLAGS += -framework CoreAudio
  LDFLAGS += -framework CoreServices
  REALTIME = -DUSE_OSX_REALTIME
endif

CC = g++

all: midipipe4

clean:
	-rm *.o





ifeq ($(PLATFORM),linux)
  PM_PLATFORM_MAKEFILE = pm_linux/Makefile
  PMLIBS = 
  PMLIBS += $(PM_DIR)/pm_linux/libportmidi.a
  PMLIBS += $(PM_DIR)/porttime/libporttime.a
endif

ifeq ($(PLATFORM),mac)
  PM_PLATFORM_MAKEFILE = pm_mac/Makefile.osx
  PMLIBS = 
  PMLIBS += $(PM_DIR)/pm_mac/libportmidi.a
  PMLIBS += $(PM_DIR)/porttime/libporttime.a
endif


.PHONY: FORCE

$(PMLIBS): FORCE
	cd $(PM_DIR); make -f $(PM_PLATFORM_MAKEFILE)
	@# Mac:   make -f pm_mac/Makefile.osx
	@# Linux: make -f pm_linux/Makefile




midipipe4.o: midipipe4.cpp
midipipe4: midipipe4.o $(PMLIBS)
