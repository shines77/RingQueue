
# Clear the default suffixes, so that built-in rules are not used.
.SUFFIXES :

SHELL := /bin/sh

CC := g++
CXX := g++

# Configuration parameters.
DESTDIR =
BINDIR := $(DESTDIR)/usr/local/bin
INCLUDEDIR := $(DESTDIR)/usr/local/include
LIBDIR := $(DESTDIR)/usr/local/lib
DATADIR := $(DESTDIR)/usr/local/share
MANDIR := $(DESTDIR)/usr/local/share/man
srcroot := 
objroot := 

# Build parameters.
CPPFLAGS := -D_REENTRANT -I$(srcroot)include -I$(objroot)include -I$(srcroot)include/RingQueue -I$(objroot)include/RingQueue
CFLAGS := -std=c++0x -Wall -w -pipe -g3 -fpermissive -fvisibility=hidden -O3 -funroll-loops -msse -msse2 -D_GNU_SOURC -D__MMX__ -D__SSE__ -D__SSE2__ -D__SSE3__
LDFLAGS := 
EXTRA_LDFLAGS := 
LIBS :=  -lpthread
RPATH_EXTRA := 
SO := so
IMPORTLIB := so
O := o
A := a
EXE := .exe
LIBPREFIX := lib
REV := 1
install_suffix := 
ABI := elf
XSLTPROC := /usr/bin/xsltproc
AUTOCONF := false
_RPATH = -Wl,-rpath,$(1)
RPATH = $(if $(1),$(call _RPATH,$(1)))

header_files := include/RingQueue/console.h include/RingQueue/dump_mem.h include/RingQueue/get_char.h \
    include/RingQueue/mq.h include/RingQueue/port.h include/RingQueue/q.h include/RingQueue/q3.h \
    include/RingQueue/RingQueue.h include/RingQueue/sleep.h include/RingQueue/sys_timer.h \
    include/RingQueue/test.h include/RingQueue/vs_inttypes.h include/RingQueue/vs_stdbool.h \
    include/RingQueue/vs_stdint.h include/RingQueue/msvc/inttypes.h include/RingQueue/msvc/stdbool.h \
    include/RingQueue/msvc/stdint.h include/RingQueue/msvc/pthread.h include/RingQueue/msvc/sched.h

enable_autogen := 0
enable_code_coverage := 0
enable_experimental := 1
enable_zone_allocator := 
DSO_LDFLAGS = -shared -Wl,-soname,$(@F)
SOREV = so.1
PIC_CFLAGS = -fPIC -DPIC
CTARGET = -o $@
LDTARGET = -o $@
MKLIB = 
AR = ar
ARFLAGS = crus
CC_MM = 1

RINGQUEUE := RingQueue
LIBRINGQUEUE := $(LIBPREFIX)RingQueue$(install_suffix)

# Lists of files.
BINS := $(srcroot)bin/pprof $(objroot)bin/RingQueue.sh

C_HDRS := $(objroot)include/RingQueue/console.h $(objroot)include/RingQueue/dump_mem.h \
	$(objroot)include/RingQueue/get_char.h $(objroot)include/RingQueue/mq.h \
	$(objroot)include/RingQueue/port.h $(objroot)include/RingQueue/q.h \
	$(objroot)include/RingQueue/q3.h $(objroot)include/RingQueue/RingQueue.h \
	$(objroot)include/RingQueue/sleep.h $(objroot)include/RingQueue/sys_timer.h \
	$(objroot)include/RingQueue/test.h $(objroot)include/RingQueue/vs_inttypes.h \
	$(objroot)include/RingQueue/vs_stdbool.h $(objroot)include/RingQueue/vs_stdint.h \
	$(objroot)include/RingQueue/msvc/inttypes.h $(objroot)include/RingQueue/msvc/stdbool.h \
	$(objroot)include/RingQueue/msvc/stdint.h $(objroot)include/RingQueue/msvc/pthread.h \
    $(objroot)include/RingQueue/msvc/sched.h

C_SRCS := $(srcroot)src/RingQueue/console.c \
	$(srcroot)src/RingQueue/dump_mem.c $(srcroot)src/RingQueue/get_char.c $(srcroot)src/RingQueue/mq.c \
	$(srcroot)src/RingQueue/sleep.c $(srcroot)src/RingQueue/sys_timer.c \
    $(srcroot)src/RingQueue/msvc/pthread.c $(srcroot)src/RingQueue/msvc/sched.c
    # $(srcroot)src/RingQueue/main.c

CPP_SRCS := $(srcroot)src/RingQueue/main.cpp

ifeq ($(IMPORTLIB),$(SO))
STATIC_LIBS := $(objroot)lib/$(LIBRINGQUEUE).$(A)
endif
ifdef PIC_CFLAGS
STATIC_LIBS += $(objroot)lib/$(LIBRINGQUEUE)_pic.$(A)
else
STATIC_LIBS += $(objroot)lib/$(LIBRINGQUEUE)_s.$(A)
endif
DSOS := $(objroot)lib/$(LIBRINGQUEUE).$(SOREV)
ifneq ($(SOREV),$(SO))
DSOS += $(objroot)lib/$(LIBRINGQUEUE).$(SO)
endif

C_OBJS := $(C_SRCS:$(srcroot)%.c=$(objroot)%.$(O))
C_PIC_OBJS := $(C_SRCS:$(srcroot)%.c=$(objroot)%.pic.$(O))
C_JET_OBJS := $(C_SRCS:$(srcroot)%.c=$(objroot)%.jet.$(O))

CPP_OBJS := $(CPP_SRCS:$(srcroot)%.cpp=$(objroot)%.$(O))
CPP_PIC_OBJS := $(CPP_SRCS:$(srcroot)%.cpp=$(objroot)%.pic.$(O))
CPP_JET_OBJS := $(CPP_SRCS:$(srcroot)%.cpp=$(objroot)%.jet.$(O))

.PHONY: all
.PHONY: clean
.PHONY: help

# Default target.
all: build_exe

#
# Include generated dependency files.
#
ifdef CC_MM
-include $(C_OBJS:%.$(O)=%.d)
-include $(C_PIC_OBJS:%.$(O)=%.d)
-include $(C_JET_OBJS:%.$(O)=%.d)
-include $(CPP_OBJS:%.$(O)=%.d)
-include $(CPP_PIC_OBJS:%.$(O)=%.d)
-include $(CPP_JET_OBJS:%.$(O)=%.d)
endif

$(C_OBJS): $(objroot)src/RingQueue/%.$(O): $(srcroot)src/RingQueue/%.c
$(C_OBJS): CPPFLAGS += -I$(srcroot)include -I$(objroot)include/RingQueue
$(C_PIC_OBJS): $(objroot)src/RingQueue/%.pic.$(O): $(srcroot)src/RingQueue/%.c
$(C_PIC_OBJS): CFLAGS += $(PIC_CFLAGS)
$(C_JET_OBJS): $(objroot)src/RingQueue/%.jet.$(O): $(srcroot)src/RingQueue/%.c
$(C_JET_OBJS): CFLAGS += -DRINGQUEUE_JET

$(CPP_OBJS): $(objroot)src/RingQueue/%.$(O): $(srcroot)src/RingQueue/%.cpp
$(CPP_OBJS): CPPFLAGS += -I$(srcroot)include -I$(objroot)include/RingQueue
$(CPP_PIC_OBJS): $(objroot)src/RingQueue/%.pic.$(O): $(srcroot)src/RingQueue/%.cpp
$(CPP_PIC_OBJS): CFLAGS += $(PIC_CFLAGS)
$(CPP_JET_OBJS): $(objroot)src/RingQueue/%.jet.$(O): $(srcroot)src/RingQueue/%.cpp
$(CPP_JET_OBJS): CFLAGS += -DRINGQUEUE_JET

ifneq ($(IMPORTLIB),$(SO))
$(C_OBJS): CPPFLAGS += -DDLLEXPORT
$(CPP_OBJS): CPPFLAGS += -DDLLEXPORT
endif

ifndef CC_MM
# Dependencies.
HEADER_DIRS = $(srcroot)src/RingQueue \
	$(objroot)include/RingQueue $(objroot)include/RingQueue/msvc
HEADERS = $(wildcard $(foreach dir,$(HEADER_DIRS),$(dir)/*.h))
$(C_OBJS) $(C_PIC_OBJS) $(C_JET_OBJS) $(CPP_OBJS) $(CPP_PIC_OBJS) $(CPP_JET_OBJS) : $(HEADERS)
endif

$(C_OBJS) $(C_PIC_OBJS) $(C_JET_OBJS) : %.$(O):
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $(CPPFLAGS) $(CTARGET) $<

$(CPP_OBJS) $(CPP_PIC_OBJS) $(CPP_JET_OBJS) : %.$(O):
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $(CPPFLAGS) $(CTARGET) $<

ifdef CC_MM
	@$(CC) -MM $(CPPFLAGS) -MT $@ -o $(@:%.$(O)=%.d) $<
endif

ifneq ($(SOREV),$(SO))
%.$(SO) : %.$(SOREV)
	@mkdir -p $(@D)
	ln -sf $(<F) $@
endif

$(objroot)bin/gcc/$(RINGQUEUE)$(EXE) : $(if $(PIC_CFLAGS),$(CPP_PIC_OBJS),$(CPP_OBJS)) $(if $(PIC_CFLAGS),$(C_PIC_OBJS),$(C_OBJS)) 
	@mkdir -p $(@D)
	$(CC) $(LDTARGET) $(filter %.$(O),$^) $(call RPATH,$(objroot)lib) $(LDFLAGS) $(filter-out -lm,$(LIBS)) -lm $(EXTRA_LDFLAGS)

build_exe: $(objroot)bin/gcc/$(RINGQUEUE)$(EXE)

#=============================================================================
# Target rules for targets named RingQueue

# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
    # @echo "... help"
.PHONY : help

# The main clean target
clean:
	rm -f $(C_OBJS)
	rm -f $(C_PIC_OBJS)
	rm -f $(C_JET_OBJS)
	rm -f $(C_OBJS:%.$(O)=%.d)
	rm -f $(C_OBJS:%.$(O)=%.gcda)
	rm -f $(C_OBJS:%.$(O)=%.gcno)
	rm -f $(C_PIC_OBJS:%.$(O)=%.d)
	rm -f $(C_PIC_OBJS:%.$(O)=%.gcda)
	rm -f $(C_PIC_OBJS:%.$(O)=%.gcno)
	rm -f $(C_JET_OBJS:%.$(O)=%.d)
	rm -f $(C_JET_OBJS:%.$(O)=%.gcda)
	rm -f $(C_JET_OBJS:%.$(O)=%.gcno)

	rm -f $(CPP_OBJS)
	rm -f $(CPP_PIC_OBJS)
	rm -f $(CPP_JET_OBJS)
	rm -f $(CPP_OBJS:%.$(O)=%.d)
	rm -f $(CPP_OBJS:%.$(O)=%.gcda)
	rm -f $(CPP_OBJS:%.$(O)=%.gcno)
	rm -f $(CPP_PIC_OBJS:%.$(O)=%.d)
	rm -f $(CPP_PIC_OBJS:%.$(O)=%.gcda)
	rm -f $(CPP_PIC_OBJS:%.$(O)=%.gcno)
	rm -f $(CPP_JET_OBJS:%.$(O)=%.d)
	rm -f $(CPP_JET_OBJS:%.$(O)=%.gcda)
	rm -f $(CPP_JET_OBJS:%.$(O)=%.gcno)

	rm -f $(DSOS) $(STATIC_LIBS)
	rm -f $(objroot)*.gcov.*

.PHONY : clean
#=============================================================================
