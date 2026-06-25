CC = gcc
CFLAGS = -O3 `pkg-config --cflags gtk+-2.0` -fcommon
LDFLAGS = `pkg-config --libs gtk+-2.0` -lm -lrt
FC = gfortran
FFLAGS = -ffree-form -O3

# ---------------------------------------------------------------------------
# Phase 4: EPICS bridge
# Set EPICS_BASE to your EPICS base install, e.g.:
#   make EPICS_BASE=/usr/local/epics/base
# If EPICS_BASE is not set, the bridge is built in stub mode (no CA).
# ---------------------------------------------------------------------------
EPICS_BASE ?=
EPICS_ARCH ?= linux-x86_64

ifneq ($(EPICS_BASE),)
  EPICS_CFLAGS  = -DHAVE_EPICS \
                  -I$(EPICS_BASE)/include \
                  -I$(EPICS_BASE)/include/os/Linux \
                  -I$(EPICS_BASE)/include/compiler/gcc
  EPICS_LDFLAGS = -L$(EPICS_BASE)/lib/$(EPICS_ARCH) -lca -lCom
else
  EPICS_CFLAGS  =
  EPICS_LDFLAGS =
endif
# Note use -g in CFLAGS and FFLAGS instead of O3 to enable the debugger ($gdb lamps or $gdbtui lamps)
SETS = calib.o check_setup.o control.o create_dir.o exogam.o feedback.o fit.o \
        freedom.o generator.o help.o init.o iospec.o macro.o main.o main_menu.o offline.o open.o \
        parse.o plot1.o plot2.o postscript.o prefs.o radware_io.o readfile.o rewrite.o rw_setup.o \
        safety.o scalers.o screen.o setup.o themes.o user.o useful.o util.o viewlog.o zlsgls.o \
        libcamac.o daq_engine.o lamps_shm.o
OBJS = ascii2d.o cmc100test.o crash.o ldcmc100.o mkword.o zmass.o $(SETS)
EXEDIR = ../
EXECS = $(EXEDIR)ascii2d $(EXEDIR)cmc100test $(EXEDIR)crash $(EXEDIR)lamps \
        $(EXEDIR)ldcmc100 $(EXEDIR)mkword $(EXEDIR)zmass $(EXEDIR)camacctl \
        $(EXEDIR)lamps_epics_bridge
LIBS = libcamac.a libcamac.so
all:    $(EXECS) $(LIBS)

# PIC object for shared library
libcamac_pic.o: libcamac.c libcamac.h
	$(CC) -O3 -fPIC -c -o libcamac_pic.o libcamac.c

# Static library
libcamac.a: libcamac.o
	ar rcs libcamac.a libcamac.o

# Shared library
libcamac.so: libcamac_pic.o
	$(CC) -shared -o libcamac.so libcamac_pic.o

$(EXEDIR)ascii2d: ascii2d.o useful.o open.o
	$(CC) -o $(EXEDIR)ascii2d ascii2d.o useful.o open.o $(CFLAGS) $(LDFLAGS)
$(EXEDIR)cmc100test: cmc100test.o useful.o open.o libcamac.o
	$(CC) -o $(EXEDIR)cmc100test cmc100test.o useful.o open.o libcamac.o $(CFLAGS) $(LDFLAGS)
$(EXEDIR)crash: crash.o useful.o
	$(CC) -o $(EXEDIR)crash crash.o useful.o $(CFLAGS) $(LDFLAGS)
$(EXEDIR)ldcmc100: ldcmc100.o
	$(CC) -o $(EXEDIR)ldcmc100 ldcmc100.o
$(EXEDIR)mkword: mkword.o useful.o
	$(CC) -o $(EXEDIR)mkword mkword.o useful.o $(CFLAGS) $(LDFLAGS)
$(EXEDIR)zmass: zmass.o useful.o open.o
	$(CC) -o $(EXEDIR)zmass zmass.o useful.o open.o $(CFLAGS) $(LDFLAGS)
$(EXEDIR)lamps: $(SETS)
	$(FC) -o $(EXEDIR)lamps $(SETS) `pkg-config --libs gtk+-2.0 gthread-2.0` -lm
	@echo "********************************************************"
	@echo "* Make complete. If you dont have errors...            *"
	@echo "* go to the upper level directory and type ./lamps     *"
	@echo "********************************************************"

# camacctl CLI - links only against libcamac (no GTK needed)
$(EXEDIR)camacctl: camacctl.o libcamac.o
	$(CC) -O3 -o $(EXEDIR)camacctl camacctl.o libcamac.o

$(OBJS): lamps.h
daq_engine.o control.o: daq_engine.h
daq_engine.o: lamps_shm.h
user.o radware_io.o: constants.h
main.o control.o: common.h
freedom.o rewrite.o: common.h

# lamps_shm.o: compiled into lamps with writer-side globals.
# -DLAMPS_SHM_WRITER_SIDE activates the extern declarations.
lamps_shm.o: lamps_shm.c lamps_shm.h
	$(CC) $(CFLAGS) -DLAMPS_SHM_WRITER_SIDE -c -o lamps_shm.o lamps_shm.c

# lamps_epics_bridge: standalone process, no GTK.
# Builds in stub mode when EPICS_BASE is not set.
$(EXEDIR)lamps_epics_bridge: lamps_epics_bridge.o lamps_shm_reader.o
	$(CC) -O3 -o $(EXEDIR)lamps_epics_bridge \
	    lamps_epics_bridge.o lamps_shm_reader.o \
	    $(EPICS_LDFLAGS) -lrt -lpthread

# Bridge-side shm object: NO writer-side externs, NO GTK.
lamps_shm_reader.o: lamps_shm.c lamps_shm.h
	$(CC) -O3 -c -o lamps_shm_reader.o lamps_shm.c

lamps_epics_bridge.o: lamps_epics_bridge.c lamps_shm.h
	$(CC) -O3 $(EPICS_CFLAGS) -c -o lamps_epics_bridge.o lamps_epics_bridge.c

clean:
	rm -f $(OBJS) $(EXECS) $(LIBS) libcamac_pic.o camacctl.o \
	    lamps_shm_reader.o lamps_epics_bridge.o
	@echo "------------Cleanup done------------"
