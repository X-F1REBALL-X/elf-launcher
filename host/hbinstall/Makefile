ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

ELF := elf-launcher-install.elf
CFLAGS := -Wall -Werror -O2 -I/tmp/elfldr-src
ifdef EL_HTTP_PORT
CFLAGS += -DEL_HTTP_PORT=$(EL_HTTP_PORT)
endif
LDADD := -lSceIpmi -lkernel_web -lSceLibcInternal -lSceNet -lSceSsl -lSceHttp -lSceSystemService -lSceUserService -lpthread

all: $(ELF)

$(ELF): main.c elfldr_boot.c notify.c update_impl.inc webapp/index.html icons/wk-store.png
	$(CC) $(CFLAGS) -o $@ main.c elfldr_boot.c notify.c /tmp/elfldr-src/elfldr.c /tmp/elfldr-src/pt.c $(LDADD)

clean:
	rm -f $(ELF)
