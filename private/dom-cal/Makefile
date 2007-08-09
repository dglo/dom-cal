.SUFFIXES: .c .o .elf .bin

%.o : %.c *.h
	$(CC) -c $(CFLAGS) $<

.elf.bin:
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(CPP) $(CPPFLAGS) -DBINFILE=\"$*.bin\" -o $*-raw.i $(RAWS)
	$(AS) $(AFLAGS) -o $*-raw.o $*-raw.i
	$(LD) --script=$(RAWX) -o $*-raw.elf $*-raw.o
	$(OBJCOPY) -O binary $*-raw.elf $*.bin

#
# Add new calibration routines here
#
CALOBJS  = atwd_cal.o amp_cal.o atwd_freq_cal.o pulser_cal.o hv_gain_cal.o baseline_cal.o hv_amp_cal.o transit_cal.o fast_acq.o fadc_cal.o discriminator_cal.o daq_baseline_cal.o

EXOBJS = calUtils.o spefit.o lmfit.o icebootUtils.o write_xml.o
KOBJS = ../lib/crt0.o ../lib/libkernel.a
ICEOBJS = ../iceboot/fis.o ../iceboot/flashdrv.o

ALLOBJS  = domcal.o calUtils.o $(CALOBJS)

all: domcal.elf 

clean:
	rm -f *.o *.i *.bin *.elf

domcal.elf: domcal.o $(EXOBJS) $(CALOBJS) $(LIBHAL) $(SYSLIBS) $(KERNELX)
	$(LD) --script=$(KERNELX) -o domcal.elf $(KOBJS) $(ICEOBJS) domcal.o $(CALOBJS) $(EXOBJS) $(LIBHAL) $(SYSLIBS) -lc

../bin/%.bin.gz: %.bin; gzip -c $< > $@

$(ALLOBJS): %.o : %.h

../bin/domcal: domcal.o $(CALOBJS) $(EXOBJS)
	gcc -o domcal domcal.o $(CALOBJS) $(EXOBJS) $(LIBHAL) $(SYSLIBS) -lm
