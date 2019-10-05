# FIXME i think we need --soname somewhere in here
SHELL=bash

.DELETE_ON_ERROR:

.PHONY: all install uninstall deinstall default build test fulltest updateport clean mrproper

XMLBIN:=$(shell which xmlstarlet 2> /dev/null || which xml 2> /dev/null || echo xml)
TAGBIN:=$(shell which exctags 2> /dev/null || which ctags 2> /dev/null || echo ctags)

# If you don't have gcc 4.3 or greater, you'll need to define appropriate march
# and mtune values for your system (see gcc's "Submodel Options" info page). For
# a package intended to run on any x86 hardware, supply "generic" for MARCH
# and do not define MTUNE. You'll also likely want to define CC such cases,
# perhaps as "gcc".
MARCH?=native
ifneq ($(MARCH),generic)
MTUNE?=native
endif

ifeq ($(shell uname),Linux)
READLINK:=readlink -f
# --default-symver is unknown to gold
LFLAGS+=-Wl,--warn-shared-textrel,-O2
DFLAGS+=-D_FILE_OFFSET_BITS=64
DFLAGS+=-D_FORTIFY_SOURCE=2 -D_GNU_SOURCE
SHM_LFLAGS:=-lrt
DL_LFLAGS:=-ldl
MATH_LFLAGS:=-lm
PTHREAD_DFLAGS:=-pthread
PTHREAD_LFLAGS:=-lpthread
else
ifeq ($(shell uname),FreeBSD)
READLINK:=realpath
PTHREAD_DFLAGS:=-pthread -D_THREAD_SAFE -D_POSIX_PTHREAD_SEMANTICS
PTHREAD_LFLAGS:=-lpthread
PMC_LFLAGS:=-lpmc
endif
endif

CUNIT:=cunit
CROSIER:=crosier
LOGDEMO:=logdemo
DAEMONIZER:=daemonizer

# Internal deps for derived directories
BINDIR:=bin
LIBDIR:=libdank
SCRIPTDIR:=sbin
TOOLDIR:=tools
CROSIERCONFDIR:=conf/$(CROSIER)

# Output directories for build.
OUT:=.out
LIBOUT:=$(OUT)/$(LIBDIR)
BINOUT:=$(OUT)/$(BINDIR)
DEPOUT:=$(OUT)/dep
OBJOUT:=$(OUT)/obj
ERSATZ:=$(OUT)/ersatz

# Versioning code, which relies in part on git-rev-parse(1)'s output
SVNREVISION:=$(shell git rev-parse HEAD)
VERSIONSRC:=$(LIBDIR)/version.c
PROD_VER:=1.3.0-pre0

# Compatibility code, selected via `uname` and linked into $(OUT) from lib/
COMPATINC:=$(ERSATZ)/compat.h
COMPATSRC:=$(ERSATZ)/compat.c
COMPAT_TARGSPEC:=$(LIBDIR)/compat-$(shell uname)

# Output directory for install
PREFIX?=/usr/local

# Hierarchal filesystem backing store implies grouping by directory and, by
# idiom, file suffix. Alias directories as our root objects -- each binary
# gets its own source directory in APPSRCDIR.
BINARIES:=$(LOGDEMO) $(CROSIER) $(CUNIT) $(DAEMONIZER)
BIN:=$(addprefix $(BINOUT)/,$(BINARIES))
LIBRARIES:=$(addsuffix .so,libdank cunit-example)
LIB:=$(addprefix $(LIBOUT)/,$(LIBRARIES) $(addsuffix .0,$(LIBRARIES)))
APPSRCDIR:=src

# Logging demo
LOGDEMO_DIR:=$(APPSRCDIR)/$(LOGDEMO)
# Stdio <-> app control interface
CROSIER_DIR:=$(APPSRCDIR)/$(CROSIER)
# daemon(1)-like program for launching programs as system daemons
DAEMONIZER_DIR:=$(APPSRCDIR)/$(DAEMONIZER)
# Cross-app unit testing application
CUNIT_DIR:=$(APPSRCDIR)/$(CUNIT)
# Example extension module for cunit
CUNITEX_DIR:=$(APPSRCDIR)/example-$(CUNIT)-extension
# libdank
LIBDANK_DIRS:=$(addprefix $(LIBDIR)/,apps arch modules objects utils)

# Target-specific FOO_DIR + common LIBDANK_DIRS -> FOO_DIRS. Each set of paths
# must include all source files necessary to build its associated end target.
LOGDEMO_DIRS:=$(LOGDEMO_DIR)
CROSIER_DIRS:=$(CROSIER_DIR)
DAEMONIZER_DIRS:=$(DAEMONIZER_DIR)
CUNITEX_DIRS:=$(CUNITEX_DIR)

# Unit testing includes common and all cunit-specific code in that language.
CUNIT_DIRS:=$(CUNIT_DIR)

CSRCDIRS:=$(CUNIT_DIRS) $(CROSIER_DIRS) $(LIBDANK_DIRS) $(LOGDEMO_DIR) $(DAEMONIZER_DIR) $(CUNITEX_DIR)
SRC:=$(shell find $(CSRCDIRS) -name .svn -prune -o -type f -name \*.c -print) $(COMPATSRC)
INC:=$(shell find $(CSRCDIRS) -name .svn -prune -o -type f -name \*.h -print) $(COMPATINC) $(LIBDIR)/gcc.h $(LIBDIR)/version.h $(ERSATZ)/magictables.h

LIBDANKSRC:=$(foreach dir, $(LIBDANK_DIRS) $(CROSIER_DIR), $(filter $(dir)/%, $(SRC)))
LIBDANKSRC:=$(filter-out $(CROSIER_DIRS)/$(CROSIER).c,$(LIBDANKSRC))
LOGDEMOSRC:=$(foreach dir, $(LOGDEMO_DIRS), $(filter $(dir)/%, $(SRC)))
CROSIERSRC:=$(foreach dir, $(CROSIER_DIRS), $(filter $(dir)/%, $(SRC)))
DAEMONIZERSRC:=$(foreach dir, $(DAEMONIZER_DIRS), $(filter $(dir)/%, $(SRC)))
CUNITSRC:=$(foreach dir, $(CUNIT_DIRS), $(filter $(dir)/%, $(SRC)))
CUNITEXSRC:=$(foreach dir, $(CUNITEX_DIRS), $(filter $(dir)/%, $(SRC)))

LIBDANKOBJS:=$(addprefix $(OBJOUT)/,$(LIBDANKSRC:%.c=%.o) $(VERSIONSRC:%.c=%.o) $(COMPAT_TARGSPEC).o)
LOGDEMOOBJS:=$(addprefix $(OBJOUT)/,$(LOGDEMOSRC:%.c=%.o))
CROSIEROBJS:=$(addprefix $(OBJOUT)/,$(CROSIERSRC:%.c=%.o))
DAEMONIZEROBJS:=$(addprefix $(OBJOUT)/,$(DAEMONIZERSRC:%.c=%.o))
CUNITOBJS:=$(addprefix $(OBJOUT)/,$(CUNITSRC:%.c=%.o))
CUNITEXOBJS:=$(addprefix $(OBJOUT)/,$(CUNITEXSRC:%.c=%.o))

# -Wflags we can't use due to stupidity, not gcc version:
#  -Wconversion (BSD headers choke for mode_t auuuuugh)
#  -Wpadded (lib/utils/rfc2396.h)
# -Wflags we can't use due to bugs in our code FIXME:
#  -Wstrict-overflow=5 -Wunsafe-loop-optimizations
WFLAGS+=-Werror -Wall -W -Wextra -Wmissing-prototypes -Wundef -Wshadow \
	-Wstrict-prototypes -Wmissing-declarations -Wnested-externs \
	-Wsign-compare -Wpointer-arith -Wbad-function-cast -Wcast-qual \
	-Wdeclaration-after-statement -Wfloat-equal -Wpacked -Winvalid-pch \
	-Wdisabled-optimization -Wcast-align -Wformat -Wformat-security \
	-Wold-style-definition -Woverlength-strings -Wwrite-strings \
	-Wstrict-aliasing=3
LFLAGS+=-Wl,--warn-common -Wl,--as-needed
IFLAGS+=-I$(APPSRCDIR) -I.

# *** We get the following from -O (taken from gcc 4.3 docs) ***
# -fauto-inc-dec -fcprop-registers -fdce -fdefer-pop -fdelayed-branch -fdse \
# -fguess-branch-probability -fif-conversion2 -fif-conversion \
# -finline-small-functions -fipa-pure-const -fipa-reference -fmerge-constants \
# -fsplit-wide-types -ftree-ccp -ftree-ch -ftree-copyrename -ftree-dce \
# -ftree-dominator-opts -ftree-dse -ftree-fre -ftree-sra -ftree-ter \
# -funit-at-a-time, "and -fomit-frame-pointer on machines where it doesn't
# interfere with debugging."

# *** We get the following from -O2 (taken from gcc 4.3 docs) ***
# -fthread-jumps -falign-functions  -falign-jumps -falign-loops -falign-labels \
# -fcaller-saves -fcrossjumping -fcse-follow-jumps  -fcse-skip-blocks \
# -fdelete-null-pointer-checks -fexpensive-optimizations -fgcse -fgcse-lm \
# -foptimize-sibling-calls -fpeephole2 -fregmove -freorder-blocks \
# -freorder-functions -frerun-cse-after-loop -fsched-interblock -fsched-spec \
# -fschedule-insns -fschedule-insns2 -fstrict-aliasing -fstrict-overflow \
# -ftree-pre -ftree-vrp 

FFLAGS+=-O2 -fomit-frame-pointer -finline-functions -fdiagnostics-show-option \
	-pipe -rdynamic -fpic

# Debugging fflags
#FFLAGS+=-g -ggdb -fmudflapth

XML_IFLAGS:=$(shell xml2-config --cflags)
XML_LFLAGS:=$(shell xml2-config --libs)
TORQUE_IFLAGS:=$(shell pkg-config --cflags libtorque)
TORQUE_LFLAGS:=$(shell (pkg-config --libs libtorque || echo -ltorque))
CURSES_LFLAGS:=-lncurses
DANK_LFLAGS:=-Wl,-R$(LIBOUT) -L$(LIBOUT) -ldank

PTHREAD_DFLAGS+=-D_REENTRANT $(DFLAGS)
PTHREAD_IFLAGS+=-include pthread.h $(IFLAGS)

ifneq ($(MARCH),undef)
MFLAGS:=-march=$(MARCH)
ifdef MTUNE
MFLAGS+=-mtune=$(MTUNE)
endif
endif

LFLAGS+=-Wl,--enable-new-dtags
PTHREAD_CFLAGS:=-std=gnu99 $(PTHREAD_DFLAGS) $(PTHREAD_IFLAGS) $(XML_IFLAGS) \
	$(TORQUE_IFLAGS) $(FFLAGS) $(MFLAGS) $(WFLAGS)
CFLAGS:=-std=gnu99 $(PTHREAD_DFLAGS) $(XML_IFLAGS) $(TORQUE_IFLAGS) $(FFLAGS) \
	$(MFLAGS) $(WFLAGS)

all: default

default: test

INSTALL:=install -v
PKGCONFIG:=$(TOOLDIR)/libdank.pc
GCCINFO:=$(TOOLDIR)/gcc-info
MAN1:=$(addprefix doc/,$(addsuffix .1,$(CROSIER) $(CUNIT) $(DAEMONIZER)))
MAN3:=$(addprefix doc/,$(addsuffix .3dank,dank events))
install: build $(INC) $(PKGCONFIG) $(MAN1) $(MAN3) $(GCCINFO)
	@for i in $(INC) $(ERSATZ)/svnrev.h ; do mkdir -p -m 2775 $(PREFIX)/include/libdank/`dirname $$i | cut -s -d/ -f2-` && $(INSTALL) -m 0644 $$i $(PREFIX)/include/libdank/`echo $$i | cut -d/ -f2-` ; done
	@mkdir -p -m 2775 $(PREFIX)/bin
	@$(INSTALL) -m 0755 $(BINOUT)/$(CUNIT) $(GCCINFO) $(PREFIX)/bin
	@mkdir -p -m 2775 $(PREFIX)/lib
	@$(INSTALL) -m 0644 $(LIB) $(PREFIX)/lib
	@mkdir -p -m 2775 $(PREFIX)/lib/pkgconfig
	@$(INSTALL) -m 0644 $(PKGCONFIG) $(PREFIX)/lib/pkgconfig
	@mkdir -p -m 2775 $(PREFIX)/libexec
	@$(INSTALL) -m 0755 $(BINOUT)/$(CROSIER) $(PREFIX)/libexec
	@mkdir -p -m 2775 $(PREFIX)/man/man1
	@$(INSTALL) -m 0644 $(MAN1) $(PREFIX)/man/man1
	@mkdir -p -m 2775 $(PREFIX)/man/man3
	@$(INSTALL) -m 0644 $(MAN3) $(PREFIX)/man/man3
	@mkdir -p -m 2775 $(PREFIX)/sbin
	@$(INSTALL) -m 0755 $(BINOUT)/$(DAEMONIZER) $(PREFIX)/sbin
	@ldconfig

uninstall: deinstall

deinstall:
	@rm -rfv $(PREFIX)/include/libdank
	@rm -rfv $(PREFIX)/lib/pkgconfig/$(notdir $(PKGCONFIG))
	@rm -rfv $(addprefix $(PREFIX)/bin/,$(CUNIT) $(GCCINFO))
	@rm -rfv $(addprefix $(PREFIX)/lib/,$(notdir $(LIB)))
	@rm -rfv $(addprefix $(PREFIX)/libexec/,$(CROSIER))
	@rm -rfv $(addprefix $(PREFIX)/man/man1/,$(notdir $(MAN1)))
	@rm -rfv $(addprefix $(PREFIX)/man/man3/,$(notdir $(MAN3)))
	@rm -rfv $(addprefix $(PREFIX)/sbin/,$(DAEMONIZER))
	@ldconfig

CROSIERCONF:=$(CROSIERCONFDIR)/$(CROSIER).conf

TAGS:=.tags

build: $(TAGS) $(BIN)

LOGDEMO_CFLAGS:=$(CFLAGS)
LOGDEMO_LFLAGS:=$(LFLAGS) $(DANK_LFLAGS)
$(BINOUT)/$(LOGDEMO): $(LIB) $(LOGDEMOOBJS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(LOGDEMO_CFLAGS) -o $@ $(LOGDEMOOBJS) $(LOGDEMO_LFLAGS)

CROSIER_CFLAGS:=$(CFLAGS)
CROSIER_LFLAGS:=$(LFLAGS)
$(BINOUT)/$(CROSIER): $(CROSIEROBJS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(CROSIER_CFLAGS) -o $@ $(CROSIEROBJS) $(CROSIER_LFLAGS)

DAEMONIZER_CFLAGS:=$(CFLAGS)
DAEMONIZER_LFLAGS:=$(LFLAGS) $(DANK_LFLAGS) $(PTHREAD_LFLAGS)
$(BINOUT)/$(DAEMONIZER): $(LIB) $(DAEMONIZEROBJS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(DAEMONIZER_CFLAGS) -o $@ $(DAEMONIZEROBJS) $(DAEMONIZER_LFLAGS)

CUNIT_CFLAGS:=$(CFLAGS)
CUNIT_LFLAGS:=$(LFLAGS) $(MATH_LFLAGS) $(DL_LFLAGS) $(XML_LFLAGS) $(DANK_LFLAGS) $(PTHREAD_LFLAGS)
$(BINOUT)/$(CUNIT): $(CUNITOBJS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(CUNIT_CFLAGS) -o $@ $(CUNITOBJS) $(CUNIT_LFLAGS)

$(LIBOUT)/libdank.so: $(LIBOUT)/libdank.so.0
	@[ -d $(@D) ] || mkdir -p $(@D)
	@ln -fsn $(shell $(READLINK) $<) $@

$(LIBOUT)/cunit-example.so: $(LIBOUT)/cunit-example.so.0
	@[ -d $(@D) ] || mkdir -p $(@D)
	@ln -fsn $(shell $(READLINK) $<) $@

CUNITEX_CFLAGS:=$(CFLAGS) -shared
CUNITEX_LFLAGS:=$(LFLAGS) $(PTHREAD_LFLAGS)
$(LIBOUT)/cunit-example.so.0: $(CUNITEXOBJS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(CUNITEX_CFLAGS) -o $@ $(CUNITEXOBJS) $(CUNITEX_LFLAGS)

LIBDANK_CFLAGS:=$(PTHREAD_CFLAGS) -shared
LIBDANK_LFLAGS:=$(LFLAGS) $(CURSES_LFLAGS) $(SHM_LFLAGS) $(XML_LFLAGS) $(TORQUE_LFLAGS) $(PMC_LFLAGS) $(PTHREAD_LFLAGS)
$(LIBOUT)/libdank.so.0: $(LIBDANKOBJS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(LIBDANK_CFLAGS) -o $@ $(LIBDANKOBJS) $(LIBDANK_LFLAGS)

$(OBJOUT)/$(APPSRCDIR)/$(LOGDEMO)/$(LOGDEMO).o: $(APPSRCDIR)/$(LOGDEMO)/$(LOGDEMO).c $(INC) $(ERSATZ)/svnrev.h
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(PTHREAD_CFLAGS) -o $@ -c $<

$(OBJOUT)/$(APPSRCDIR)/$(DAEMONIZER)/$(DAEMONIZER).o: $(APPSRCDIR)/$(DAEMONIZER)/$(DAEMONIZER).c $(INC) $(ERSATZ)/svnrev.h
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(PTHREAD_CFLAGS) -o $@ -c $<

$(OBJOUT)/$(APPSRCDIR)/$(CUNIT)/$(CUNIT).o: $(APPSRCDIR)/$(CUNIT)/$(CUNIT).c $(INC) $(ERSATZ)/svnrev.h
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(PTHREAD_CFLAGS) -o $@ -c $<

$(OBJOUT)/$(LIBDIR)/version.o: $(VERSIONSRC) $(INC) $(ERSATZ)/svnrev.h
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) -DVERSION=\"$(PROD_VER)\" $(PTHREAD_CFLAGS) -c -o $@ $<

$(OBJOUT)/%.o: %.c $(INC)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(PTHREAD_CFLAGS) -c $< -o $@

$(OBJOUT)/%.s: %.c $(INC)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(PTHREAD_CFLAGS) -S $< -o $@

# Don't use gcc's default .i output extension; it conflicts with SWIG interface
# files, and thus this default target can screw things up.
$(OBJOUT)/%.E: %.c $(INC)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(CC) $(PTHREAD_CFLAGS) -E $< -o $@

$(COMPATINC): $(COMPAT_TARGSPEC).h
	@[ -d $(@D) ] || mkdir -p $(@D)
	@ln -fsn $(shell $(READLINK) $<) $@

$(COMPATSRC): $(COMPAT_TARGSPEC).c
	@[ -d $(@D) ] || mkdir -p $(@D)
	@ln -fsn $(shell $(READLINK) $<) $@

$(ERSATZ)/svnrev.h: $(LIBDIR)/ersatz
	@[ -d $(@D) ] || mkdir -p $(@D)
	echo "\"$(SVNREVISION)\"" > $@

$(ERSATZ)/magictables.h: $(TOOLDIR)/magictables $(LIBDIR)/ersatz
	@[ -d $(@D) ] || mkdir -p $(@D)
	$< > $@

$(LIBDIR)/ersatz: $(ERSATZ)
	ln -sf ../../$< $@

TEST_DATA:=testing
# MALLOC_CHECK_ is a glibc memory checking technique; see 3.2.2.9 "Heap
# Consistency Checking" in the glibc documentation.
test: build
	$(BINOUT)/$(DAEMONIZER) -u $(USER) -p $(TEST_DATA)/daemonizerlock -- /bin/echo erp
	$(BINOUT)/$(DAEMONIZER) -u $(USER) -r 2 -L 1 -- /bin/echo erp -n
	# turned off until libtorque stops locking up :/
	#$(BINOUT)/$(LOGDEMO)
	#$(BINOUT)/$(LOGDEMO) -v
	#env MALLOC_CHECK_=2 $(BINOUT)/$(CUNIT) -o $(LIBOUT)/$(CUNIT)-example.so -c $(TEST_DATA) -a

fulltest: test
	$(GCCINFO)
	env MALLOC_CHECK_=2 $(BINOUT)/$(CUNIT) -c $(TEST_DATA) -b100 cpucount
	env MALLOC_CHECK_=2 $(BINOUT)/$(CUNIT) -o $(LIBOUT)/$(CUNIT)-example.so -c $(TEST_DATA) -f -a

$(TAGS): $(MAKEFILE_LIST) $(SRC) $(INC)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(TAGBIN) -f $@ $^

clean:
	@rm -vfr $(OUT)

mrproper: clean
	@rm -vf $(TAGS)

# Export NEWPORTSVN and build the updateport target to release a new port
updateport: $(APPSRCDIR)/packaging/release-port
	@[ -d "$(NEWPORTSVN)" ] || { echo "Export the path to a newports checkout as NEWPORTSVN" >&2 ; false ; }
	@$< $(PROD_VER) $(NEWPORTSVN)
