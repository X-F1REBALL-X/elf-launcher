ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

ELF := ps5-elfs-host-installer.elf
CFLAGS := -Wall -Werror -O2
# NO -lSceAppInstUtil / UserService / SystemService — elfldr cannot load those NEEDED.
# Resolve AppInstUtil at runtime via dlopen + kernel_dynlib NID.
LDADD :=

all: $(ELF)

$(ELF): main.c assets/param.json assets/icon0.png
	$(CC) $(CFLAGS) -o $@ main.c $(LDADD)

clean:
	rm -f $(ELF)
