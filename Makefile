dirs = crc-gen atmega32 atmega328 bootloader sdrive-ctrl

all: bootloader.m328 debugloader
	for dir in $(dirs); \
		do $(MAKE) -C $$dir || exit 1; \
	done

clean:
	for dir in $(dirs); \
		do $(MAKE) -C $$dir clean; \
	done
	$(MAKE) -C bootloader -f Makefile.m328 clean
	rm debugloader

bootloader.m328:
	$(MAKE) -C bootloader -f Makefile.m328

debugloader: debugloader.xa
	xa -o $@ $@.xa
	xxd -i $@ $@.h
	sed -i 's/unsigned char/const u08 PROGMEM/' $@.h
	perl -i -npe 's,(.*len.*),//$$1,' $@.h
	
