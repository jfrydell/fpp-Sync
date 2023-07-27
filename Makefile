SRCDIR ?= /opt/fpp/src
include $(SRCDIR)/makefiles/common/setup.mk
include $(SRCDIR)/makefiles/platform/*.mk

all: libfpp-Sync.$(SHLIB_EXT)

OBJECTS_fpp_Sync_so += src/FPPSync.o
LIBS_fpp_Sync_so += -L$(SRCDIR) -lfpp -ljsoncpp -lhttpserver
CXXFLAGS_src/FPPSync.o += -I$(SRCDIR)

%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-Sync.$(SHLIB_EXT): $(OBJECTS_fpp_Sync_so) $(SRCDIR)/libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_Sync_so) $(LIBS_fpp_Sync_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-Sync.so $(OBJECTS_fpp_Sync_so)
