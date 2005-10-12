.SUFFIXES: .c .o .elf .bin

.c.o:
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
CALOBJS  = atwd_cal.o amp_cal.o

EXOBJS = calUtils.o
KOBJS = ../lib/crt0.o ../lib/libkernel.a

#CFILES   = domcal.c calUtils.c $(CALOBJS:.o=.c)
ALLOBJS  = domcal.o calUtils.o $(CALOBJS)

all: domcal.elf 

clean:
	rm -f *.o *.i *.bin *.elf

domcal.elf: domcal.o $(EXOBJS) $(CALOBJS) $(LIBHAL)
	$(LD) --script=$(KERNELX) -o domcal.elf \
	$(KOBJS) domcal.o $(CALOBJS) $(EXOBJS) $(LIBHAL) $(SYSLIBS)

../bin/%.bin.gz: %.bin; gzip -c $< > $@

$(ALLOBJS): %.o : %.h

../bin/domcal: domcal.o $(CALOBJS) $(EXOBJS)
	gcc -o domcal domcal.o $(CALOBJS) $(EXOBJS) $(LIBHAL) -lm
