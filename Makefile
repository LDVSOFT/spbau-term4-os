CC ?= gcc
LD ?= gcc
QEMU = qemu-system-x86_64

RUNFLAGS := -no-reboot -no-shutdown -serial stdio -enable-kvm -initrd initramfs.cpio
# -pedantic is off because I want some GCC extensions
CFLAGS := -g -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -ffreestanding -fno-pie -fno-omit-frame-pointer \
	-mcmodel=kernel -Wall -Wextra -Werror -std=gnu11 -Og \
	-Wframe-larger-than=4096 -Wstack-usage=4096 -Wno-unknown-warning-option -Wno-unused-parameter -Wno-unused-function
LFLAGS := -nostdlib -z max-page-size=0x1000

ASM := bootstrap.S videomem.S interrupt-wrappers.S threads-wrappers.S
AOBJ:= $(ASM:.S=.o)
ADEP:= $(ASM:.S=.d)

SRC := main.c pic.c interrupt.c serial.c pit.c print.c memory.c buddy.c \
	bootstrap-alloc.c paging.c log.c slab-allocator.c threads.c string.c cmdline.c \
	test.c list.c fs.c initramfs.c
OBJ := $(AOBJ) $(SRC:.c=.o)
DEP := $(ADEP) $(SRC:.c=.d)

all: kernel

kernel: $(OBJ) kernel.ld Makefile
	$(LD) $(LFLAGS) $(LINK_FLAGS) -T kernel.ld -o $@ $(OBJ)

%.o: %.S Makefile
	$(CC) $(COMPILE_FLAGS) -D__ASM_FILE__ -g -MMD -c $< -o $@

%.o: %.c Makefile
	$(CC) $(CFLAGS) $(COMPILE_FLAGS) -MMD -c $< -o $@

-include $(DEP)

INITRAMFS_FILES := $(wildcard initramfs_source/**)

initramfs.cpio: $(INITRAMFS_FILES)
	(cd initramfs_source; ../make_initramfs.sh . ../initramfs.cpio)

.PHONY: clean clean-full run run-log run-debug
clean:
	rm -f kernel $(OBJ) $(DEP) log.txt initramfs.cpio

clean-full:
	rm -f kernel *.o *.d log.txt initramfs.cpio

run: kernel initramfs.cpio
	$(QEMU) $(RUNFLAGS) -kernel kernel -append 'log_lvl=30 log_clr=1' $(RUN_FLAGS)

run-log: kernel initramfs.cpio
	$(QEMU) $(RUNFLAGS) -kernel kernel -append 'log_lvl=1 log_clr=0' $(RUN_FLAGS) | tee log.txt | grep -vE '^!'

run-debug: kernel initramfs.cpio
	$(QEMU) $(RUNFLAGS) -kernel kernel -append 'log_lvl=10 log_clr=1' -s $(RUN_FLAGS)
