ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

ELF := ps5-elfs-host.elf
CFLAGS := -Wall -Werror -O2
MINIZ_CFLAGS := -O2 -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_WRITING_APIS -Wno-unused-function
LDADD := -lSceSystemService -lSceUserService -lSceNet -lSceSsl -lSceHttp -lpthread

all: $(ELF)

miniz.o: miniz.c miniz.h
	$(CC) $(MINIZ_CFLAGS) -c -o $@ miniz.c

$(ELF): main.c index_html.c miniz.o
	$(CC) $(CFLAGS) -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_WRITING_APIS -o $@ main.c index_html.c miniz.o $(LDADD)

clean:
	rm -f $(ELF) miniz.o
