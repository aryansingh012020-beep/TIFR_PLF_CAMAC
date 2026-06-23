CC = gcc
CFLAGS = -O3 `pkg-config --cflags gtk+-2.0` -fcommon
LDFLAGS = `pkg-config --libs gtk+-2.0` -lm
FC = gfortran
FFLAGS = -ffree-form -O3
# Note use -g in CFLAGS and FFLAGS instead of O3 to enable the debugger ($gdb lamps or $gdbtui lamps)
SETS = calib.o check_setup.o control.o create_dir.o exogam.o feedback.o fit.o \
       freedom.o generator.o help.o init.o iospec.o macro.o main.o main_menu.o offline.o open.o \
       parse.o plot1.o plot2.o postscript.o prefs.o radware_io.o readfile.o rewrite.o rw_setup.o \
       safety.o scalers.o screen.o setup.o themes.o user.o useful.o util.o viewlog.o zlsgls.o libcamac.o
OBJS = ascii2d.o cmc100test.o crash.o ldcmc100.o mkword.o zmass.o $(SETS)
EXEDIR = ../
EXECS = $(EXEDIR)ascii2d $(EXEDIR)cmc100test $(EXEDIR)crash $(EXEDIR)lamps \
        $(EXEDIR)ldcmc100 $(EXEDIR)mkword $(EXEDIR)zmass
all:    $(EXECS)
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
$(OBJS): lamps.h
user.o radware_io.o: constants.h
main.o control.o: common.h
freedom.o rewrite.o: common.h
clean:
	rm -f $(OBJS) $(EXECS)
	@echo "------------Cleanup done------------"
