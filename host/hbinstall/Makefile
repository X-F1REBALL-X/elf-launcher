ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

ELF := elf-launcher-install.elf
CFLAGS := -Wall -Werror -O2 -I/tmp/elfldr-src
LDADD := -lkernel_web -lSceLibcInternal -lSceNet

all: $(ELF)

$(ELF): main.c elfldr_boot.c
	$(CC) $(CFLAGS) -o $@ main.c elfldr_boot.c /tmp/elfldr-src/elfldr.c /tmp/elfldr-src/pt.c /tmp/elfldr-src/notify.c $(LDADD)

clean:
	rm -f $(ELF)
